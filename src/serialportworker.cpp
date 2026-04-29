/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: serialportworker.cpp
 * Version: 1.0.0
 * Description: 串口工作线程对象实现
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#include "serialportworker.h"

#include <QByteArray>
#include <QTimer>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace {

// 把 Windows API 返回的错误码转换成可读文本，
// 便于直接显示在状态栏或错误提示中。
/**
 * @brief 将 Windows 错误码转换为可读文本
 *
 * @param errorCode: 系统错误码
 *
 * @return 对应的错误描述文本
 */
QString lastErrorMessage(DWORD errorCode)
{
    wchar_t *buffer = nullptr;
    const DWORD flags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length =
        FormatMessageW(flags, nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    QString message;
    if (length > 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer).trimmed();
        LocalFree(buffer);
    } else {
        message = QStringLiteral("错误码 %1").arg(errorCode);
    }

    return message;
}

// 把界面文本转换成 DCB 所需的校验位枚举值。
/**
 * @brief 将界面校验位文本转换为 Windows DCB 常量
 *
 * @param parityText: 校验位文本
 *
 * @return 对应的校验位常量
 */
BYTE parityFromText(const QString &parityText)
{
    const QString normalized = parityText.trimmed();
    if (normalized.compare(QStringLiteral("Odd"), Qt::CaseInsensitive) == 0) {
        return ODDPARITY;
    }
    if (normalized.compare(QStringLiteral("Even"), Qt::CaseInsensitive) == 0) {
        return EVENPARITY;
    }
    if (normalized.compare(QStringLiteral("Mark"), Qt::CaseInsensitive) == 0) {
        return MARKPARITY;
    }
    if (normalized.compare(QStringLiteral("Space"), Qt::CaseInsensitive) == 0) {
        return SPACEPARITY;
    }
    return NOPARITY;
}

// 把界面文本转换成 Windows API 所需的停止位常量。
/**
 * @brief 将界面停止位文本转换为 Windows API 常量
 *
 * @param stopBitsText: 停止位文本
 *
 * @return 对应的停止位常量
 */
BYTE stopBitsFromText(const QString &stopBitsText)
{
    const QString normalized = stopBitsText.trimmed();
    if (normalized == QStringLiteral("1.5")) {
        return ONE5STOPBITS;
    }
    if (normalized == QStringLiteral("2")) {
        return TWOSTOPBITS;
    }
    return ONESTOPBIT;
}

} // namespace

/**
 * @brief 构造串口工作线程对象
 *
 * @param parent: 父对象指针
 *
 * @return 无
 */
SerialPortWorker::SerialPortWorker(QObject *parent)
    : QObject(parent)
    , portHandle(nullptr)
    , pollTimer(new QTimer(this))
    , currentBaudRate(115200)
    , currentDataBits(8)
    , currentParity(QStringLiteral("None"))
    , currentStopBits(QStringLiteral("1"))
    , rtsEnabled(false)
    , dtrEnabled(false)
    , lastModemStatus(0xffffffffUL)
{
    // 20ms 的轮询周期对串口调试已经足够细，
    // 同时不会让线程因为过于频繁轮询而浪费太多 CPU。
    pollTimer->setInterval(20);
    connect(pollTimer, &QTimer::timeout, this, &SerialPortWorker::pollPort);
}

/**
 * @brief 析构串口工作线程对象
 *
 * @param 无
 *
 * @return 无
 */
SerialPortWorker::~SerialPortWorker()
{
    closePort();
}

/**
 * @brief 按指定参数打开串口
 *
 * @param portName: 端口名称
 * @param baudRate: 波特率
 * @param dataBits: 数据位
 * @param parityText: 校验位文本
 * @param stopBitsText: 停止位文本
 *
 * @return 无
 */
void SerialPortWorker::openPort(const QString &portName,
                                int baudRate,
                                int dataBits,
                                const QString &parityText,
                                const QString &stopBitsText)
{
    // 每次打开新串口前都先关闭旧串口，
    // 防止旧句柄、旧参数和旧缓冲区残留。
    closePort();

    currentPortName = portName.trimmed().toUpper();
    currentBaudRate = baudRate;
    currentDataBits = dataBits;
    currentParity = parityText.trimmed();
    currentStopBits = stopBitsText.trimmed();

    if (currentPortName.isEmpty()) {
        emit errorOccurred(QStringLiteral("请选择有效的串口号。"));
        return;
    }

    // Windows 访问高编号 COM 口时，一般需要使用 \\.\COMx 形式的设备路径。
    const QString nativePortName = currentPortName.startsWith(QStringLiteral("\\\\.\\"))
                                       ? currentPortName
                                       : QStringLiteral("\\\\.\\%1").arg(currentPortName);
    const std::wstring portPath = nativePortName.toStdWString();
    HANDLE handle = CreateFileW(portPath.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                0,
                                nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        emit errorOccurred(QStringLiteral("打开 %1 失败：%2")
                               .arg(currentPortName, lastErrorMessage(GetLastError())));
        return;
    }

    // DCB 结构体定义了串口最核心的参数：
    // 波特率、数据位、校验位、停止位以及各种流控选项。
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(handle, &dcb)) {
        const QString message = QStringLiteral("读取串口参数失败：%1")
                                    .arg(lastErrorMessage(GetLastError()));
        CloseHandle(handle);
        emit errorOccurred(message);
        return;
    }

    dcb.BaudRate = static_cast<DWORD>(currentBaudRate);
    dcb.ByteSize = static_cast<BYTE>(currentDataBits);
    dcb.Parity = parityFromText(currentParity);
    dcb.StopBits = stopBitsFromText(currentStopBits);
    dcb.fBinary = TRUE;
    dcb.fParity = dcb.Parity != NOPARITY;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = dtrEnabled ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = rtsEnabled ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(handle, &dcb)) {
        const QString message = QStringLiteral("设置串口参数失败：%1")
                                    .arg(lastErrorMessage(GetLastError()));
        CloseHandle(handle);
        emit errorOccurred(message);
        return;
    }

    // 超时策略决定读写函数的阻塞行为。
    // 这里采用较保守的配置，优先保证调试工具的交互流畅性。
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 10;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 2;

    if (!SetCommTimeouts(handle, &timeouts)) {
        const QString message = QStringLiteral("设置串口超时失败：%1")
                                    .arg(lastErrorMessage(GetLastError()));
        CloseHandle(handle);
        emit errorOccurred(message);
        return;
    }

    // 打开后先清空收发缓冲区，避免上一次会话残留数据混入当前调试。
    SetupComm(handle, 8192, 8192);
    PurgeComm(handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

    portHandle = handle;
    lastModemStatus = 0xffffffffUL;
    pollTimer->start();
    updateModemStatus();
    emit portOpened(buildSummary(QStringLiteral("已打开")));
}

/**
 * @brief 关闭当前串口
 *
 * @param 无
 *
 * @return 无
 */
void SerialPortWorker::closePort()
{
    // 先生成摘要文本，再释放句柄。
    // 这样即使句柄释放后某些状态不可读，界面仍能拿到完整提示。
    const bool hadOpenPort = portHandle != nullptr;
    const QString summary = buildSummary(QStringLiteral("已关闭"));

    pollTimer->stop();
    closeHandle();
    lastModemStatus = 0xffffffffUL;
    emit modemStatusChanged(false, false, false, false);

    if (hadOpenPort) {
        emit portClosed(summary);
    }
}

/**
 * @brief 向当前串口写入数据
 *
 * @param data: 待发送的字节流
 *
 * @return 无
 */
void SerialPortWorker::writeData(const QByteArray &data)
{
    // 写串口时直接把 QByteArray 当作连续字节流传给 WriteFile。
    if (portHandle == nullptr || data.isEmpty()) {
        return;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(portHandle);
    DWORD writtenBytes = 0;

    if (!WriteFile(handle,
                   data.constData(),
                   static_cast<DWORD>(data.size()),
                   &writtenBytes,
                   nullptr)) {
        emit errorOccurred(QStringLiteral("发送数据失败：%1")
                               .arg(lastErrorMessage(GetLastError())));
        closePort();
        return;
    }

    emit bytesWritten(static_cast<qint64>(writtenBytes));
}

/**
 * @brief 设置 RTS 输出状态
 *
 * @param enabled: `true` 表示置位 RTS
 *
 * @return 无
 */
void SerialPortWorker::setRtsEnabled(bool enabled)
{
    // 先记住目标状态；如果串口已经打开，再立刻同步到底层设备。
    rtsEnabled = enabled;

    if (portHandle == nullptr) {
        return;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(portHandle);
    if (!EscapeCommFunction(handle, enabled ? SETRTS : CLRRTS)) {
        emit errorOccurred(QStringLiteral("设置 RTS 失败：%1")
                               .arg(lastErrorMessage(GetLastError())));
        return;
    }

    updateModemStatus();
}

/**
 * @brief 设置 DTR 输出状态
 *
 * @param enabled: `true` 表示置位 DTR
 *
 * @return 无
 */
void SerialPortWorker::setDtrEnabled(bool enabled)
{
    // DTR 的处理方式与 RTS 一致，先保存目标值，再尝试实时下发。
    dtrEnabled = enabled;

    if (portHandle == nullptr) {
        return;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(portHandle);
    if (!EscapeCommFunction(handle, enabled ? SETDTR : CLRDTR)) {
        emit errorOccurred(QStringLiteral("设置 DTR 失败：%1")
                               .arg(lastErrorMessage(GetLastError())));
        return;
    }

    updateModemStatus();
}

/**
 * @brief 轮询串口接收数据与线路状态
 *
 * @param 无
 *
 * @return 无
 */
void SerialPortWorker::pollPort()
{
    // 轮询读取分两步：
    // 1. 查询输入队列中还有多少字节；
    // 2. 按块读出，再通过信号回传给主线程更新界面。
    if (portHandle == nullptr) {
        return;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(portHandle);
    COMSTAT comStatus = {};
    DWORD errors = 0;

    if (!ClearCommError(handle, &errors, &comStatus)) {
        emit errorOccurred(QStringLiteral("读取串口状态失败：%1")
                               .arg(lastErrorMessage(GetLastError())));
        closePort();
        return;
    }

    DWORD remainingBytes = comStatus.cbInQue;
    while (remainingBytes > 0) {
        // 分块读取可以避免瞬时大流量时一次性申请过大缓冲区。
        const DWORD chunkSize = std::min<DWORD>(remainingBytes, 4096);
        QByteArray buffer(static_cast<int>(chunkSize), Qt::Uninitialized);
        DWORD bytesRead = 0;

        if (!ReadFile(handle, buffer.data(), chunkSize, &bytesRead, nullptr)) {
            emit errorOccurred(QStringLiteral("接收数据失败：%1")
                                   .arg(lastErrorMessage(GetLastError())));
            closePort();
            return;
        }

        if (bytesRead == 0) {
            break;
        }

        buffer.truncate(static_cast<int>(bytesRead));
        emit dataReceived(buffer);

        if (bytesRead >= remainingBytes) {
            break;
        }
        remainingBytes -= bytesRead;
    }

    updateModemStatus();
}

/**
 * @brief 生成当前串口状态摘要
 *
 * @param stateText: 状态描述文本
 *
 * @return 供界面显示的摘要文本
 */
QString SerialPortWorker::buildSummary(const QString &stateText) const
{
    // 统一状态摘要格式，便于主窗口直接显示在状态栏中。
    const QString portText = currentPortName.isEmpty() ? QStringLiteral("未选择串口") : currentPortName;
    return QStringLiteral("%1 %2 %3bps,%4,%5,%6")
        .arg(portText)
        .arg(stateText)
        .arg(currentBaudRate)
        .arg(currentDataBits)
        .arg(currentStopBits)
        .arg(currentParity);
}

/**
 * @brief 读取并更新调制解调器线路状态
 *
 * @param 无
 *
 * @return 无
 */
void SerialPortWorker::updateModemStatus()
{
    // 这里只关心硬件线状态是否发生变化；
    // 如果状态没变，就不重复发信号刷新界面。
    if (portHandle == nullptr) {
        emit modemStatusChanged(false, false, false, false);
        return;
    }

    DWORD modemStatus = 0;
    if (!GetCommModemStatus(reinterpret_cast<HANDLE>(portHandle), &modemStatus)) {
        return;
    }

    if (modemStatus == lastModemStatus) {
        return;
    }

    lastModemStatus = modemStatus;
    emit modemStatusChanged((modemStatus & MS_CTS_ON) != 0,
                            (modemStatus & MS_DSR_ON) != 0,
                            (modemStatus & MS_RLSD_ON) != 0,
                            (modemStatus & MS_RING_ON) != 0);
}

/**
 * @brief 释放底层串口句柄
 *
 * @param 无
 *
 * @return 无
 */
void SerialPortWorker::closeHandle()
{
    // 只做最底层的句柄释放，方便在多个关闭路径中复用。
    if (portHandle == nullptr) {
        return;
    }

    CloseHandle(reinterpret_cast<HANDLE>(portHandle));
    portHandle = nullptr;
}
