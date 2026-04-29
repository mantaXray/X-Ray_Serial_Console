/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: mainwindow.cpp
 * Version: 1.00.01
 * Description: X-Ray Serial Console main window implementation
 *
 * Author: mantaXray
 * Date: 2026年04月29日
 */

#include "appversion.h"
#include "appsettings.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multistringdock.h"
#include "serialportworker.h"
#include "toolboxdialog.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <devguid.h>
#include <setupapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr qint64 kPreviewByteLimit = 64 * 1024;
constexpr qint64 kLargeFileThreshold = 256 * 1024;
constexpr qint64 kLiveViewLimit = 1024 * 1024;
constexpr int kChecksumNone = 0;
constexpr int kChecksumModbusCrc16 = 1;
constexpr int kChecksumCcittCrc16 = 2;
constexpr int kChecksumCrc32 = 3;
constexpr int kChecksumAdd8 = 4;
constexpr int kChecksumXor8 = 5;
constexpr int kChecksumAdd16 = 6;
constexpr int kModbusFormatUnsigned = 0;
constexpr int kModbusFormatSigned = 1;
constexpr int kModbusFormatHex = 2;
constexpr int kModbusFormatBinary = 3;
constexpr int kModbusFormatFloat32 = 4;
constexpr int kModbusFormatFloat32WordSwap = 5;
constexpr int kModbusFormatUseDefault = -1;
constexpr int kModbusAddressColumn = 0;
constexpr int kModbusRawColumn = 1;
constexpr int kModbusValueColumn = 2;
constexpr int kModbusWriteColumn = 3;
constexpr int kModbusUpdatedColumn = 4;
constexpr int kModbusStatusColumn = 5;
constexpr int kModbusTableRowHeight = 24;
constexpr int kModbusTableMinimumRowHeight = 22;
constexpr int kModbusTableMinimumHeight = 280;
constexpr int kModbusMonitorSplitterLayoutVersion = 2;
constexpr int kModbusMonitorGeometryLayoutVersion = 2;
constexpr auto kCustomBaudRateToken = "__custom_baud__";

/**
 * @brief 设置输入控件的紧凑宽度范围
 *
 * @param widget: 待调整控件
 * @param minimumWidth: 最小宽度
 * @param maximumWidth: 最大宽度
 *
 * @return 无
 */
void setCompactInputWidth(QWidget *widget, int minimumWidth, int maximumWidth)
{
    if (widget == nullptr) {
        return;
    }

    widget->setMinimumWidth(minimumWidth);
    widget->setMaximumWidth(maximumWidth);
}

/**
 * @brief 配置 Modbus 表格列宽策略，保留用户拖拽后的列宽。
 *
 * @param table: 需要配置的寄存器表格
 *
 * @return 无
 */
void configureModbusMonitorTableColumns(QTableWidget *table)
{
    if (table == nullptr) {
        return;
    }

    QHeaderView *header = table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionsMovable(false);
    header->setMinimumSectionSize(64);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);

    if (table->property("modbusColumnWidthsInitialized").toBool()) {
        return;
    }

    // 默认列宽按窗口宽度铺满一次，之后刷新表格内容时保留用户拖拽调整后的比例。
    constexpr int columnCount = 6;
    const int weights[columnCount] = {90, 110, 180, 110, 150, 140};
    const int minimums[columnCount] = {72, 84, 120, 84, 112, 96};
    const int weightSum = 780;
    const int minimumTotal = 568;
    const int availableWidth = qMax(qMax(table->viewport()->width(), table->width()), minimumTotal);
    int usedWidth = 0;
    for (int column = 0; column < columnCount - 1; ++column) {
        const int width = qMax(minimums[column], availableWidth * weights[column] / weightSum);
        table->setColumnWidth(column, width);
        usedWidth += width;
    }
    table->setColumnWidth(columnCount - 1, qMax(minimums[columnCount - 1], availableWidth - usedWidth));
    table->setProperty("modbusColumnWidthsInitialized", true);
}

int modbusFormatRegisterSpan(int format)
{
    return (format == kModbusFormatFloat32 || format == kModbusFormatFloat32WordSwap) ? 2 : 1;
}

bool isModbusMonitorDisplayFormat(int format)
{
    switch (format) {
    case kModbusFormatUnsigned:
    case kModbusFormatSigned:
    case kModbusFormatHex:
    case kModbusFormatBinary:
    case kModbusFormatFloat32:
    case kModbusFormatFloat32WordSwap:
        return true;
    default:
        return false;
    }
}

QFont defaultConsoleFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font.family().trimmed().isEmpty()) {
        font.setFamily(QStringLiteral("Consolas"));
    }
    font.setPointSize(13);
    return font;
}

QString formattedByteCount(qint64 bytes)
{
    const QStringList suffixes = {
        QStringLiteral("B"),
        QStringLiteral("KB"),
        QStringLiteral("MB"),
        QStringLiteral("GB")
    };

    double value = static_cast<double>(bytes);
    int suffixIndex = 0;
    while (value >= 1024.0 && suffixIndex < suffixes.size() - 1) {
        value /= 1024.0;
        ++suffixIndex;
    }

    return suffixIndex == 0 ? QStringLiteral("%1 %2").arg(bytes).arg(suffixes.at(suffixIndex))
                            : QStringLiteral("%1 %2").arg(QString::number(value, 'f', value >= 10.0 ? 1 : 2)).arg(suffixes.at(suffixIndex));
}

QString extractPortName(const QString &text)
{
    static const QRegularExpression portPattern(QStringLiteral("\\b(COM\\d+)\\b"),
                                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = portPattern.match(text.trimmed());
    if (match.hasMatch()) {
        return match.captured(1).trimmed().toUpper();
    }

    return text.trimmed().toUpper();
}

QString queryDevicePropertyText(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData, DWORD property)
{
    wchar_t buffer[512] = {};
    DWORD dataType = 0;
    if (!SetupDiGetDeviceRegistryPropertyW(deviceInfoSet,
                                           deviceInfoData,
                                           property,
                                           &dataType,
                                           reinterpret_cast<PBYTE>(buffer),
                                           sizeof(buffer),
                                           nullptr)) {
        return {};
    }

    return QString::fromWCharArray(buffer).trimmed();
}

QHash<QString, QString> queryFriendlyPortNames()
{
    QHash<QString, QString> friendlyNames;
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return friendlyNames;
    }

    for (DWORD deviceIndex = 0;; ++deviceIndex) {
        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(deviceInfoData);
        if (!SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
            break;
        }

        HKEY deviceRegistryKey = SetupDiOpenDevRegKey(deviceInfoSet,
                                                      &deviceInfoData,
                                                      DICS_FLAG_GLOBAL,
                                                      0,
                                                      DIREG_DEV,
                                                      KEY_QUERY_VALUE);
        if (deviceRegistryKey == INVALID_HANDLE_VALUE) {
            continue;
        }

        wchar_t portBuffer[256] = {};
        DWORD valueType = 0;
        DWORD valueSize = sizeof(portBuffer);
        const LSTATUS queryResult = RegQueryValueExW(deviceRegistryKey,
                                                     L"PortName",
                                                     nullptr,
                                                     &valueType,
                                                     reinterpret_cast<LPBYTE>(portBuffer),
                                                     &valueSize);
        RegCloseKey(deviceRegistryKey);
        if (queryResult != ERROR_SUCCESS || (valueType != REG_SZ && valueType != REG_EXPAND_SZ)) {
            continue;
        }

        const QString portName = extractPortName(QString::fromWCharArray(portBuffer));
        if (!portName.startsWith(QStringLiteral("COM"))) {
            continue;
        }

        QString friendlyName = queryDevicePropertyText(deviceInfoSet, &deviceInfoData, SPDRP_FRIENDLYNAME);
        if (friendlyName.isEmpty()) {
            friendlyName = queryDevicePropertyText(deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC);
        }
        if (!friendlyName.isEmpty()) {
            friendlyNames.insert(portName, friendlyName);
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return friendlyNames;
}

QString buildPortDisplayText(const QString &portName, const QString &friendlyName)
{
    const QString normalizedPortName = extractPortName(portName);
    const QString trimmedFriendlyName = friendlyName.trimmed();
    if (normalizedPortName.isEmpty()) {
        return trimmedFriendlyName;
    }
    if (trimmedFriendlyName.isEmpty()) {
        return normalizedPortName;
    }

    QString normalizedFriendlyName = trimmedFriendlyName;
    const QString escapedPortName = QRegularExpression::escape(normalizedPortName);

    // 友好名称里经常自带“(COM13)”这一段，这里去掉重复端口号，统一显示成“COM13 设备名”。
    normalizedFriendlyName.remove(QRegularExpression(QStringLiteral("\\(\\s*%1\\s*\\)").arg(escapedPortName),
                                                     QRegularExpression::CaseInsensitiveOption));
    normalizedFriendlyName.remove(QRegularExpression(QStringLiteral("\\[\\s*%1\\s*\\]").arg(escapedPortName),
                                                     QRegularExpression::CaseInsensitiveOption));
    normalizedFriendlyName.remove(QRegularExpression(QStringLiteral("（\\s*%1\\s*）").arg(escapedPortName),
                                                     QRegularExpression::CaseInsensitiveOption));
    normalizedFriendlyName.remove(QRegularExpression(QStringLiteral("\\b%1\\b").arg(escapedPortName),
                                                     QRegularExpression::CaseInsensitiveOption));
    normalizedFriendlyName.remove(QRegularExpression(QStringLiteral("^[\\s\\-:：,，]+|[\\s\\-:：,，]+$")));
    normalizedFriendlyName.replace(QRegularExpression(QStringLiteral("\\s{2,}")), QStringLiteral(" "));
    normalizedFriendlyName = normalizedFriendlyName.trimmed();

    if (normalizedFriendlyName.isEmpty()) {
        return normalizedPortName;
    }

    return QStringLiteral("%1 %2").arg(normalizedPortName, normalizedFriendlyName);
}

QString currentPortName(const QComboBox *comboBox)
{
    if (comboBox == nullptr) {
        return {};
    }

    const int currentIndex = comboBox->currentIndex();
    if (currentIndex >= 0) {
        const QString storedPortName = extractPortName(comboBox->currentData().toString());
        if (!storedPortName.isEmpty()) {
            return storedPortName;
        }
    }

    return extractPortName(comboBox->currentText());
}

QString currentPortDisplayText(const QComboBox *comboBox)
{
    if (comboBox == nullptr) {
        return {};
    }

    const QString displayText = comboBox->currentText().trimmed();
    if (!displayText.isEmpty()) {
        return displayText;
    }

    return currentPortName(comboBox);
}

// QueryDosDevice 能直接查询系统当前可见的 COM 设备名。
// 它可以补足某些驱动没有完整写注册表时的端口枚举结果。
QStringList queryDosDevicePorts()
{
    QStringList ports;
    wchar_t targetPath[4096] = {};

    for (int index = 1; index <= 256; ++index) {
        const QString portName = QStringLiteral("COM%1").arg(index);
        const std::wstring nativeName = portName.toStdWString();
        const DWORD length = QueryDosDeviceW(nativeName.c_str(),
                                             targetPath,
                                             static_cast<DWORD>(sizeof(targetPath) / sizeof(targetPath[0])));
        if (length != 0) {
            ports << portName;
        }
    }

    return ports;
}

/**
 * @brief 计算 Modbus CRC16
 *
 * @param data: 参与校验的字节数组
 *
 * @return 低字节在前的 Modbus CRC16 数值
 */
quint16 modbusCrc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const auto byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x0001) != 0) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 按 Modbus 大端字节序追加 16 位值
 *
 * @param buffer: 目标字节缓冲区
 * @param value: 需要追加的 16 位数值
 *
 * @return 无
 */
void appendBigEndian16(QByteArray &buffer, quint16 value)
{
    // Modbus RTU 数据区的寄存器地址和数量均为高字节在前。
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

/**
 * @brief 从 Modbus 大端字节序读取 16 位值
 *
 * @param buffer: 源字节缓冲区
 * @param offset: 读取起始偏移
 *
 * @return 对应的 16 位数值
 */
quint16 readBigEndian16(const QByteArray &buffer, int offset)
{
    // 响应数据中的每个寄存器占 2 字节，高字节在 offset 位置。
    return static_cast<quint16>((static_cast<quint8>(buffer.at(offset)) << 8)
                                | static_cast<quint8>(buffer.at(offset + 1)));
}

/**
 * @brief 格式化单字节十六进制文本
 *
 * @param value: 单字节数值
 *
 * @return 形如 0x01 的文本
 */
QString formatByteHex(quint8 value)
{
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
}

/**
 * @brief 格式化 16 位寄存器十六进制文本
 *
 * @param value: 16 位寄存器值
 *
 * @return 形如 0x0001 的文本
 */
QString formatWordHex(quint16 value)
{
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QString formatWordBinary(quint16 value)
{
    return QStringLiteral("0b%1").arg(value, 16, 2, QLatin1Char('0'));
}

bool isModbusReadFunction(int functionCode)
{
    return functionCode == 0x03 || functionCode == 0x04;
}

bool isModbusWriteFunction(int functionCode)
{
    return functionCode == 0x06 || functionCode == 0x10;
}

/**
 * @brief 将相邻两个寄存器按大端字序解释为 Float32 文本
 *
 * @param highWord: 高 16 位寄存器
 * @param lowWord: 低 16 位寄存器
 *
 * @return 可显示的浮点文本，异常值返回空文本
 */
QString registerFloatText(quint16 highWord, quint16 lowWord)
{
    // 默认采用常见的 AB CD 字序，后续可再扩展 CD AB / BA DC 等字序选项。
    const quint32 raw = (static_cast<quint32>(highWord) << 16) | lowWord;
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(raw));
    // 用 memcpy 避免违反严格别名规则。
    std::memcpy(&value, &raw, sizeof(value));
    return std::isfinite(value) ? QString::number(value, 'g', 8) : QString();
}

/**
 * @brief 计算 CCITT CRC16
 *
 * @param data: 参与校验的字节数组
 *
 * @return 高字节在前的 CRC16-CCITT 数值
 */
quint16 crc16Ccitt(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const auto byte : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(byte) << 8);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0 ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                      : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

/**
 * @brief 计算 CRC32
 *
 * @param data: 参与校验的字节数组
 *
 * @return 标准 CRC32 数值
 */
quint32 crc32Value(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (const auto byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) != 0 ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/**
 * @brief 根据校验模式生成需要追加的校验字节
 *
 * @param payload: 原始负载数据
 * @param checksumMode: 校验模式编号
 *
 * @return 需要附加到发送末尾的校验字节
 */
QByteArray checksumSuffix(const QByteArray &payload, int checksumMode)
{
    QByteArray suffix;

    switch (checksumMode) {
    case kChecksumModbusCrc16: {
        const quint16 crc = modbusCrc16(payload);
        suffix.append(static_cast<char>(crc & 0xFF));
        suffix.append(static_cast<char>((crc >> 8) & 0xFF));
        break;
    }
    case kChecksumCcittCrc16: {
        const quint16 crc = crc16Ccitt(payload);
        suffix.append(static_cast<char>((crc >> 8) & 0xFF));
        suffix.append(static_cast<char>(crc & 0xFF));
        break;
    }
    case kChecksumCrc32: {
        const quint32 crc = crc32Value(payload);
        suffix.append(static_cast<char>((crc >> 24) & 0xFF));
        suffix.append(static_cast<char>((crc >> 16) & 0xFF));
        suffix.append(static_cast<char>((crc >> 8) & 0xFF));
        suffix.append(static_cast<char>(crc & 0xFF));
        break;
    }
    case kChecksumAdd8: {
        quint8 value = 0;
        for (const auto byte : payload) {
            value = static_cast<quint8>(value + static_cast<quint8>(byte));
        }
        suffix.append(static_cast<char>(value));
        break;
    }
    case kChecksumXor8: {
        quint8 value = 0;
        for (const auto byte : payload) {
            value ^= static_cast<quint8>(byte);
        }
        suffix.append(static_cast<char>(value));
        break;
    }
    case kChecksumAdd16: {
        quint16 value = 0;
        for (const auto byte : payload) {
            value = static_cast<quint16>(value + static_cast<quint8>(byte));
        }
        suffix.append(static_cast<char>((value >> 8) & 0xFF));
        suffix.append(static_cast<char>(value & 0xFF));
        break;
    }
    default:
        break;
    }

    return suffix;
}

} // namespace

/**
 * @brief 构造主窗口并初始化串口调试台
 *
 * @param parent: 父对象指针
 *
 * @return 无
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialWorker(new SerialPortWorker)
    , serialThread(new QThread(this))
    , autoSendTimer(new QTimer(this))
    , receivePacketTimer(new QTimer(this))
    , modbusMonitorTimer(new QTimer(this))
    , modbusMonitorPacketTimer(new QTimer(this))
    , modbusMonitorResponseTimer(new QTimer(this))
    , receiveEdit(nullptr)
    , sendEdit(nullptr)
    , portComboBox(nullptr)
    , baudRateComboBox(nullptr)
    , dataBitsComboBox(nullptr)
    , parityComboBox(nullptr)
    , stopBitsComboBox(nullptr)
    , hexDisplayCheckBox(nullptr)
    , timestampCheckBox(nullptr)
    , wrapCheckBox(nullptr)
    , pauseDisplayCheckBox(nullptr)
    , hexSendCheckBox(nullptr)
    , appendNewLineCheckBox(nullptr)
    , autoSendCheckBox(nullptr)
    , rtsCheckBox(nullptr)
    , dtrCheckBox(nullptr)
    , autoSendIntervalSpinBox(nullptr)
    , refreshPortsButton(nullptr)
    , openPortButton(nullptr)
    , clearReceiveButton(nullptr)
    , saveReceiveButton(nullptr)
    , loadSendFileButton(nullptr)
    , clearSendButton(nullptr)
    , sendButton(nullptr)
    , receiveSectionLabel(nullptr)
    , sendSectionLabel(nullptr)
    , portLabel(nullptr)
    , baudRateLabel(nullptr)
    , dataBitsLabel(nullptr)
    , parityLabel(nullptr)
    , stopBitsLabel(nullptr)
    , intervalLabel(nullptr)
    , receiveTimeoutUnitLabel(nullptr)
    , autoSendIntervalUnitLabel(nullptr)
    , brandStatusLabel(nullptr)
    , sendCountLabel(nullptr)
    , receiveCountLabel(nullptr)
    , portStateLabel(nullptr)
    , lineStateLabel(nullptr)
    , connectionMenu(nullptr)
    , configMenu(nullptr)
    , viewMenu(nullptr)
    , sendMenu(nullptr)
    , presetMenu(nullptr)
    , toolsMenu(nullptr)
    , protocolToolboxMenu(nullptr)
    , languageMenu(nullptr)
    , helpMenu(nullptr)
    , refreshAction(nullptr)
    , togglePortAction(nullptr)
    , quitAction(nullptr)
    , resetCounterAction(nullptr)
    , copySummaryAction(nullptr)
    , clearReceiveAction(nullptr)
    , saveReceiveAction(nullptr)
    , hexDisplayAction(nullptr)
    , timestampAction(nullptr)
    , wrapAction(nullptr)
    , sendNowAction(nullptr)
    , loadFileAction(nullptr)
    , hexSendAction(nullptr)
    , autoSendAction(nullptr)
    , heartbeatAction(nullptr)
    , modbusAction(nullptr)
    , clearSendAction(nullptr)
    , pauseAction(nullptr)
    , languageChineseAction(nullptr)
    , languageEnglishAction(nullptr)
    , guideAction(nullptr)
    , aboutAction(nullptr)
    , currentLanguage(UiLanguage::Chinese)
    , portOpen(false)
    , totalSendBytes(0)
    , totalReceiveBytes(0)
{
    ui->setupUi(this);

    // 串口工作对象常驻独立线程中，避免串口操作阻塞主界面。
    serialWorker->moveToThread(serialThread);
    connect(serialThread, &QThread::finished, serialWorker, &QObject::deleteLater);
    serialThread->start();

    autoSendTimer->setSingleShot(false);
    receivePacketTimer->setSingleShot(true);
    modbusMonitorTimer->setSingleShot(false);
    modbusMonitorPacketTimer->setSingleShot(true);
    modbusMonitorResponseTimer->setSingleShot(true);
    connect(modbusMonitorResponseTimer, &QTimer::timeout, this, [this] {
        cancelModbusMonitorReceiveSession(false);
        updateModbusMonitorState();
    });

    buildUi();
    buildMenus();
    buildStatusBar();
    connectUi();
    loadDisplaySettings();

    // 主线程只发请求，真正的串口 I/O 在工作线程内执行。
    connect(this,
            &MainWindow::requestOpenPort,
            serialWorker,
            &SerialPortWorker::openPort,
            Qt::QueuedConnection);
    connect(this,
            &MainWindow::requestClosePort,
            serialWorker,
            &SerialPortWorker::closePort,
            Qt::QueuedConnection);
    connect(this,
            &MainWindow::requestWriteData,
            serialWorker,
            &SerialPortWorker::writeData,
            Qt::QueuedConnection);
    connect(this,
            &MainWindow::requestSetRtsEnabled,
            serialWorker,
            &SerialPortWorker::setRtsEnabled,
            Qt::QueuedConnection);
    connect(this,
            &MainWindow::requestSetDtrEnabled,
            serialWorker,
            &SerialPortWorker::setDtrEnabled,
            Qt::QueuedConnection);

    // 串口连接状态变化后，统一刷新按钮和状态栏文本。
    connect(serialWorker, &SerialPortWorker::portOpened, this, [this](const QString &) {
        portOpen = true;
        updateUiState();
        updatePortSummary();
        updateAutoSendTimer();
        statusBar()->showMessage(uiText(QStringLiteral("串口已连接"),
                                        QStringLiteral("Serial port connected")),
                                 3000);
    });
    connect(serialWorker, &SerialPortWorker::portClosed, this, [this](const QString &) {
        portOpen = false;
        lineStateLabel->setText(QStringLiteral("CTS=0  DSR=0  RLSD=0"));
        updateUiState();
        updatePortSummary();
        updateAutoSendTimer();
        statusBar()->showMessage(uiText(QStringLiteral("串口已断开"),
                                        QStringLiteral("Serial port disconnected")),
                                 3000);
    });

    // 这里直接展示工作线程回传的错误文本。
    // 目前底层串口层仍以中文错误前缀为主，后续如有需要可以继续抽象成错误码。
    connect(serialWorker, &SerialPortWorker::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });

    // 接收数据后按当前显示模式渲染到接收区。
    // 开启“超时分包显示”时先缓存数据，等超时后再一次性刷新，
    // 这样可以把同一帧里分多次到达的字节聚合起来，减少时间戳断包。
    connect(serialWorker, &SerialPortWorker::dataReceived, this, [this](const QByteArray &data) {
        totalReceiveBytes += data.size();
        updateCounters();
        handleModbusMonitorBytes(data);

        if (pauseDisplayCheckBox->isChecked()) {
            return;
        }

        if (packetSplitCheckBox->isChecked()) {
            pendingReceivePacket.append(data);
            receivePacketTimer->start(receiveTimeoutSpinBox->value());
            return;
        }

        appendTrafficLog(data, false, hexDisplayCheckBox->isChecked());
    });

    connect(serialWorker, &SerialPortWorker::bytesWritten, this, [this](qint64 count) {
        totalSendBytes += count;
        updateCounters();
    });

    connect(serialWorker,
            &SerialPortWorker::modemStatusChanged,
            this,
            [this](bool cts, bool dsr, bool rlsd, bool) {
                lineStateLabel->setText(
                    QStringLiteral("CTS=%1  DSR=%2  RLSD=%3")
                        .arg(cts ? 1 : 0)
                        .arg(dsr ? 1 : 0)
                        .arg(rlsd ? 1 : 0));
            });

    connect(autoSendTimer, &QTimer::timeout, this, [this] { sendCurrentPayload(true); });
    connect(receivePacketTimer, &QTimer::timeout, this, &MainWindow::flushPendingReceivePacket);

    applyLanguage(UiLanguage::Chinese);
    refreshPortList();
    updateUiState();
    updateWrapMode();
    updateCounters();
    updatePortSummary();
    statusBar()->showMessage(uiText(QStringLiteral("X-Ray 串口调试台已就绪"),
                                    QStringLiteral("X-Ray Serial Console is ready")),
                             2500);
}

/**
 * @brief 析构主窗口并安全关闭工作线程
 *
 * @param 无
 *
 * @return 无
 */
MainWindow::~MainWindow()
{
    autoSendTimer->stop();
    stopLiveCapture();

    // 析构前在工作线程中关闭串口，确保底层句柄安全释放。
    if (serialThread->isRunning()) {
        QMetaObject::invokeMethod(serialWorker, "closePort", Qt::BlockingQueuedConnection);
        serialThread->quit();
        serialThread->wait(2000);
    }

    delete ui;
}

/**
 * @brief 处理主窗口关闭事件
 *
 * @param event: 关闭事件对象
 *
 * @return 无
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    autoSendTimer->stop();
    // 关闭程序是接收监视的最终边界，先把等待超时聚合的数据落到显示/导出缓冲。
    flushPendingReceivePacket();
    saveDisplaySettings();

    if (serialThread->isRunning()) {
        QMetaObject::invokeMethod(serialWorker, "closePort", Qt::BlockingQueuedConnection);
    }

    QMainWindow::closeEvent(event);
}

/**
 * @brief 构建主窗口界面布局
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::buildUi()
{
    // buildUi() 负责搭建主窗口里用户能直接操作和看到的全部界面。
    // 这个函数不只是创建控件，还集中定义了整套 QSS 视觉风格，
    // 让菜单、编辑器、输入框、按钮和状态栏保持统一外观。
    //
    // 当前界面层次：
    // - receiveEdit：主接收监视区
    // - controlPanel：位于下方的高密度操作区
    //   - 接收工具行
    //   - 多字符串调度状态行
    //   - 超时分包设置行
    //   - 串口连接行
    //   - 串口参数网格
    //   - 发送选项行
    //   - 校验附加行
    //   - 导入文件提示卡
    //   - 发送编辑区 + 发送按钮行
    //
    // 视觉方向：
    // - 蓝白浅色主题
    // - 信息密度较高
    // - 交互控件强调高对比度
    // - 底部状态栏采用经典分段式样式，便于快速扫读设备状态
    setWindowTitle(windowTitleText());
    resize(1280, 800);
    setMinimumSize(1120, 720);
    setStyleSheet(QStringLiteral(R"(
        /* 主窗口最外层背景色。 */
        QMainWindow {
            background: #f3f7fc;
        }
        /* 通用控件默认文字颜色和界面字体。 */
        QWidget {
            color: #16324f;
        }
        /* 顶部菜单栏外观。 */
        QMenuBar {
            background: #f8fbff;
            color: #16324f;
            border-bottom: 1px solid #d7e3f0;
        }
        QMenuBar::item {
            background: transparent;
            color: #16324f;
            padding: 6px 10px;
        }
        QMenuBar::item:selected {
            background: #e7f0ff;
            color: #123052;
        }
        /* 下拉菜单面板。 */
        QMenu {
            background: #ffffff;
            color: #16324f;
            border: 1px solid #d7e3f0;
        }
        QMenu::item:selected {
            background: #e7f0ff;
            color: #123052;
        }
        /* 主接收区和发送区文本编辑器。 */
        QPlainTextEdit#receiveView, QPlainTextEdit#sendView {
            background: #ffffff;
            border: 1px solid #bfd2e6;
            color: #112c49;
            selection-background-color: #d9e9ff;
            selection-color: #102742;
        }
        /* 接收监视区与底部操作面板。 */
        QFrame#monitorPanel {
            background: #ffffff;
            border: 1px solid #d7e3f0;
            border-radius: 6px;
        }
        QFrame#controlPanel {
            background: #f8fbff;
            border-top: 1px solid #d7e3f0;
        }
        /* 底部按功能拆成独立卡片，用浅边框把信息块收住。 */
        QFrame#configCard {
            background: #ffffff;
            border: 1px solid #d7e3f0;
            border-radius: 6px;
        }
        /* 普通标签和强调用的小节标题。 */
        QLabel {
            color: #16324f;
        }
        QLabel#sectionPrimary, QLabel#sectionSecondary, QLabel[sectionRole="secondary"] {
            color: #2563eb;
            font-weight: 700;
        }
        QLabel#presetStatusChip {
            color: #5c728b;
            background: #eef4fb;
            border: 1px solid #d7e3f0;
            border-radius: 10px;
            padding: 2px 8px;
        }
        QCheckBox {
            color: #16324f;
            spacing: 5px;
        }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid #7d95af;
            border-radius: 3px;
            background: #ffffff;
        }
        QCheckBox::indicator:hover {
            border-color: #2563eb;
            background: #f3f8ff;
        }
        QCheckBox::indicator:checked {
            border-color: #1e4fb9;
            background: #2563eb;
            image: url(:/icons/checkmark.svg);
        }
        QCheckBox::indicator:disabled {
            border-color: #cbd8e5;
            background: #f1f6fb;
        }
        QCheckBox::indicator:checked:disabled {
            border-color: #b9d0f5;
            background: #b9d0f5;
            image: url(:/icons/checkmark.svg);
        }
        QCheckBox:disabled,
        QLabel:disabled {
            color: #8aa0b7;
        }
        /* 下拉框和数字输入框使用统一的浅色输入外壳。 */
        QComboBox {
            min-height: 24px;
            padding: 0 30px 0 8px;
            color: #16324f;
            background: #ffffff;
            border: 1px solid #bfd2e6;
            border-radius: 3px;
        }
        QSpinBox {
            min-height: 24px;
            padding: 0 28px 0 8px;
            color: #16324f;
            background: #ffffff;
            border: 1px solid #bfd2e6;
            border-radius: 3px;
        }
        /* 输入控件的悬停、聚焦和禁用态。 */
        QComboBox:hover,
        QSpinBox:hover {
            border-color: #9dbce0;
        }
        QComboBox:focus,
        QSpinBox:focus {
            border-color: #2563eb;
        }
        QComboBox:disabled, QSpinBox:disabled {
            color: #8aa0b7;
            background: #f1f6fb;
        }
        QComboBox QLineEdit,
        QSpinBox QLineEdit {
            border: none;
            padding: 0;
            margin: 0;
            color: #16324f;
            background: transparent;
            selection-background-color: #d9e9ff;
            selection-color: #102742;
        }
        /* 数字输入框上下按钮区域和自定义箭头图标。 */
        QSpinBox::up-button,
        QSpinBox::down-button {
            subcontrol-origin: border;
            width: 22px;
            border-left: 1px solid #cfdff0;
            background: #edf4ff;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-bottom: 1px solid #d8e5f2;
            border-top-right-radius: 3px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 3px;
        }
        QSpinBox::up-button:hover,
        QSpinBox::down-button:hover {
            background: #e2efff;
        }
        QSpinBox::up-button:pressed,
        QSpinBox::down-button:pressed {
            background: #d5e7ff;
        }
        QSpinBox::up-button:disabled,
        QSpinBox::down-button:disabled {
            background: #f1f6fb;
            border-left-color: #d7e3f0;
        }
        QSpinBox::up-arrow {
            image: url(:/icons/chevron-up.svg);
            width: 10px;
            height: 6px;
        }
        QSpinBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 10px;
            height: 6px;
        }
        /* 下拉按钮区域和自定义箭头图标。 */
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #cfdff0;
            background: #edf4ff;
            border-top-right-radius: 3px;
            border-bottom-right-radius: 3px;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 12px;
            height: 8px;
        }
        /* 普通按钮基础样式。 */
        QPushButton {
            min-height: 26px;
            padding: 0 10px;
            color: #16324f;
            background: #ffffff;
            border: 1px solid #bfd2e6;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: #eef5ff;
        }
        QPushButton:pressed {
            background: #ddeaff;
        }
        QPushButton:disabled {
            color: #8aa0b7;
            background: #f1f6fb;
            border-color: #d7e3f0;
        }
        /* 主发送按钮：用更强的蓝色强调主操作。 */
        QPushButton#sendButton {
            color: #ffffff;
            background: #2563eb;
            border: 1px solid #1e4fb9;
            font-weight: 700;
            min-width: 110px;
        }
        QPushButton#sendButton:hover {
            background: #1f5bdd;
        }
        QPushButton#sendButton:pressed {
            background: #194fbe;
        }
        /* 导入文件后的提示卡，用来说明当前文件的处理方式。 */
        QFrame#importNoticeCard {
            background: #eef6ff;
            border: 1px solid #bfd6f2;
            border-left: 4px solid #2563eb;
            border-radius: 6px;
        }
        /* 提示卡内部的标题、细节和说明文字层级。 */
        QLabel#importNoticeTitle {
            color: #16324f;
            font-size: 13px;
            font-weight: 700;
            background: transparent;
        }
        QLabel#importNoticeDetail {
            color: #21496f;
            font-size: 12px;
            background: transparent;
        }
        QLabel#importNoticeHint {
            color: #2b5c8c;
            font-size: 12px;
            background: transparent;
        }
        /* 提示卡右侧的操作按钮。 */
        QPushButton#importNoticeAction {
            min-height: 28px;
            padding: 0 12px;
            color: #ffffff;
            background: #2563eb;
            border: 1px solid #1e4fb9;
            border-radius: 3px;
            font-weight: 700;
        }
        QPushButton#importNoticeAction:hover {
            background: #1f5bdd;
        }
        QPushButton#importNoticeAction:pressed {
            background: #194fbe;
        }
        QPushButton#importNoticeClose {
            min-height: 28px;
            padding: 0 12px;
        }
        /* 分段式经典状态栏。 */
        QStatusBar {
            background: #ececec;
            color: #111111;
            border-top: 1px solid #a9a9a9;
            padding: 0px;
        }
        QStatusBar::item {
            border: none;
        }
        QStatusBar QLabel[statusRole="segment"] {
            color: #111111;
            background: #f6f6f6;
            border: 1px solid #a6a6a6;
            padding: 1px 6px;
            margin: 0px;
        }
        /* 消息框延续主窗口的整体配色。 */
        QMessageBox {
            background: #f8fbff;
        }
        QMessageBox QLabel {
            color: #16324f;
            font-size: 13px;
            min-width: 420px;
            background: transparent;
        }
    )"));

    // 主界面基准字体改用 QWidget 继承链分发，避免 QSS 固定字号压过串口收发区的动态 setFont。
    QFont uiFont(QStringLiteral("Microsoft YaHei UI"));
    uiFont.setPointSize(13);
    setFont(uiFont);

    // Qt 里的组合框下拉列表通常是独立弹层，不一定完整继承主窗口样式。
    // 这里单独补一套样式，避免它退回平台原生外观，破坏整体一致性。
    const QString comboPopupStyle = QStringLiteral(R"(
        QListView {
            background: #ffffff;
            color: #16324f;
            border: 1px solid #bfd2e6;
            outline: 0;
            padding: 4px 0;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }
        QListView::item {
            min-height: 24px;
            padding: 2px 10px;
            background: #ffffff;
            color: #16324f;
        }
        QListView::item:hover {
            background: #eaf2ff;
            color: #123052;
        }
        QListView::item:selected {
            background: #2563eb;
            color: #ffffff;
        }
        QScrollBar:vertical {
            background: #f1f6fb;
            width: 10px;
            margin: 4px 2px 4px 0;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #b7ccec;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
            border: none;
        }
    )");
    // 给每个组合框统一挂上下拉列表样式，并规范滚动行为。
    const auto polishComboBox = [&comboPopupStyle](QComboBox *comboBox) {
        auto *listView = new QListView(comboBox);
        listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        listView->setUniformItemSizes(true);
        listView->setStyleSheet(comboPopupStyle);
        comboBox->setView(listView);
        comboBox->setMaxVisibleItems(18);
    };

    // 根布局：上方只放接收监视，下方再拆成“串口连接”和“发送面板”。
    auto *rootLayout = new QVBoxLayout(ui->centralwidget);
    rootLayout->setContentsMargins(2, 2, 2, 2);
    rootLayout->setSpacing(2);

    auto *receivePanelFrame = new QFrame(ui->centralwidget);
    receivePanelFrame->setObjectName(QStringLiteral("monitorPanel"));
    auto *receivePanelLayout = new QVBoxLayout(receivePanelFrame);
    receivePanelLayout->setContentsMargins(8, 8, 8, 8);
    receivePanelLayout->setSpacing(6);

    // 接收相关操作贴在接收监视区顶部，避免混到发送控制里。
    auto *receiveHeaderLayout = new QHBoxLayout;
    receiveHeaderLayout->setContentsMargins(0, 0, 0, 0);
    receiveHeaderLayout->setSpacing(6);
    receiveSectionLabel = new QLabel(receivePanelFrame);
    receiveSectionLabel->setObjectName(QStringLiteral("sectionPrimary"));
    clearReceiveButton = new QPushButton(receivePanelFrame);
    saveReceiveButton = new QPushButton(receivePanelFrame);
    receiveHeaderLayout->addWidget(receiveSectionLabel);
    receiveHeaderLayout->addWidget(clearReceiveButton);
    receiveHeaderLayout->addWidget(saveReceiveButton);
    receiveHeaderLayout->addStretch();
    receivePanelLayout->addLayout(receiveHeaderLayout);

    auto *receiveOptionsLayout = new QHBoxLayout;
    receiveOptionsLayout->setContentsMargins(0, 0, 0, 0);
    receiveOptionsLayout->setSpacing(6);
    pauseDisplayCheckBox = new QCheckBox(receivePanelFrame);
    hexDisplayCheckBox = new QCheckBox(receivePanelFrame);
    timestampCheckBox = new QCheckBox(receivePanelFrame);
    wrapCheckBox = new QCheckBox(receivePanelFrame);
    packetSplitCheckBox = new QCheckBox(receivePanelFrame);
    receiveTimeoutLabel = new QLabel(receivePanelFrame);
    receiveTimeoutSpinBox = new QSpinBox(receivePanelFrame);
    receiveTimeoutUnitLabel = new QLabel(receivePanelFrame);
    timestampCheckBox->setChecked(true);
    wrapCheckBox->setChecked(true);
    receiveTimeoutSpinBox->setRange(1, 5000);
    receiveTimeoutSpinBox->setValue(50);
    receiveTimeoutSpinBox->setMinimumWidth(72);
    receiveTimeoutUnitLabel->setText(QStringLiteral("ms"));
    receiveTimeoutLabel->setEnabled(false);
    receiveTimeoutSpinBox->setEnabled(false);
    receiveTimeoutUnitLabel->setEnabled(false);
    receiveOptionsLayout->addWidget(pauseDisplayCheckBox);
    receiveOptionsLayout->addWidget(hexDisplayCheckBox);
    receiveOptionsLayout->addWidget(timestampCheckBox);
    receiveOptionsLayout->addWidget(wrapCheckBox);
    receiveOptionsLayout->addSpacing(8);
    receiveOptionsLayout->addWidget(packetSplitCheckBox);
    receiveOptionsLayout->addWidget(receiveTimeoutLabel);
    receiveOptionsLayout->addWidget(receiveTimeoutSpinBox);
    receiveOptionsLayout->addWidget(receiveTimeoutUnitLabel);
    receiveOptionsLayout->addStretch();
    receivePanelLayout->addLayout(receiveOptionsLayout);

    receiveEdit = new QPlainTextEdit(receivePanelFrame);
    receiveEdit->setObjectName(QStringLiteral("receiveView"));
    receiveEdit->setReadOnly(true);
    receiveEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    receiveEdit->setWordWrapMode(QTextOption::NoWrap);
    receiveEdit->setMaximumBlockCount(4000);
    topDisplaySplitter = new QSplitter(Qt::Horizontal, receivePanelFrame);
    topDisplaySplitter->setChildrenCollapsible(false);
    topDisplaySplitter->setHandleWidth(6);
    topDisplaySplitter->addWidget(receiveEdit);
    receivePanelLayout->addWidget(topDisplaySplitter, 1);
    rootLayout->addWidget(receivePanelFrame, 1);

    // 底部操作区只放串口连接和发送，两类动作左右分开。
    auto *controlPanel = new QFrame(ui->centralwidget);
    controlPanel->setObjectName(QStringLiteral("controlPanel"));
    auto *panelLayout = new QVBoxLayout(controlPanel);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(0);

    auto *bottomCardsLayout = new QHBoxLayout;
    bottomCardsLayout->setContentsMargins(0, 0, 0, 0);
    bottomCardsLayout->setSpacing(10);

    auto *serialConfigFrame = new QFrame(controlPanel);
    serialConfigFrame->setObjectName(QStringLiteral("configCard"));
    auto *serialConfigCardLayout = new QVBoxLayout(serialConfigFrame);
    serialConfigCardLayout->setContentsMargins(12, 10, 12, 10);
    serialConfigCardLayout->setSpacing(8);

    auto *sendConfigFrame = new QFrame(controlPanel);
    sendConfigFrame->setObjectName(QStringLiteral("configCard"));
    auto *sendConfigCardLayout = new QVBoxLayout(sendConfigFrame);
    sendConfigCardLayout->setContentsMargins(12, 10, 12, 10);
    sendConfigCardLayout->setSpacing(8);

    auto *serialSectionTitleLabel = new QLabel(serialConfigFrame);
    serialSectionTitleLabel->setObjectName(QStringLiteral("serialSectionTitle"));
    serialSectionTitleLabel->setProperty("sectionRole", QStringLiteral("secondary"));
    serialConfigCardLayout->addWidget(serialSectionTitleLabel);

    // 串口连接与电平控制归到左侧卡片。
    auto *connectionLayout = new QHBoxLayout;
    connectionLayout->setContentsMargins(0, 0, 0, 0);
    connectionLayout->setSpacing(6);
    portLabel = new QLabel(serialConfigFrame);
    portComboBox = new QComboBox(serialConfigFrame);
    portComboBox->setEditable(true);
    portComboBox->setInsertPolicy(QComboBox::NoInsert);
    portComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    portComboBox->setMinimumWidth(QFontMetrics(portComboBox->font())
                                      .horizontalAdvance(QStringLiteral("COM13 USB-SERIAL CH340"))
                                  + 64);
    polishComboBox(portComboBox);

    refreshPortsButton = new QPushButton(serialConfigFrame);
    openPortButton = new QPushButton(serialConfigFrame);
    connectionLayout->addWidget(portLabel);
    connectionLayout->addWidget(portComboBox, 1);
    connectionLayout->addWidget(refreshPortsButton);
    connectionLayout->addWidget(openPortButton);
    serialConfigCardLayout->addLayout(connectionLayout);

    baudRateLabel = new QLabel(serialConfigFrame);
    baudRateComboBox = new QComboBox(serialConfigFrame);
    baudRateComboBox->setEditable(true);
    baudRateComboBox->setInsertPolicy(QComboBox::NoInsert);
    baudRateComboBox->setMinimumContentsLength(9);
    baudRateComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    baudRateComboBox->setMinimumWidth(148);
    polishComboBox(baudRateComboBox);
    populateBaudRateOptions();
    baudRateComboBox->setToolTip(
        uiText(QStringLiteral("可直接输入自定义波特率，或从下拉中选择“自定义...”"),
               QStringLiteral("Type a custom baud rate directly or choose \"Custom...\".")));

    dataBitsLabel = new QLabel(serialConfigFrame);
    dataBitsComboBox = new QComboBox(serialConfigFrame);
    dataBitsComboBox->setMinimumWidth(72);
    polishComboBox(dataBitsComboBox);
    dataBitsComboBox->addItems({
        QStringLiteral("5"),
        QStringLiteral("6"),
        QStringLiteral("7"),
        QStringLiteral("8")
    });
    dataBitsComboBox->setCurrentText(QStringLiteral("8"));

    parityLabel = new QLabel(serialConfigFrame);
    parityComboBox = new QComboBox(serialConfigFrame);
    parityComboBox->setMinimumWidth(96);
    polishComboBox(parityComboBox);

    stopBitsLabel = new QLabel(serialConfigFrame);
    stopBitsComboBox = new QComboBox(serialConfigFrame);
    stopBitsComboBox->setMinimumWidth(84);
    polishComboBox(stopBitsComboBox);
    stopBitsComboBox->addItems({
        QStringLiteral("1"),
        QStringLiteral("1.5"),
        QStringLiteral("2")
    });

    rtsCheckBox = new QCheckBox(QStringLiteral("RTS"), serialConfigFrame);
    dtrCheckBox = new QCheckBox(QStringLiteral("DTR"), serialConfigFrame);

    auto *serialParamsLayout = new QGridLayout;
    serialParamsLayout->setContentsMargins(0, 0, 0, 0);
    serialParamsLayout->setHorizontalSpacing(8);
    serialParamsLayout->setVerticalSpacing(6);
    serialParamsLayout->addWidget(baudRateLabel, 0, 0);
    serialParamsLayout->addWidget(baudRateComboBox, 0, 1);
    serialParamsLayout->addWidget(dataBitsLabel, 0, 2);
    serialParamsLayout->addWidget(dataBitsComboBox, 0, 3);
    serialParamsLayout->addWidget(parityLabel, 1, 0);
    serialParamsLayout->addWidget(parityComboBox, 1, 1);
    serialParamsLayout->addWidget(stopBitsLabel, 1, 2);
    serialParamsLayout->addWidget(stopBitsComboBox, 1, 3);
    serialParamsLayout->addWidget(rtsCheckBox, 1, 4);
    serialParamsLayout->addWidget(dtrCheckBox, 1, 5);
    serialParamsLayout->setColumnStretch(6, 1);
    serialConfigCardLayout->addLayout(serialParamsLayout);
    serialConfigCardLayout->addStretch();

    // 发送卡片集中放发送编辑、文件导入、循环发送和校验附加。
    auto *sendHeaderLayout = new QHBoxLayout;
    sendHeaderLayout->setContentsMargins(0, 0, 0, 0);
    sendHeaderLayout->setSpacing(8);
    sendSectionLabel = new QLabel(sendConfigFrame);
    sendSectionLabel->setObjectName(QStringLiteral("sectionSecondary"));
    presetDockToggleButton = new QPushButton(sendConfigFrame);
    presetDockToggleButton->setCheckable(true);
    presetSchedulerStatusLabel = new QLabel(sendConfigFrame);
    presetSchedulerStatusLabel->setObjectName(QStringLiteral("presetStatusChip"));
    presetSchedulerStatusLabel->setMinimumWidth(0);
    presetSchedulerStatusLabel->setWordWrap(false);
    presetSchedulerStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    presetSchedulerStatusLabel->setVisible(false);
    sendHeaderLayout->addWidget(sendSectionLabel);
    sendHeaderLayout->addStretch();
    sendHeaderLayout->addWidget(presetDockToggleButton);
    sendHeaderLayout->addWidget(presetSchedulerStatusLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    sendConfigCardLayout->addLayout(sendHeaderLayout);

    loadSendFileButton = new QPushButton(sendConfigFrame);
    clearSendButton = new QPushButton(sendConfigFrame);
    sendButton = new QPushButton(sendConfigFrame);
    sendButton->setObjectName(QStringLiteral("sendButton"));
    sendButton->setMinimumHeight(86);

    sendEdit = new QPlainTextEdit(sendConfigFrame);
    sendEdit->setObjectName(QStringLiteral("sendView"));
    sendEdit->setFixedHeight(86);

    auto *sendEditorRowLayout = new QHBoxLayout;
    sendEditorRowLayout->setContentsMargins(0, 0, 0, 0);
    sendEditorRowLayout->setSpacing(6);
    sendEditorRowLayout->addWidget(sendEdit, 1);
    sendEditorRowLayout->addWidget(sendButton, 0, Qt::AlignBottom);

    auto *sendToolsLayout = new QHBoxLayout;
    sendToolsLayout->setContentsMargins(0, 0, 0, 0);
    sendToolsLayout->setSpacing(6);
    hexSendCheckBox = new QCheckBox(sendConfigFrame);
    appendNewLineCheckBox = new QCheckBox(sendConfigFrame);
    autoSendCheckBox = new QCheckBox(sendConfigFrame);
    intervalLabel = new QLabel(sendConfigFrame);
    autoSendIntervalSpinBox = new QSpinBox(sendConfigFrame);
    autoSendIntervalUnitLabel = new QLabel(sendConfigFrame);
    autoSendIntervalSpinBox->setRange(10, 60000);
    autoSendIntervalSpinBox->setValue(500);
    autoSendIntervalSpinBox->setMinimumWidth(76);
    autoSendIntervalUnitLabel->setText(QStringLiteral("ms"));

    sendToolsLayout->addWidget(loadSendFileButton);
    sendToolsLayout->addWidget(clearSendButton);
    sendToolsLayout->addSpacing(8);
    sendToolsLayout->addWidget(hexSendCheckBox);
    sendToolsLayout->addWidget(appendNewLineCheckBox);
    sendToolsLayout->addWidget(autoSendCheckBox);
    sendToolsLayout->addWidget(intervalLabel);
    sendToolsLayout->addWidget(autoSendIntervalSpinBox);
    sendToolsLayout->addWidget(autoSendIntervalUnitLabel);
    sendToolsLayout->addStretch();

    auto *sendChecksumLayout = new QHBoxLayout;
    sendChecksumLayout->setContentsMargins(0, 0, 0, 0);
    sendChecksumLayout->setSpacing(6);
    checksumLabel = new QLabel(sendConfigFrame);
    checksumComboBox = new QComboBox(sendConfigFrame);
    checksumComboBox->setMinimumWidth(150);
    polishComboBox(checksumComboBox);
    sendChecksumLayout->addWidget(checksumLabel);
    sendChecksumLayout->addWidget(checksumComboBox);
    sendChecksumLayout->addStretch();

    // 文件导入提示卡：说明当前发送区里是完整内容还是安全预览。
    importNoticeFrame = new QFrame(sendConfigFrame);
    importNoticeFrame->setObjectName(QStringLiteral("importNoticeCard"));
    importNoticeFrame->setVisible(false);

    auto *noticeLayout = new QHBoxLayout(importNoticeFrame);
    noticeLayout->setContentsMargins(12, 10, 12, 10);
    noticeLayout->setSpacing(10);

    auto *noticeTextLayout = new QVBoxLayout;
    noticeTextLayout->setContentsMargins(0, 0, 0, 0);
    noticeTextLayout->setSpacing(2);

    importNoticeTitleLabel = new QLabel(importNoticeFrame);
    importNoticeTitleLabel->setObjectName(QStringLiteral("importNoticeTitle"));
    importNoticeTitleLabel->setWordWrap(true);
    importNoticeDetailLabel = new QLabel(importNoticeFrame);
    importNoticeDetailLabel->setObjectName(QStringLiteral("importNoticeDetail"));
    importNoticeDetailLabel->setWordWrap(true);
    importNoticeHintLabel = new QLabel(importNoticeFrame);
    importNoticeHintLabel->setObjectName(QStringLiteral("importNoticeHint"));
    importNoticeHintLabel->setWordWrap(true);

    noticeTextLayout->addWidget(importNoticeTitleLabel);
    noticeTextLayout->addWidget(importNoticeDetailLabel);
    noticeTextLayout->addWidget(importNoticeHintLabel);

    auto *noticeButtonLayout = new QVBoxLayout;
    noticeButtonLayout->setContentsMargins(0, 0, 0, 0);
    noticeButtonLayout->setSpacing(6);

    importNoticeActionButton = new QPushButton(importNoticeFrame);
    importNoticeActionButton->setObjectName(QStringLiteral("importNoticeAction"));
    importNoticeActionButton->setVisible(false);
    importNoticeCloseButton = new QPushButton(importNoticeFrame);
    importNoticeCloseButton->setObjectName(QStringLiteral("importNoticeClose"));

    noticeButtonLayout->addWidget(importNoticeActionButton);
    noticeButtonLayout->addWidget(importNoticeCloseButton);
    noticeButtonLayout->addStretch();

    noticeLayout->addLayout(noticeTextLayout, 1);
    noticeLayout->addLayout(noticeButtonLayout);

    sendConfigCardLayout->addWidget(importNoticeFrame);
    sendConfigCardLayout->addLayout(sendEditorRowLayout);
    sendConfigCardLayout->addLayout(sendToolsLayout);
    sendConfigCardLayout->addLayout(sendChecksumLayout);
    sendConfigCardLayout->addStretch();

    bottomCardsLayout->addWidget(serialConfigFrame, 4);
    bottomCardsLayout->addWidget(sendConfigFrame, 7);
    panelLayout->addLayout(bottomCardsLayout);

    rootLayout->addWidget(controlPanel);

    // 多字符串面板和工具箱都做成独立对象，主窗口只负责接线和状态同步。
    // 这样后面继续加功能时，不需要把所有业务都塞回主窗口 central widget。
    // 扩展工具放在 central widget 之外：
    // - presetDock：可停靠的多字符串发送面板
    // - toolboxDialog：协议与工具箱对话框
    presetDock = new MultiStringDock(this);
    presetDock->setFeatures(QDockWidget::DockWidgetClosable);
    topDisplaySplitter->addWidget(presetDock);
    topDisplaySplitter->setStretchFactor(0, 5);
    topDisplaySplitter->setStretchFactor(1, 4);
    topDisplaySplitter->setSizes({900, 0});
    presetDock->hide();
    showPresetDockAction = presetDock->toggleViewAction();

    toolboxDialog = new ToolboxDialog(this);
    buildModbusMonitorDock();
}

/**
 * @brief 构建主窗口菜单与动作
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::buildMenus()
{
    connectionMenu = menuBar()->addMenu(QString());
    refreshAction = connectionMenu->addAction(QString());
    togglePortAction = connectionMenu->addAction(QString());
    connectionMenu->addSeparator();
    quitAction = connectionMenu->addAction(QString());

    configMenu = menuBar()->addMenu(QString());
    resetCounterAction = configMenu->addAction(QString());
    copySummaryAction = configMenu->addAction(QString());

    viewMenu = menuBar()->addMenu(QString());
    clearReceiveAction = viewMenu->addAction(QString());
    saveReceiveAction = viewMenu->addAction(QString());
    viewMenu->addSeparator();
    hexDisplayAction = viewMenu->addAction(QString());
    hexDisplayAction->setCheckable(true);
    timestampAction = viewMenu->addAction(QString());
    timestampAction->setCheckable(true);
    wrapAction = viewMenu->addAction(QString());
    wrapAction->setCheckable(true);
    viewMenu->addSeparator();
    increaseFontAction = viewMenu->addAction(QString());
    decreaseFontAction = viewMenu->addAction(QString());
    resetFontAction = viewMenu->addAction(QString());
    fontSizeAction = viewMenu->addAction(QString());
    fontFamilyAction = viewMenu->addAction(QString());
    viewMenu->addSeparator();
    highlightAction = viewMenu->addAction(QString());
    clearHighlightAction = viewMenu->addAction(QString());
    filterAction = viewMenu->addAction(QString());
    clearFilterAction = viewMenu->addAction(QString());

    sendMenu = menuBar()->addMenu(QString());
    sendNowAction = sendMenu->addAction(QString());
    loadFileAction = sendMenu->addAction(QString());
    sendFileAction = sendMenu->addAction(QString());
    sendMenu->addSeparator();
    hexSendAction = sendMenu->addAction(QString());
    hexSendAction->setCheckable(true);
    autoSendAction = sendMenu->addAction(QString());
    autoSendAction->setCheckable(true);

    presetMenu = nullptr;
    heartbeatAction = new QAction(this);
    modbusAction = new QAction(this);

    toolsMenu = menuBar()->addMenu(QString());
    clearSendAction = toolsMenu->addAction(QString());
    pauseAction = toolsMenu->addAction(QString());
    pauseAction->setCheckable(true);
    toolsMenu->addSeparator();
    startCaptureAction = toolsMenu->addAction(QString());
    stopCaptureAction = toolsMenu->addAction(QString());
    toolsMenu->addSeparator();
    protocolToolboxMenu = toolsMenu->addMenu(QString());
    calculatorAction = protocolToolboxMenu->addAction(QString());
    hexConverterAction = protocolToolboxMenu->addAction(QString());
    customProtocolToolAction = protocolToolboxMenu->addAction(QString());

    toolsMenu->addSeparator();
    toolsMenu->addAction(modbusMonitorAction);
    modbusToolAction = modbusMonitorAction;
    menuBar()->addAction(showPresetDockAction);

    languageMenu = menuBar()->addMenu(QString());
    languageChineseAction = languageMenu->addAction(QString());
    languageChineseAction->setCheckable(true);
    languageEnglishAction = languageMenu->addAction(QString());
    languageEnglishAction->setCheckable(true);

    auto *languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    languageGroup->addAction(languageChineseAction);
    languageGroup->addAction(languageEnglishAction);

    helpMenu = menuBar()->addMenu(QString());
    guideAction = helpMenu->addAction(QString());
    aboutAction = helpMenu->addAction(QString());
}

/**
 * @brief 构建主窗口状态栏
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::buildStatusBar()
{
    ui->statusbar->setSizeGripEnabled(false);

    auto *statusStrip = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusStrip);
    statusLayout->setContentsMargins(3, 2, 3, 2);
    statusLayout->setSpacing(2);

    const auto initSegment = [](QLabel *label, int minimumWidth, Qt::Alignment alignment) {
        label->setProperty("statusRole", QStringLiteral("segment"));
        label->setMinimumHeight(20);
        if (minimumWidth > 0) {
            label->setMinimumWidth(minimumWidth);
        }
        label->setAlignment(alignment);
    };

    brandStatusLabel = new QLabel(statusStrip);
    sendCountLabel = new QLabel(statusStrip);
    receiveCountLabel = new QLabel(statusStrip);
    portStateLabel = new QLabel(statusStrip);
    lineStateLabel = new QLabel(QStringLiteral("CTS=0  DSR=0  RLSD=0"), statusStrip);

    initSegment(brandStatusLabel, 88, Qt::AlignLeft | Qt::AlignVCenter);
    initSegment(sendCountLabel, 72, Qt::AlignLeft | Qt::AlignVCenter);
    initSegment(receiveCountLabel, 72, Qt::AlignLeft | Qt::AlignVCenter);
    initSegment(portStateLabel, 360, Qt::AlignLeft | Qt::AlignVCenter);
    initSegment(lineStateLabel, 180, Qt::AlignLeft | Qt::AlignVCenter);

    statusLayout->addWidget(brandStatusLabel);
    statusLayout->addWidget(sendCountLabel);
    statusLayout->addWidget(receiveCountLabel);
    statusLayout->addWidget(portStateLabel, 1);
    statusLayout->addWidget(lineStateLabel);

    ui->statusbar->addWidget(statusStrip, 1);
}

/**
 * @brief 连接主界面控件与业务信号
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::connectUi()
{
    // 面板按钮与菜单动作尽量共用同一套业务逻辑。
    connect(clearReceiveButton, &QPushButton::clicked, this, [this] {
        receivePacketTimer->stop();
        pendingReceivePacket.clear();
        receiveLogBuffer.clear();
        refreshReceiveView();
        statusBar()->showMessage(uiText(QStringLiteral("接收区已清空"),
                                        QStringLiteral("Receive panel cleared")),
                                 2000);
    });
    connect(saveReceiveButton, &QPushButton::clicked, this, &MainWindow::saveReceiveContent);
    connect(loadSendFileButton, &QPushButton::clicked, this, &MainWindow::loadSendFile);
    connect(clearSendButton, &QPushButton::clicked, this, [this] {
        sendEdit->clear();
        clearImportedFileNotice();
    });
    connect(importNoticeCloseButton, &QPushButton::clicked, this, &MainWindow::clearImportedFileNotice);
    connect(importNoticeActionButton, &QPushButton::clicked, this, [this] {
        queueFileForSending(importedFilePath);
    });

    connect(refreshPortsButton, &QPushButton::clicked, this, [this] {
        refreshPortList(portComboBox->currentText());
    });

    connect(openPortButton, &QPushButton::clicked, this, [this] {
        if (portOpen) {
            emit requestClosePort();
            return;
        }

        const QString portName = currentPortName(portComboBox);
        if (portName.isEmpty()) {
            statusBar()->showMessage(uiText(QStringLiteral("请选择有效的串口号"),
                                            QStringLiteral("Please choose a valid COM port")),
                                     3000);
            return;
        }

        emit requestOpenPort(portName,
                             currentBaudRate(),
                             currentDataBits(),
                             currentParityCode(),
                             stopBitsComboBox->currentText());
    });

    connect(sendButton, &QPushButton::clicked, this, [this] { sendCurrentPayload(false); });
    connect(sendEdit, &QPlainTextEdit::textChanged, this, &MainWindow::saveSendEditorText);
    connect(wrapCheckBox, &QCheckBox::toggled, this, [this] {
        updateWrapMode();
        saveDisplaySettings();
    });
    connect(packetSplitCheckBox, &QCheckBox::toggled, this, [this](bool enabled) {
        receiveTimeoutLabel->setEnabled(enabled);
        receiveTimeoutSpinBox->setEnabled(enabled);
        if (receiveTimeoutUnitLabel != nullptr) {
            receiveTimeoutUnitLabel->setEnabled(enabled);
        }
        if (!enabled) {
            flushPendingReceivePacket();
        }
        saveDisplaySettings();
    });
    connect(receiveTimeoutSpinBox, &QSpinBox::valueChanged, this, [this](int) {
        if (receivePacketTimer->isActive()) {
            receivePacketTimer->start(receiveTimeoutSpinBox->value());
        }
        saveDisplaySettings();
    });
    connect(checksumComboBox, &QComboBox::currentTextChanged, this, [this](const QString &) {
        saveDisplaySettings();
    });
    connect(autoSendCheckBox, &QCheckBox::toggled, this, [this] { updateAutoSendTimer(); });
    connect(autoSendIntervalSpinBox,
            &QSpinBox::valueChanged,
            this,
            [this](int) { updateAutoSendTimer(); });
    connect(rtsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit requestSetRtsEnabled(checked);
    });
    connect(dtrCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        emit requestSetDtrEnabled(checked);
    });
    connect(pauseDisplayCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            receivePacketTimer->stop();
            pendingReceivePacket.clear();
        }
    });
    connect(timestampCheckBox, &QCheckBox::toggled, this, [this](bool) {
        flushPendingReceivePacket();
        saveDisplaySettings();
    });
    connect(hexDisplayCheckBox, &QCheckBox::toggled, this, [this](bool) {
        flushPendingReceivePacket();
        saveDisplaySettings();
    });

    // 参数变化后，即使串口尚未连接，也立即更新状态栏预览。
    const auto updatePreview = [this] {
        updatePortSummary();
    };

    connect(portComboBox, &QComboBox::currentTextChanged, this, [this, updatePreview](const QString &) {
        const QString tooltipText = currentPortDisplayText(portComboBox);
        portComboBox->setToolTip(tooltipText);
        if (QLineEdit *editor = portComboBox->lineEdit()) {
            editor->setToolTip(tooltipText);
        }
        updatePreview();
        saveDisplaySettings();
    });
    connect(portComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        if (QLineEdit *editor = portComboBox->lineEdit()) {
            editor->setCursorPosition(0);
        }
    });
    connect(baudRateComboBox, &QComboBox::currentTextChanged, this, [this, updatePreview](const QString &text) {
        bool ok = false;
        const int baudRate = text.trimmed().toInt(&ok);
        if (ok && baudRate > 0) {
            baudRateComboBox->setProperty("lastValidBaud", QString::number(baudRate));
            saveDisplaySettings();
        }
        updatePreview();
    });
    connect(baudRateComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (baudRateComboBox->itemData(index).toString() == QString::fromLatin1(kCustomBaudRateToken)) {
            promptCustomBaudRate();
        }
    });
    connect(dataBitsComboBox, &QComboBox::currentTextChanged, this, [this, updatePreview](const QString &) {
        updatePreview();
        saveDisplaySettings();
    });
    connect(parityComboBox, &QComboBox::currentTextChanged, this, [this, updatePreview](const QString &) {
        updatePreview();
        saveDisplaySettings();
    });
    connect(stopBitsComboBox, &QComboBox::currentTextChanged, this, [this, updatePreview](const QString &) {
        updatePreview();
        saveDisplaySettings();
    });

    // 菜单动作。
    connect(refreshAction, &QAction::triggered, this, [this] { refreshPortList(portComboBox->currentText()); });
    connect(togglePortAction, &QAction::triggered, openPortButton, &QPushButton::click);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    connect(resetCounterAction, &QAction::triggered, this, [this] {
        totalSendBytes = 0;
        totalReceiveBytes = 0;
        updateCounters();
        statusBar()->showMessage(uiText(QStringLiteral("收发统计已清零"),
                                        QStringLiteral("Traffic counters reset")),
                                 2500);
    });
    connect(copySummaryAction, &QAction::triggered, this, [this] {
        QGuiApplication::clipboard()->setText(portStateLabel->text());
        statusBar()->showMessage(uiText(QStringLiteral("连接摘要已复制"),
                                        QStringLiteral("Connection summary copied")),
                                 2500);
    });
    connect(clearReceiveAction, &QAction::triggered, clearReceiveButton, &QPushButton::click);
    connect(saveReceiveAction, &QAction::triggered, saveReceiveButton, &QPushButton::click);
    connect(hexDisplayAction, &QAction::toggled, hexDisplayCheckBox, &QCheckBox::setChecked);
    connect(timestampAction, &QAction::toggled, timestampCheckBox, &QCheckBox::setChecked);
    connect(wrapAction, &QAction::toggled, wrapCheckBox, &QCheckBox::setChecked);
    connect(sendNowAction, &QAction::triggered, sendButton, &QPushButton::click);
    connect(loadFileAction, &QAction::triggered, loadSendFileButton, &QPushButton::click);
    connect(sendFileAction, &QAction::triggered, this, &MainWindow::sendFileData);
    connect(hexSendAction, &QAction::toggled, hexSendCheckBox, &QCheckBox::setChecked);
    connect(autoSendAction, &QAction::toggled, autoSendCheckBox, &QCheckBox::setChecked);
    connect(clearSendAction, &QAction::triggered, clearSendButton, &QPushButton::click);
    connect(pauseAction, &QAction::toggled, pauseDisplayCheckBox, &QCheckBox::setChecked);
    connect(increaseFontAction, &QAction::triggered, this, [this] {
        consoleFontPointSize = qMin(consoleFontPointSize + 1, 36);
        updateConsoleFont();
        saveDisplaySettings();
    });
    connect(decreaseFontAction, &QAction::triggered, this, [this] {
        consoleFontPointSize = qMax(consoleFontPointSize - 1, 9);
        updateConsoleFont();
        saveDisplaySettings();
    });
    connect(resetFontAction, &QAction::triggered, this, [this] {
        const QFont defaultFont = defaultConsoleFont();
        consoleFontPointSize = defaultFont.pointSize();
        consoleFontFamily = defaultFont.family();
        updateConsoleFont();
        saveDisplaySettings();
    });
    connect(fontSizeAction, &QAction::triggered, this, [this] {
        bool ok = false;
        const int value = QInputDialog::getInt(this,
                                               uiText(QStringLiteral("设置字号"), QStringLiteral("Set Font Size")),
                                               uiText(QStringLiteral("接收区字号"), QStringLiteral("Console font size")),
                                               consoleFontPointSize,
                                               9,
                                               36,
                                               1,
                                               &ok);
        if (!ok) {
            return;
        }
        consoleFontPointSize = value;
        updateConsoleFont();
        saveDisplaySettings();
    });
    connect(fontFamilyAction, &QAction::triggered, this, &MainWindow::chooseConsoleFont);
    connect(filterAction, &QAction::triggered, this, [this] {
        bool ok = false;
        const QString keyword = QInputDialog::getText(
            this,
            uiText(QStringLiteral("设置筛选关键字"), QStringLiteral("Set Filter Keyword")),
            uiText(QStringLiteral("只显示包含该关键字的行"), QStringLiteral("Only show lines containing this keyword")),
            QLineEdit::Normal,
            receiveFilterKeyword,
            &ok);
        if (!ok) {
            return;
        }
        receiveFilterKeyword = keyword.trimmed();
        refreshReceiveView();
        saveDisplaySettings();
    });
    connect(clearFilterAction, &QAction::triggered, this, [this] {
        receiveFilterKeyword.clear();
        refreshReceiveView();
        saveDisplaySettings();
    });
    connect(highlightAction, &QAction::triggered, this, [this] {
        bool ok = false;
        const QString keyword = QInputDialog::getText(
            this,
            uiText(QStringLiteral("设置高亮关键字"), QStringLiteral("Set Highlight Keyword")),
            uiText(QStringLiteral("高亮接收区内匹配内容"), QStringLiteral("Highlight matching text in receive view")),
            QLineEdit::Normal,
            receiveHighlightKeyword,
            &ok);
        if (!ok) {
            return;
        }
        receiveHighlightKeyword = keyword.trimmed();
        applyReceiveHighlights();
        saveDisplaySettings();
    });
    connect(clearHighlightAction, &QAction::triggered, this, [this] {
        receiveHighlightKeyword.clear();
        applyReceiveHighlights();
        saveDisplaySettings();
    });
    connect(startCaptureAction, &QAction::triggered, this, &MainWindow::startLiveCapture);
    connect(stopCaptureAction, &QAction::triggered, this, &MainWindow::stopLiveCapture);
    connect(calculatorAction, &QAction::triggered, this, [this] { openToolboxPage(0); });
    connect(hexConverterAction, &QAction::triggered, this, [this] { openToolboxPage(1); });
    connect(customProtocolToolAction, &QAction::triggered, this, [this] { openToolboxPage(2); });

    connect(heartbeatAction, &QAction::triggered, this, [this] {
        clearImportedFileNotice();
        sendEdit->setPlainText(QStringLiteral("AA 55 01 00"));
        hexSendCheckBox->setChecked(true);
    });
    connect(modbusAction, &QAction::triggered, this, [this] {
        clearImportedFileNotice();
        sendEdit->setPlainText(QStringLiteral("01 03 00 00 00 02 C4 0B"));
        hexSendCheckBox->setChecked(true);
    });

    connect(languageChineseAction, &QAction::triggered, this, [this] {
        applyLanguage(UiLanguage::Chinese);
        statusBar()->showMessage(QStringLiteral("已切换为中文界面"), 2000);
    });
    connect(languageEnglishAction, &QAction::triggered, this, [this] {
        applyLanguage(UiLanguage::English);
        statusBar()->showMessage(QStringLiteral("Switched to English interface"), 2000);
    });

    connect(guideAction, &QAction::triggered, this, [this] {
        showInfoDialog(uiText(QStringLiteral("使用说明"), QStringLiteral("Quick Guide")),
                       quickGuideText());
    });

    connect(aboutAction, &QAction::triggered, this, [this] {
        showInfoDialog(uiText(QStringLiteral("关于 X-Ray 串口调试台"),
                              QStringLiteral("About X-Ray Serial Console")),
                       aboutDialogText());
    });

    // 动作与复选框双向同步，避免菜单状态和面板状态不一致。
    connect(hexDisplayCheckBox, &QCheckBox::toggled, hexDisplayAction, &QAction::setChecked);
    connect(timestampCheckBox, &QCheckBox::toggled, timestampAction, &QAction::setChecked);
    connect(wrapCheckBox, &QCheckBox::toggled, wrapAction, &QAction::setChecked);
    connect(hexSendCheckBox, &QCheckBox::toggled, hexSendAction, &QAction::setChecked);
    connect(autoSendCheckBox, &QCheckBox::toggled, autoSendAction, &QAction::setChecked);
    connect(pauseDisplayCheckBox, &QCheckBox::toggled, pauseAction, &QAction::setChecked);

    // 双向连接建立后，立刻把菜单动作和底部复选框状态对齐，避免首次启动时显示不同步。
    hexDisplayAction->setChecked(hexDisplayCheckBox->isChecked());
    timestampAction->setChecked(timestampCheckBox->isChecked());
    wrapAction->setChecked(wrapCheckBox->isChecked());
    hexSendAction->setChecked(hexSendCheckBox->isChecked());
    autoSendAction->setChecked(autoSendCheckBox->isChecked());
    pauseAction->setChecked(pauseDisplayCheckBox->isChecked());

    connect(presetDock,
            &MultiStringDock::requestCaptureFromEditor,
            this,
            [this](bool replaceCurrentRow) {
                presetDock->captureEditorPayload(sendEdit->toPlainText(),
                                                 hexSendCheckBox->isChecked(),
                                                 appendNewLineCheckBox->isChecked(),
                                                 replaceCurrentRow);
            });
    connect(presetDock,
            &MultiStringDock::requestSendPayload,
            this,
            [this](const QString &payloadText, bool hexMode, bool appendNewLine) {
                sendPresetPayload(payloadText, hexMode, appendNewLine);
            });
    connect(presetDock, &MultiStringDock::schedulerStateChanged, this, &MainWindow::updatePresetSchedulerStatus);
    connect(presetDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (presetDockToggleButton != nullptr) {
            presetDockToggleButton->setChecked(visible);
        }
        applyPresetDockVisibilityLayout(visible);
        saveDisplaySettings();
        updatePresetSchedulerStatus();
    });
    connect(presetDockToggleButton, &QPushButton::clicked, this, [this](bool checked) {
        if (presetDock != nullptr) {
            presetDock->setVisible(checked);
        }
    });
    connect(showPresetDockAction, &QAction::toggled, this, [this](bool visible) {
        if (presetDockToggleButton != nullptr && presetDockToggleButton->isChecked() != visible) {
            presetDockToggleButton->setChecked(visible);
        }
        updatePresetSchedulerStatus();
    });
    connect(topDisplaySplitter, &QSplitter::splitterMoved, this, [this] {
        if (presetDock != nullptr && presetDock->isVisible()) {
            saveDisplaySettings();
        }
    });
    connect(modbusMonitorDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible) {
            // 工具窗口关闭后必须停止独立扫描，避免隐藏定时器继续占用串口发送链路。
            modbusMonitorTimer->stop();
            cancelModbusMonitorReceiveSession(true);
            if (modbusMonitorStartButton != nullptr && modbusMonitorStartButton->isChecked()) {
                QSignalBlocker blocker(modbusMonitorStartButton);
                modbusMonitorStartButton->setChecked(false);
            }
            updateModbusMonitorState();
        }
        saveDisplaySettings();
    });
    connect(modbusMonitorSplitter, &QSplitter::splitterMoved, this, [this] {
        saveDisplaySettings();
    });
    connect(modbusRegisterTable->horizontalHeader(), &QHeaderView::sectionResized, this, [this] {
        saveDisplaySettings();
    });

    connect(toolboxDialog,
            &ToolboxDialog::requestLoadPayload,
            this,
            [this](const QString &payloadText, bool hexMode, bool appendNewLine) {
                clearImportedFileNotice();
                sendEdit->setPlainText(payloadText);
                hexSendCheckBox->setChecked(hexMode);
                appendNewLineCheckBox->setChecked(appendNewLine);
            });
    connect(toolboxDialog,
            &ToolboxDialog::requestSendPayload,
            this,
            [this](const QString &payloadText, bool hexMode, bool appendNewLine) {
                sendToolboxPayload(payloadText, hexMode, appendNewLine);
            });

    updatePresetSchedulerStatus();
}

/**
 * @brief 按当前语言刷新界面文本
 *
 * @param language: 需要切换到的界面语言
 *
 * @return 无
 */
void MainWindow::applyLanguage(UiLanguage language)
{
    currentLanguage = language;

    // 先调整下拉框中带文本的条目，保证后续摘要和界面文本一致。
    populateParityOptions();
    populateBaudRateOptions();

    setWindowTitle(windowTitleText());

    receiveSectionLabel->setText(uiText(QStringLiteral("接收监视"), QStringLiteral("Receive Panel")));
    clearReceiveButton->setText(uiText(QStringLiteral("清空窗口"), QStringLiteral("Clear")));
    loadSendFileButton->setText(uiText(QStringLiteral("导入文件"), QStringLiteral("Import")));
    saveReceiveButton->setText(uiText(QStringLiteral("导出接收"), QStringLiteral("Export")));
    pauseDisplayCheckBox->setText(uiText(QStringLiteral("暂停刷新"), QStringLiteral("Pause View")));
    hexDisplayCheckBox->setText(uiText(QStringLiteral("HEX显示"), QStringLiteral("HEX View")));
    timestampCheckBox->setText(uiText(QStringLiteral("时间戳"), QStringLiteral("Timestamp")));
    wrapCheckBox->setText(uiText(QStringLiteral("自动换行"), QStringLiteral("Auto Wrap")));
    packetSplitCheckBox->setText(uiText(QStringLiteral("超时分包显示"), QStringLiteral("Split By Timeout")));
    receiveTimeoutLabel->setText(uiText(QStringLiteral("超时"), QStringLiteral("Timeout")));
    if (receiveTimeoutUnitLabel != nullptr) {
        receiveTimeoutUnitLabel->setText(QStringLiteral("ms"));
    }

    if (auto *serialSectionTitleLabel = findChild<QLabel *>(QStringLiteral("serialSectionTitle"))) {
        serialSectionTitleLabel->setText(uiText(QStringLiteral("串口连接"), QStringLiteral("Serial Connection")));
    }
    portLabel->setText(uiText(QStringLiteral("端口"), QStringLiteral("Port")));
    refreshPortsButton->setText(uiText(QStringLiteral("刷新端口"), QStringLiteral("Refresh")));
    baudRateLabel->setText(uiText(QStringLiteral("波特率"), QStringLiteral("Baud")));
    dataBitsLabel->setText(uiText(QStringLiteral("数据位"), QStringLiteral("Data Bits")));
    parityLabel->setText(uiText(QStringLiteral("校验"), QStringLiteral("Parity")));
    stopBitsLabel->setText(uiText(QStringLiteral("停止位"), QStringLiteral("Stop Bits")));
    rtsCheckBox->setText(QStringLiteral("RTS"));
    dtrCheckBox->setText(QStringLiteral("DTR"));

    sendSectionLabel->setText(uiText(QStringLiteral("发送面板"), QStringLiteral("Transmit Panel")));
    hexSendCheckBox->setText(uiText(QStringLiteral("HEX发送"), QStringLiteral("HEX Send")));
    appendNewLineCheckBox->setText(uiText(QStringLiteral("追加换行"), QStringLiteral("Append CRLF")));
    autoSendCheckBox->setText(uiText(QStringLiteral("循环发送"), QStringLiteral("Loop Send")));
    intervalLabel->setText(uiText(QStringLiteral("间隔"), QStringLiteral("Interval")));
    if (autoSendIntervalUnitLabel != nullptr) {
        autoSendIntervalUnitLabel->setText(QStringLiteral("ms"));
    }
    checksumLabel->setText(uiText(QStringLiteral("校验附加"), QStringLiteral("Checksum")));
    clearSendButton->setText(uiText(QStringLiteral("清空发送"), QStringLiteral("Clear Send")));
    sendButton->setText(uiText(QStringLiteral("发送数据"), QStringLiteral("Send")));
    importNoticeCloseButton->setText(uiText(QStringLiteral("收起提示"), QStringLiteral("Hide")));

    const int currentChecksumMode = checksumComboBox->currentData().toInt();
    {
        QSignalBlocker blocker(checksumComboBox);
        checksumComboBox->clear();
        checksumComboBox->addItem(uiText(QStringLiteral("无"), QStringLiteral("None")), kChecksumNone);
        checksumComboBox->addItem(QStringLiteral("Modbus CRC16"), kChecksumModbusCrc16);
        checksumComboBox->addItem(QStringLiteral("CCITT CRC16"), kChecksumCcittCrc16);
        checksumComboBox->addItem(QStringLiteral("CRC32"), kChecksumCrc32);
        checksumComboBox->addItem(QStringLiteral("ADD8"), kChecksumAdd8);
        checksumComboBox->addItem(QStringLiteral("XOR8"), kChecksumXor8);
        checksumComboBox->addItem(QStringLiteral("ADD16"), kChecksumAdd16);
        checksumComboBox->setCurrentIndex(qMax(0, checksumComboBox->findData(currentChecksumMode)));
    }

    receiveEdit->setPlaceholderText(uiText(QStringLiteral("接收监视区"),
                                          QStringLiteral("Receive monitor")));
    sendEdit->setPlaceholderText(uiText(QStringLiteral("输入待发送的数据；HEX 模式下请输入十六进制字节流"),
                                       QStringLiteral("Enter data to send. In HEX mode, input hexadecimal bytes.")));

    connectionMenu->setTitle(uiText(QStringLiteral("连接"), QStringLiteral("Connection")));
    refreshAction->setText(uiText(QStringLiteral("刷新端口"), QStringLiteral("Refresh Ports")));
    togglePortAction->setText(uiText(QStringLiteral("连接/断开"), QStringLiteral("Connect / Disconnect")));
    quitAction->setText(uiText(QStringLiteral("退出"), QStringLiteral("Exit")));

    configMenu->setTitle(uiText(QStringLiteral("参数"), QStringLiteral("Settings")));
    resetCounterAction->setText(uiText(QStringLiteral("清零收发统计"), QStringLiteral("Reset Counters")));
    copySummaryAction->setText(uiText(QStringLiteral("复制连接摘要"), QStringLiteral("Copy Connection Summary")));

    viewMenu->setTitle(uiText(QStringLiteral("视图"), QStringLiteral("View")));
    clearReceiveAction->setText(uiText(QStringLiteral("清空接收区"), QStringLiteral("Clear Receive")));
    saveReceiveAction->setText(uiText(QStringLiteral("导出接收文本"), QStringLiteral("Export Receive")));
    hexDisplayAction->setText(uiText(QStringLiteral("十六进制显示"), QStringLiteral("HEX Display")));
    timestampAction->setText(uiText(QStringLiteral("显示时间戳"), QStringLiteral("Show Timestamp")));
    wrapAction->setText(uiText(QStringLiteral("自动换行"), QStringLiteral("Auto Wrap")));

    sendMenu->setTitle(uiText(QStringLiteral("发送"), QStringLiteral("Send")));
    sendNowAction->setText(uiText(QStringLiteral("立即发送"), QStringLiteral("Send Now")));
    loadFileAction->setText(uiText(QStringLiteral("从文件装入发送区"), QStringLiteral("Load File To Send Box")));
    hexSendAction->setText(uiText(QStringLiteral("十六进制发送"), QStringLiteral("HEX Send")));
    autoSendAction->setText(uiText(QStringLiteral("循环发送"), QStringLiteral("Loop Send")));

    heartbeatAction->setText(uiText(QStringLiteral("载入心跳帧 AA 55 01 00"),
                                    QStringLiteral("Load Heartbeat Frame AA 55 01 00")));
    modbusAction->setText(uiText(QStringLiteral("载入 Modbus 查询帧"),
                                 QStringLiteral("Load Modbus Query Frame")));

    toolsMenu->setTitle(uiText(QStringLiteral("工具"), QStringLiteral("Tools")));
    clearSendAction->setText(uiText(QStringLiteral("清空发送区"), QStringLiteral("Clear Send Box")));
    pauseAction->setText(uiText(QStringLiteral("暂停接收刷新"), QStringLiteral("Pause Receive View")));
    protocolToolboxMenu->setTitle(uiText(QStringLiteral("协议与数据工具箱"),
                                         QStringLiteral("Protocol / Data Toolbox")));

    languageMenu->setTitle(uiText(QStringLiteral("语言"), QStringLiteral("Language")));
    languageChineseAction->setText(QStringLiteral("中文"));
    languageEnglishAction->setText(QStringLiteral("English"));

    helpMenu->setTitle(uiText(QStringLiteral("帮助"), QStringLiteral("Help")));
    guideAction->setText(uiText(QStringLiteral("使用说明"), QStringLiteral("Quick Guide")));
    aboutAction->setText(uiText(QStringLiteral("关于 X-Ray 串口调试台"),
                                QStringLiteral("About X-Ray Serial Console")));

    brandStatusLabel->setText(QStringLiteral("X-Ray"));
    presetDockToggleButton->setText(uiText(QStringLiteral("多字符串"), QStringLiteral("Multi String")));

    increaseFontAction->setText(uiText(QStringLiteral("增大字号"), QStringLiteral("Increase Font")));
    decreaseFontAction->setText(uiText(QStringLiteral("减小字号"), QStringLiteral("Decrease Font")));
    resetFontAction->setText(uiText(QStringLiteral("恢复默认字号"), QStringLiteral("Reset Font")));
    fontSizeAction->setText(uiText(QStringLiteral("设置字号..."), QStringLiteral("Set Font Size...")));
    fontFamilyAction->setText(uiText(QStringLiteral("设置字体..."), QStringLiteral("Set Font...")));
    highlightAction->setText(uiText(QStringLiteral("设置高亮关键字..."), QStringLiteral("Set Highlight Keyword...")));
    clearHighlightAction->setText(uiText(QStringLiteral("清除高亮"), QStringLiteral("Clear Highlight")));
    filterAction->setText(uiText(QStringLiteral("设置筛选关键字..."), QStringLiteral("Set Filter Keyword...")));
    clearFilterAction->setText(uiText(QStringLiteral("清除筛选"), QStringLiteral("Clear Filter")));
    showPresetDockAction->setText(uiText(QStringLiteral("多字符串"), QStringLiteral("Multi String")));
    sendFileAction->setText(uiText(QStringLiteral("直接发送文件..."), QStringLiteral("Send File Directly...")));
    startCaptureAction->setText(uiText(QStringLiteral("开始持续保存接收数据..."),
                                       QStringLiteral("Start Continuous Capture...")));
    stopCaptureAction->setText(uiText(QStringLiteral("停止持续保存"), QStringLiteral("Stop Continuous Capture")));
    calculatorAction->setText(uiText(QStringLiteral("计算器"), QStringLiteral("Calculator")));
    hexConverterAction->setText(uiText(QStringLiteral("文本/HEX 转换"), QStringLiteral("Text / HEX Converter")));
    modbusToolAction->setText(uiText(QStringLiteral("Modbus 读写工具"), QStringLiteral("Modbus Read / Write Tool")));
    customProtocolToolAction->setText(uiText(QStringLiteral("自定义协议组包解析"),
                                             QStringLiteral("Custom Protocol Builder / Parser")));
    updateModbusMonitorTexts();

    updateLanguageActions();
    updateDockTexts();
    updateImportedFileNoticeTexts();
    updatePresetSchedulerStatus();
    updateUiState();
    updateCounters();
    updatePortSummary();
}

/**
 * @brief 同步语言菜单勾选状态
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateLanguageActions()
{
    languageChineseAction->setChecked(currentLanguage == UiLanguage::Chinese);
    languageEnglishAction->setChecked(currentLanguage == UiLanguage::English);
}

/**
 * @brief 按当前语言填充校验位下拉选项
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::populateParityOptions()
{
    // 校验位显示文本会随语言切换，但底层 userData 固定保持英文代码，便于串口层使用。
    const QString currentCode = currentParityCode().isEmpty() ? QStringLiteral("None") : currentParityCode();

    QSignalBlocker blocker(parityComboBox);
    parityComboBox->clear();
    parityComboBox->addItem(uiText(QStringLiteral("无"), QStringLiteral("None")), QStringLiteral("None"));
    parityComboBox->addItem(uiText(QStringLiteral("奇校验"), QStringLiteral("Odd")), QStringLiteral("Odd"));
    parityComboBox->addItem(uiText(QStringLiteral("偶校验"), QStringLiteral("Even")), QStringLiteral("Even"));
    parityComboBox->addItem(uiText(QStringLiteral("标记"), QStringLiteral("Mark")), QStringLiteral("Mark"));
    parityComboBox->addItem(uiText(QStringLiteral("空格"), QStringLiteral("Space")), QStringLiteral("Space"));

    const int index = parityComboBox->findData(currentCode);
    parityComboBox->setCurrentIndex(index >= 0 ? index : 0);
}

/**
 * @brief 按当前语言填充波特率选项，并保留当前自定义值
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::populateBaudRateOptions()
{
    if (baudRateComboBox == nullptr) {
        return;
    }

    QString preferredBaud = baudRateComboBox->property("lastValidBaud").toString().trimmed();
    if (preferredBaud.isEmpty()) {
        preferredBaud = baudRateComboBox->currentText().trimmed();
    }
    if (preferredBaud.isEmpty()) {
        preferredBaud = QStringLiteral("115200");
    }

    const QStringList baudRates = {
        QStringLiteral("1200"),
        QStringLiteral("2400"),
        QStringLiteral("4800"),
        QStringLiteral("9600"),
        QStringLiteral("19200"),
        QStringLiteral("38400"),
        QStringLiteral("57600"),
        QStringLiteral("115200"),
        QStringLiteral("230400"),
        QStringLiteral("460800"),
        QStringLiteral("921600"),
        QStringLiteral("1000000"),
        QStringLiteral("1500000"),
        QStringLiteral("2000000"),
        QStringLiteral("2500000"),
        QStringLiteral("3000000")
    };

    QSignalBlocker blocker(baudRateComboBox);
    baudRateComboBox->clear();
    for (const QString &rate : baudRates) {
        baudRateComboBox->addItem(rate, rate);
    }

    bool ok = false;
    const int baudValue = preferredBaud.toInt(&ok);
    if (ok && baudValue > 0 && baudRateComboBox->findText(preferredBaud, Qt::MatchFixedString) < 0) {
        baudRateComboBox->addItem(preferredBaud, preferredBaud);
    }

    baudRateComboBox->addItem(uiText(QStringLiteral("自定义..."), QStringLiteral("Custom...")),
                              QString::fromLatin1(kCustomBaudRateToken));
    baudRateComboBox->setCurrentText(ok && baudValue > 0 ? QString::number(baudValue) : QStringLiteral("115200"));
    baudRateComboBox->setProperty("lastValidBaud",
                                  ok && baudValue > 0 ? QString::number(baudValue) : QStringLiteral("115200"));
}

/**
 * @brief 刷新串口下拉列表
 *
 * @param preferredPort: 优先选中的串口名
 *
 * @return 无
 */
void MainWindow::refreshPortList(const QString &preferredPort)
{
    const QString currentText = preferredPort.trimmed().isEmpty() ? portComboBox->currentText().trimmed()
                                                                  : preferredPort.trimmed();
    const QStringList ports = availablePorts();
    const QHash<QString, QString> friendlyNames = queryFriendlyPortNames();
    const QString preferred = extractPortName(currentText);

    QSignalBlocker blocker(portComboBox);
    portComboBox->clear();
    for (const QString &portName : ports) {
        portComboBox->addItem(buildPortDisplayText(portName, friendlyNames.value(portName)), portName);
    }

    if (!preferred.isEmpty() && portComboBox->findData(preferred) < 0) {
        portComboBox->addItem(preferred, preferred);
    }

    if (!preferred.isEmpty()) {
        const int preferredIndex = portComboBox->findData(preferred);
        if (preferredIndex >= 0) {
            portComboBox->setCurrentIndex(preferredIndex);
        } else {
            portComboBox->setEditText(preferred);
        }
    } else if (portComboBox->count() > 0) {
        portComboBox->setCurrentIndex(0);
    }

    const QString portToolTip = currentPortDisplayText(portComboBox);
    portComboBox->setToolTip(portToolTip);
    if (QLineEdit *editor = portComboBox->lineEdit()) {
        editor->setToolTip(portToolTip);
        editor->setCursorPosition(0);
    }

    if (ports.isEmpty()) {
        statusBar()->showMessage(uiText(QStringLiteral("未检测到端口，可手工输入 COM 号"),
                                        QStringLiteral("No serial ports found. You can still type a COM port manually.")),
                                 3500);
    } else {
        statusBar()->showMessage(uiText(QStringLiteral("端口列表已刷新"),
                                        QStringLiteral("Port list refreshed")),
                                 2000);
    }

    updatePortSummary();
}

/**
 * @brief 根据串口连接状态刷新界面控件可用性
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateUiState()
{
    const bool editable = !portOpen;

    portComboBox->setEnabled(editable);
    baudRateComboBox->setEnabled(editable);
    dataBitsComboBox->setEnabled(editable);
    parityComboBox->setEnabled(editable);
    stopBitsComboBox->setEnabled(editable);
    refreshPortsButton->setEnabled(editable);

    openPortButton->setText(portOpen ? uiText(QStringLiteral("断开串口"), QStringLiteral("Disconnect"))
                                     : uiText(QStringLiteral("连接串口"), QStringLiteral("Connect")));
    sendButton->setEnabled(portOpen);
    if (sendFileAction != nullptr) {
        sendFileAction->setEnabled(portOpen);
    }
    if (importNoticeActionButton != nullptr) {
        importNoticeActionButton->setEnabled(portOpen);
    }
    if (presetDock != nullptr) {
        presetDock->setTransportReady(portOpen);
    }
    if (toolboxDialog != nullptr) {
        toolboxDialog->setTransportReady(portOpen);
    }
    updateModbusMonitorState();
    rtsCheckBox->setEnabled(portOpen);
    dtrCheckBox->setEnabled(portOpen);
    updatePresetSchedulerStatus();
}

/**
 * @brief 根据复选框状态更新接收区自动换行模式
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateWrapMode()
{
    const bool wrapEnabled = wrapCheckBox != nullptr && wrapCheckBox->isChecked();
    receiveEdit->setLineWrapMode(wrapEnabled ? QPlainTextEdit::WidgetWidth
                                             : QPlainTextEdit::NoWrap);
    receiveEdit->setWordWrapMode(wrapEnabled ? QTextOption::WrapAnywhere
                                             : QTextOption::NoWrap);
}

/**
 * @brief 刷新状态栏中的串口摘要信息
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updatePortSummary()
{
    const QString portText = currentPortDisplayText(portComboBox).isEmpty()
                                 ? uiText(QStringLiteral("未选择串口"), QStringLiteral("No Port"))
                                 : currentPortDisplayText(portComboBox);
    const QString stateText = portOpen ? uiText(QStringLiteral("已连接"), QStringLiteral("Connected"))
                                       : uiText(QStringLiteral("已关闭"), QStringLiteral("Closed"));

    portStateLabel->setText(QStringLiteral("%1 %2 %3bps,%4,%5,%6")
                                .arg(portText)
                                .arg(stateText)
                                .arg(currentBaudRate())
                                .arg(currentDataBits())
                                .arg(stopBitsComboBox->currentText())
                                .arg(parityComboBox->currentText()));
}

/**
 * @brief 刷新收发字节计数显示
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateCounters()
{
    sendCountLabel->setText(QStringLiteral("S:%1").arg(totalSendBytes));
    receiveCountLabel->setText(QStringLiteral("R:%1").arg(totalReceiveBytes));
}

/**
 * @brief 根据当前状态更新循环发送定时器
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateAutoSendTimer()
{
    if (autoSendCheckBox->isChecked() && portOpen) {
        autoSendTimer->start(autoSendIntervalSpinBox->value());
    } else {
        autoSendTimer->stop();
    }
}

/**
 * @brief 将接收区滚动到最底部
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::scrollReceiveToBottom()
{
    if (QScrollBar *scrollBar = receiveEdit->verticalScrollBar()) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

/**
 * @brief 按当前显示模式把收发数据追加到日志缓冲区
 *
 * @param data: 待显示的原始字节流
 * @param outgoing: `true` 表示发送方向，`false` 表示接收方向
 * @param hexMode: `true` 表示按十六进制显示，`false` 表示按文本显示
 * @param flushPendingReceiveBeforeOutgoing: 发送日志前是否把主接收区超时分包缓存刷出
 *
 * @return 无
 */
void MainWindow::appendTrafficLog(const QByteArray &data,
                                  bool outgoing,
                                  bool hexMode,
                                  bool flushPendingReceiveBeforeOutgoing)
{
    if (data.isEmpty()) {
        return;
    }

    if (outgoing && flushPendingReceiveBeforeOutgoing && !pendingReceivePacket.isEmpty()) {
        // 轮询发送会不断产生新的发送边界，先刷新上一包接收数据，避免超时分包一直续期不显示。
        flushPendingReceivePacket();
    }

    if (timestampCheckBox->isChecked()) {
        QString displayText = trafficPayloadText(data, hexMode);
        if (!hexMode) {
            displayText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            displayText.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        }

        QStringList lines = hexMode ? QStringList{displayText}
                                    : displayText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        if (!hexMode && displayText.endsWith(QLatin1Char('\n')) && !lines.isEmpty()) {
            lines.removeLast();
        }
        if (lines.isEmpty()) {
            lines << QString();
        }

        QStringList formattedLines;
        const QString prefix = QStringLiteral("[%1] %2")
                                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
                                   .arg(trafficPrefix(outgoing));
        for (const QString &line : lines) {
            formattedLines << (prefix + line);
        }

        QString chunk = formattedLines.join(QLatin1Char('\n'));
        if (!receiveLogBuffer.isEmpty() && !receiveLogBuffer.endsWith(QLatin1Char('\n'))) {
            chunk.prepend(QLatin1Char('\n'));
        }
        chunk.append(QLatin1Char('\n'));
        appendReceiveChunk(chunk);
        return;
    }

    if (outgoing) {
        QString displayText = trafficPayloadText(data, hexMode);
        if (!hexMode) {
            displayText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            displayText.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        }

        QStringList lines = hexMode ? QStringList{displayText}
                                    : displayText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        if (!hexMode && displayText.endsWith(QLatin1Char('\n')) && !lines.isEmpty()) {
            lines.removeLast();
        }
        if (lines.isEmpty()) {
            lines << QString();
        }

        QStringList formattedLines;
        const QString prefix = trafficPrefix(true);
        for (const QString &line : lines) {
            formattedLines << (prefix + line);
        }

        QString chunk = formattedLines.join(QLatin1Char('\n'));
        if (!receiveLogBuffer.isEmpty() && !receiveLogBuffer.endsWith(QLatin1Char('\n'))) {
            // 关闭时间戳时接收区可能是原始接收流，发送帧需要另起一行避免和接收字节粘在一起。
            chunk.prepend(QLatin1Char('\n'));
        }
        chunk.append(QLatin1Char('\n'));
        appendReceiveChunk(chunk);
        return;
    }

    QString chunk = trafficPayloadText(data, hexMode);
    if (hexMode
        && !receiveLogBuffer.isEmpty()
        && !receiveLogBuffer.endsWith(QLatin1Char(' '))
        && !receiveLogBuffer.endsWith(QLatin1Char('\n'))) {
        chunk.prepend(QLatin1Char(' '));
    }
    appendReceiveChunk(chunk);
}

/**
 * @brief 刷新超时分包缓冲区中的接收数据
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::flushPendingReceivePacket()
{
    receivePacketTimer->stop();
    if (pendingReceivePacket.isEmpty()) {
        return;
    }
    if (pauseDisplayCheckBox->isChecked()) {
        pendingReceivePacket.clear();
        return;
    }

    const QByteArray packet = pendingReceivePacket;
    pendingReceivePacket.clear();
    appendTrafficLog(packet, false, hexDisplayCheckBox->isChecked());
}

/**
 * @brief 将新的接收文本片段追加到内部显示缓冲
 *
 * @param chunk: 已经格式化好的显示文本
 *
 * @return 无
 */
void MainWindow::appendReceiveChunk(const QString &chunk)
{
    if (chunk.isEmpty()) {
        return;
    }

    receiveLogBuffer.append(chunk);
    if (receiveLogBuffer.size() > kLiveViewLimit) {
        receiveLogBuffer.remove(0, receiveLogBuffer.size() - int(kLiveViewLimit));
        const int firstBreak = receiveLogBuffer.indexOf(QLatin1Char('\n'));
        if (firstBreak > 0) {
            receiveLogBuffer.remove(0, firstBreak + 1);
        }
    }

    writeLiveCaptureChunk(chunk);
    refreshReceiveView();
}

/**
 * @brief 根据筛选条件重建接收区显示内容
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::refreshReceiveView()
{
    QString displayText = receiveLogBuffer;
    const bool filterActive = !receiveFilterKeyword.isEmpty();
    if (!receiveFilterKeyword.isEmpty()) {
        const QStringList lines = displayText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        QStringList filteredLines;
        for (const QString &line : lines) {
            if (line.contains(receiveFilterKeyword, Qt::CaseInsensitive)) {
                filteredLines << line;
            }
        }
        displayText = filteredLines.join(QLatin1Char('\n'));
    }

    if (receiveSectionLabel != nullptr) {
        receiveSectionLabel->setText(filterActive
                                         ? uiText(QStringLiteral("接收监视（筛选中）"),
                                                  QStringLiteral("Receive Panel (Filtered)"))
                                         : uiText(QStringLiteral("接收监视"), QStringLiteral("Receive Panel")));
    }
    receiveEdit->setPlaceholderText(
        filterActive
            ? uiText(QStringLiteral("筛选关键字“%1”没有匹配内容；使用“视图 > 清除筛选”可恢复全部显示。")
                         .arg(receiveFilterKeyword),
                     QStringLiteral("No content matches filter \"%1\". Use View > Clear Filter to show all.")
                         .arg(receiveFilterKeyword))
            : uiText(QStringLiteral("接收监视区"), QStringLiteral("Receive monitor")));
    receiveEdit->setPlainText(displayText);
    applyReceiveHighlights();
    scrollReceiveToBottom();
}

/**
 * @brief 对接收区中命中的关键字执行高亮
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::applyReceiveHighlights()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (!receiveHighlightKeyword.isEmpty()) {
        QTextCursor cursor(receiveEdit->document());
        QTextCharFormat format;
        format.setBackground(QColor(QStringLiteral("#fff59d")));
        format.setForeground(QColor(QStringLiteral("#102742")));

        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = receiveEdit->document()->find(receiveHighlightKeyword, cursor, QTextDocument::FindFlags());
            if (cursor.isNull()) {
                break;
            }

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format = format;
            selections << selection;
        }
    }

    receiveEdit->setExtraSelections(selections);
}

/**
 * @brief 统一更新收发编辑区字号
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateConsoleFont()
{
    QFont consoleFont = defaultConsoleFont();
    if (!consoleFontFamily.trimmed().isEmpty()) {
        consoleFont.setFamily(consoleFontFamily.trimmed());
    }
    consoleFont.setPointSize(qBound(9, consoleFontPointSize, 72));

    receiveEdit->setFont(consoleFont);
    sendEdit->setFont(consoleFont);
}

/**
 * @brief 刷新停靠面板及其联动文本
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateDockTexts()
{
    if (presetDock != nullptr) {
        presetDock->setChineseMode(currentLanguage == UiLanguage::Chinese);
    }
    if (toolboxDialog != nullptr) {
        toolboxDialog->setChineseMode(currentLanguage == UiLanguage::Chinese);
    }
    updatePresetSchedulerStatus();
}

/**
 * @brief 刷新多字符串轮询发送器的状态栏文本
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updatePresetSchedulerStatus()
{
    if (presetDockToggleButton == nullptr || presetSchedulerStatusLabel == nullptr || presetDock == nullptr) {
        return;
    }

    presetDockToggleButton->setChecked(presetDock->isVisible());

    const int activeCount = presetDock->activePresetCount();
    if (activeCount <= 0) {
        presetSchedulerStatusLabel->setVisible(false);
        return;
    }

    presetSchedulerStatusLabel->setVisible(true);
    if (!presetDock->isTransportReady()) {
        presetSchedulerStatusLabel->setText(
            uiText(QStringLiteral("多字符串 %1 条待发").arg(activeCount),
                   QStringLiteral("Multi String %1 armed").arg(activeCount)));
        return;
    }

    presetSchedulerStatusLabel->setText(
        uiText(QStringLiteral("多字符串运行中 %1 条").arg(activeCount),
               QStringLiteral("Multi String running %1").arg(activeCount)));
}

/**
 * @brief 构建 Modbus 实时监视停靠表格
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::buildModbusMonitorDock()
{
    modbusMonitorDock = new QDockWidget(this);
    modbusMonitorDock->setObjectName(QStringLiteral("modbusMonitorDock"));
    modbusMonitorDock->setFeatures(QDockWidget::DockWidgetClosable
                                   | QDockWidget::DockWidgetMovable
                                   | QDockWidget::DockWidgetFloatable);

    auto *container = new QWidget(modbusMonitorDock);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *requestLayout = new QVBoxLayout;
    requestLayout->setContentsMargins(0, 0, 0, 0);
    requestLayout->setSpacing(6);
    auto *topRequestLayout = new QHBoxLayout;
    topRequestLayout->setContentsMargins(0, 0, 0, 0);
    topRequestLayout->setSpacing(8);
    auto *writeRequestLayout = new QHBoxLayout;
    writeRequestLayout->setContentsMargins(0, 0, 0, 0);
    writeRequestLayout->setSpacing(6);
    const auto makeModbusField = [container](QLabel *label, QWidget *editor) {
        auto *field = new QWidget(container);
        auto *fieldLayout = new QHBoxLayout(field);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(4);
        // 字段组整体也固定到内容宽度，避免父级行布局拉宽后在标签和输入框附近出现空白。
        field->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        editor->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        fieldLayout->addWidget(label);
        fieldLayout->addWidget(editor);
        return field;
    };

    auto *slaveLabel = new QLabel(container);
    slaveLabel->setObjectName(QStringLiteral("modbusMonitorSlaveLabel"));
    modbusMonitorSlaveSpinBox = new QSpinBox(container);
    modbusMonitorSlaveSpinBox->setRange(1, 247);
    modbusMonitorSlaveSpinBox->setValue(1);
    setCompactInputWidth(modbusMonitorSlaveSpinBox, 64, 78);

    auto *functionLabel = new QLabel(container);
    functionLabel->setObjectName(QStringLiteral("modbusMonitorFunctionLabel"));
    modbusMonitorFunctionComboBox = new QComboBox(container);
    modbusMonitorFunctionComboBox->addItem(QStringLiteral("03 Read Holding Registers"), 0x03);
    modbusMonitorFunctionComboBox->addItem(QStringLiteral("04 Read Input Registers"), 0x04);
    modbusMonitorFunctionComboBox->addItem(QStringLiteral("06 Write Single Register"), 0x06);
    modbusMonitorFunctionComboBox->addItem(QStringLiteral("16 Write Multiple Registers"), 0x10);
    setCompactInputWidth(modbusMonitorFunctionComboBox, 168, 188);

    auto *addressLabel = new QLabel(container);
    addressLabel->setObjectName(QStringLiteral("modbusMonitorAddressLabel"));
    modbusMonitorStartAddressSpinBox = new QSpinBox(container);
    modbusMonitorStartAddressSpinBox->setRange(0, 65535);
    modbusMonitorStartAddressSpinBox->setValue(0);
    setCompactInputWidth(modbusMonitorStartAddressSpinBox, 94, 108);

    auto *quantityLabel = new QLabel(container);
    quantityLabel->setObjectName(QStringLiteral("modbusMonitorQuantityLabel"));
    modbusMonitorQuantitySpinBox = new QSpinBox(container);
    modbusMonitorQuantitySpinBox->setRange(1, 125);
    modbusMonitorQuantitySpinBox->setValue(10);
    setCompactInputWidth(modbusMonitorQuantitySpinBox, 82, 96);

    auto *formatLabel = new QLabel(container);
    formatLabel->setObjectName(QStringLiteral("modbusMonitorFormatLabel"));
    modbusMonitorFormatComboBox = new QComboBox(container);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("Unsigned Decimal"), kModbusFormatUnsigned);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("Signed Decimal"), kModbusFormatSigned);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("HEX"), kModbusFormatHex);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("Binary"), kModbusFormatBinary);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("Float32 AB CD"), kModbusFormatFloat32);
    modbusMonitorFormatComboBox->addItem(QStringLiteral("Float32 CD AB"), kModbusFormatFloat32WordSwap);
    setCompactInputWidth(modbusMonitorFormatComboBox, 150, 190);

    auto *writeValuesLabel = new QLabel(container);
    writeValuesLabel->setObjectName(QStringLiteral("modbusMonitorWriteValuesLabel"));
    modbusMonitorWriteValuesEdit = new QLineEdit(container);
    modbusMonitorWriteValuesEdit->setPlaceholderText(QStringLiteral("0x0001, 2, 3"));

    topRequestLayout->addWidget(makeModbusField(slaveLabel, modbusMonitorSlaveSpinBox));
    topRequestLayout->addWidget(makeModbusField(functionLabel, modbusMonitorFunctionComboBox));
    topRequestLayout->addWidget(makeModbusField(addressLabel, modbusMonitorStartAddressSpinBox));
    topRequestLayout->addWidget(makeModbusField(quantityLabel, modbusMonitorQuantitySpinBox));
    topRequestLayout->setAlignment(Qt::AlignLeft);
    writeRequestLayout->addWidget(makeModbusField(formatLabel, modbusMonitorFormatComboBox));
    writeRequestLayout->addWidget(writeValuesLabel);
    writeRequestLayout->addWidget(modbusMonitorWriteValuesEdit, 1);
    requestLayout->addLayout(topRequestLayout);
    requestLayout->addLayout(writeRequestLayout);

    auto *controlLayout = new QHBoxLayout;
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(6);

    auto *intervalLabel = new QLabel(container);
    intervalLabel->setObjectName(QStringLiteral("modbusMonitorIntervalLabel"));
    modbusMonitorIntervalSpinBox = new QSpinBox(container);
    modbusMonitorIntervalUnitLabel = new QLabel(container);
    modbusMonitorIntervalSpinBox->setRange(100, 60000);
    modbusMonitorIntervalSpinBox->setValue(1000);
    setCompactInputWidth(modbusMonitorIntervalSpinBox, 96, 112);
    modbusMonitorIntervalUnitLabel->setText(QStringLiteral("ms"));
    modbusMonitorStartButton = new QPushButton(container);
    modbusMonitorStartButton->setCheckable(true);
    modbusMonitorPollButton = new QPushButton(container);
    modbusMonitorClearButton = new QPushButton(container);
    modbusMonitorStatusLabel = new QLabel(container);
    modbusMonitorStatusLabel->setObjectName(QStringLiteral("modbusMonitorStatus"));
    modbusMonitorStatusLabel->setWordWrap(false);

    controlLayout->addWidget(intervalLabel);
    controlLayout->addWidget(modbusMonitorIntervalSpinBox);
    controlLayout->addWidget(modbusMonitorIntervalUnitLabel);
    controlLayout->addWidget(modbusMonitorStartButton);
    controlLayout->addWidget(modbusMonitorPollButton);
    controlLayout->addWidget(modbusMonitorClearButton);
    controlLayout->addWidget(modbusMonitorStatusLabel, 1);

    modbusRegisterTable = new QTableWidget(container);
    modbusRegisterTable->setColumnCount(6);
    modbusRegisterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    modbusRegisterTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    modbusRegisterTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    modbusRegisterTable->setContextMenuPolicy(Qt::CustomContextMenu);
    modbusRegisterTable->setAlternatingRowColors(true);
    modbusRegisterTable->verticalHeader()->setVisible(false);
    modbusRegisterTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    modbusRegisterTable->verticalHeader()->setDefaultSectionSize(kModbusTableRowHeight);
    modbusRegisterTable->verticalHeader()->setMinimumSectionSize(kModbusTableMinimumRowHeight);
    modbusRegisterTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    modbusRegisterTable->setMinimumHeight(kModbusTableMinimumHeight);
    QPalette tablePalette = modbusRegisterTable->palette();
    tablePalette.setColor(QPalette::Base, QColor(QStringLiteral("#f8fbff")));
    tablePalette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#edf4ff")));
    modbusRegisterTable->setPalette(tablePalette);
    configureModbusMonitorTableColumns(modbusRegisterTable);

    auto *logLabel = new QLabel(container);
    logLabel->setObjectName(QStringLiteral("modbusMonitorLogLabel"));
    modbusMonitorTimestampCheckBox = new QCheckBox(container);
    modbusMonitorPacketSplitCheckBox = new QCheckBox(container);
    modbusMonitorTimeoutLabel = new QLabel(container);
    modbusMonitorTimeoutSpinBox = new QSpinBox(container);
    modbusMonitorTimeoutUnitLabel = new QLabel(container);
    modbusMonitorTimeoutSpinBox->setRange(1, 5000);
    modbusMonitorTimestampCheckBox->setChecked(true);
    modbusMonitorTimeoutSpinBox->setValue(50);
    setCompactInputWidth(modbusMonitorTimeoutSpinBox, 82, 96);
    modbusMonitorTimeoutUnitLabel->setText(QStringLiteral("ms"));
    modbusMonitorLogEdit = new QPlainTextEdit(container);
    modbusMonitorLogEdit->setReadOnly(true);
    modbusMonitorLogEdit->setMaximumBlockCount(1000);
    modbusMonitorLogEdit->setMinimumHeight(170);

    modbusMonitorSplitter = new QSplitter(Qt::Vertical, container);
    modbusMonitorSplitter->setObjectName(QStringLiteral("modbusMonitorSplitter"));
    modbusMonitorSplitter->addWidget(modbusRegisterTable);
    auto *logPanel = new QWidget(modbusMonitorSplitter);
    auto *logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(4);
    logLayout->addWidget(logLabel);
    auto *logOptionsLayout = new QHBoxLayout;
    logOptionsLayout->setContentsMargins(0, 0, 0, 0);
    logOptionsLayout->setSpacing(6);
    logOptionsLayout->addWidget(modbusMonitorTimestampCheckBox);
    logOptionsLayout->addWidget(modbusMonitorPacketSplitCheckBox);
    logOptionsLayout->addWidget(modbusMonitorTimeoutLabel);
    logOptionsLayout->addWidget(modbusMonitorTimeoutSpinBox);
    logOptionsLayout->addWidget(modbusMonitorTimeoutUnitLabel);
    logOptionsLayout->addStretch();
    logLayout->addLayout(logOptionsLayout);
    logLayout->addWidget(modbusMonitorLogEdit, 1);
    modbusMonitorSplitter->addWidget(logPanel);
    modbusMonitorSplitter->setStretchFactor(0, 2);
    modbusMonitorSplitter->setStretchFactor(1, 3);
    modbusMonitorSplitter->setCollapsible(0, false);
    modbusMonitorSplitter->setCollapsible(1, true);
    modbusMonitorSplitter->setSizes({300, 260});

    layout->addLayout(requestLayout);
    layout->addLayout(controlLayout);
    layout->addWidget(modbusMonitorSplitter, 1);
    modbusMonitorDock->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, modbusMonitorDock);
    modbusMonitorDock->setAllowedAreas(Qt::NoDockWidgetArea);
    modbusMonitorDock->setFloating(true);
    modbusMonitorDock->resize(680, 680);
    modbusMonitorDock->setMinimumSize(600, 500);
    modbusMonitorDock->hide();
    modbusMonitorAction = modbusMonitorDock->toggleViewAction();

    connect(modbusMonitorTimer, &QTimer::timeout, this, &MainWindow::sendModbusMonitorRequest);
    connect(modbusMonitorPacketTimer, &QTimer::timeout, this, &MainWindow::flushPendingModbusMonitorLogPacket);
    connect(modbusMonitorTimestampCheckBox, &QCheckBox::toggled, this, [this](bool) {
        flushPendingModbusMonitorLogPacket();
        saveDisplaySettings();
    });
    connect(modbusMonitorPacketSplitCheckBox, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!enabled) {
            flushPendingModbusMonitorLogPacket();
        }
        updateModbusMonitorLogOptionState();
        saveDisplaySettings();
    });
    connect(modbusMonitorTimeoutSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (modbusMonitorPacketTimer != nullptr && modbusMonitorPacketTimer->isActive()) {
                    modbusMonitorPacketTimer->start(value);
                }
                saveDisplaySettings();
            });
    connect(modbusRegisterTable,
            &QTableWidget::customContextMenuRequested,
            this,
            &MainWindow::showModbusMonitorFormatMenu);
    updateModbusMonitorLogOptionState();
    connect(modbusMonitorStartButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            sendModbusMonitorRequest();
            modbusMonitorTimer->start(modbusMonitorIntervalSpinBox->value());
        } else {
            modbusMonitorTimer->stop();
            cancelModbusMonitorReceiveSession(true);
        }
        updateModbusMonitorState();
    });
    connect(modbusMonitorPollButton, &QPushButton::clicked, this, &MainWindow::sendModbusMonitorRequest);
    connect(modbusMonitorClearButton, &QPushButton::clicked, this, &MainWindow::resetModbusMonitorState);
    connect(modbusMonitorIntervalSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        if (modbusMonitorTimer->isActive()) {
            modbusMonitorTimer->start(value);
        }
    });
    const auto refreshRows = [this] {
        updateModbusMonitorRows();
        cancelModbusMonitorReceiveSession(true);
    };
    connect(modbusMonitorSlaveSpinBox, &QSpinBox::valueChanged, this, refreshRows);
    connect(modbusMonitorFunctionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        updateModbusMonitorFunctionState();
        updateModbusMonitorRows();
        modbusMonitorBuffer.clear();
    });
    connect(modbusMonitorStartAddressSpinBox, &QSpinBox::valueChanged, this, refreshRows);
    connect(modbusMonitorQuantitySpinBox, &QSpinBox::valueChanged, this, refreshRows);
    connect(modbusMonitorFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        refreshModbusMonitorTableValues();
        saveDisplaySettings();
    });

    updateModbusMonitorTexts();
    updateModbusMonitorFunctionState();
    updateModbusMonitorRows();
    updateModbusMonitorState();
}

/**
 * @brief 刷新 Modbus 实时监视窗口文本
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateModbusMonitorTexts()
{
    if (modbusMonitorDock == nullptr) {
        return;
    }

    modbusMonitorDock->setWindowTitle(uiText(QStringLiteral("Modbus 读写工具"),
                                             QStringLiteral("Modbus Read / Write Tool")));
    if (modbusMonitorAction != nullptr) {
        modbusMonitorAction->setText(uiText(QStringLiteral("Modbus 读写工具"),
                                            QStringLiteral("Modbus Read / Write Tool")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorSlaveLabel"))) {
        label->setText(uiText(QStringLiteral("站号"), QStringLiteral("Slave")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorFunctionLabel"))) {
        label->setText(uiText(QStringLiteral("功能码"), QStringLiteral("Function")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorAddressLabel"))) {
        label->setText(uiText(QStringLiteral("起始地址"), QStringLiteral("Start")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorQuantityLabel"))) {
        label->setText(uiText(QStringLiteral("数量"), QStringLiteral("Qty")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorIntervalLabel"))) {
        label->setText(uiText(QStringLiteral("扫描间隔"), QStringLiteral("Scan Interval")));
    }
    if (modbusMonitorIntervalUnitLabel != nullptr) {
        modbusMonitorIntervalUnitLabel->setText(QStringLiteral("ms"));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorFormatLabel"))) {
        label->setText(uiText(QStringLiteral("默认格式"), QStringLiteral("Default Format")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorWriteValuesLabel"))) {
        label->setText(uiText(QStringLiteral("写入值"), QStringLiteral("Write Values")));
    }
    if (auto *label = modbusMonitorDock->findChild<QLabel *>(QStringLiteral("modbusMonitorLogLabel"))) {
        label->setText(uiText(QStringLiteral("原始收发日志"), QStringLiteral("Raw TX/RX Log")));
    }
    if (modbusMonitorTimestampCheckBox != nullptr) {
        modbusMonitorTimestampCheckBox->setText(uiText(QStringLiteral("时间戳"), QStringLiteral("Timestamp")));
    }
    if (modbusMonitorPacketSplitCheckBox != nullptr) {
        modbusMonitorPacketSplitCheckBox->setText(uiText(QStringLiteral("超时分包显示"),
                                                        QStringLiteral("Split By Timeout")));
    }
    if (modbusMonitorTimeoutLabel != nullptr) {
        modbusMonitorTimeoutLabel->setText(uiText(QStringLiteral("超时"), QStringLiteral("Timeout")));
    }
    if (modbusMonitorTimeoutUnitLabel != nullptr) {
        modbusMonitorTimeoutUnitLabel->setText(QStringLiteral("ms"));
    }

    if (modbusMonitorFunctionComboBox != nullptr) {
        const int currentCode = modbusMonitorFunctionComboBox->currentData().toInt();
        QSignalBlocker blocker(modbusMonitorFunctionComboBox);
        modbusMonitorFunctionComboBox->clear();
        modbusMonitorFunctionComboBox->addItem(uiText(QStringLiteral("03 读保持寄存器"),
                                                     QStringLiteral("03 Read Holding Registers")),
                                               0x03);
        modbusMonitorFunctionComboBox->addItem(uiText(QStringLiteral("04 读输入寄存器"),
                                                     QStringLiteral("04 Read Input Registers")),
                                               0x04);
        modbusMonitorFunctionComboBox->addItem(uiText(QStringLiteral("06 写单个寄存器"),
                                                     QStringLiteral("06 Write Single Register")),
                                               0x06);
        modbusMonitorFunctionComboBox->addItem(uiText(QStringLiteral("16 写多个寄存器"),
                                                     QStringLiteral("16 Write Multiple Registers")),
                                               0x10);
        modbusMonitorFunctionComboBox->setCurrentIndex(qMax(0, modbusMonitorFunctionComboBox->findData(currentCode)));
    }
    if (modbusMonitorFormatComboBox != nullptr) {
        const int currentFormat = modbusMonitorFormatComboBox->currentData().toInt();
        QSignalBlocker blocker(modbusMonitorFormatComboBox);
        modbusMonitorFormatComboBox->clear();
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatUnsigned), kModbusFormatUnsigned);
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatSigned), kModbusFormatSigned);
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatHex), kModbusFormatHex);
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatBinary), kModbusFormatBinary);
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatFloat32), kModbusFormatFloat32);
        modbusMonitorFormatComboBox->addItem(modbusMonitorFormatName(kModbusFormatFloat32WordSwap),
                                             kModbusFormatFloat32WordSwap);
        modbusMonitorFormatComboBox->setCurrentIndex(qMax(0, modbusMonitorFormatComboBox->findData(currentFormat)));
    }

    modbusRegisterTable->setHorizontalHeaderLabels({
        uiText(QStringLiteral("地址"), QStringLiteral("Address")),
        uiText(QStringLiteral("原始值"), QStringLiteral("Raw")),
        uiText(QStringLiteral("解析值"), QStringLiteral("Value")),
        uiText(QStringLiteral("写入值"), QStringLiteral("Write")),
        uiText(QStringLiteral("更新时间"), QStringLiteral("Updated")),
        uiText(QStringLiteral("状态"), QStringLiteral("Status")),
    });
    configureModbusMonitorTableColumns(modbusRegisterTable);
    refreshModbusMonitorTableValues();
    updateModbusMonitorFunctionState();
    updateModbusMonitorState();
}

/**
 * @brief 刷新 Modbus 实时监视控件状态
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateModbusMonitorState()
{
    if (modbusMonitorStartButton == nullptr) {
        return;
    }

    bool running = modbusMonitorTimer != nullptr && modbusMonitorTimer->isActive();
    if (!portOpen && running) {
        modbusMonitorTimer->stop();
        cancelModbusMonitorReceiveSession(true);
        QSignalBlocker blocker(modbusMonitorStartButton);
        modbusMonitorStartButton->setChecked(false);
        running = false;
    }

    const int functionCode = modbusMonitorFunctionComboBox->currentData().toInt();
    const bool readFunction = isModbusReadFunction(functionCode);
    if (!readFunction && running) {
        modbusMonitorTimer->stop();
        cancelModbusMonitorReceiveSession(true);
        QSignalBlocker blocker(modbusMonitorStartButton);
        modbusMonitorStartButton->setChecked(false);
        running = false;
    }
    modbusMonitorStartButton->setText(running ? uiText(QStringLiteral("停止扫描"), QStringLiteral("Stop Scan"))
                                               : uiText(QStringLiteral("开始扫描"), QStringLiteral("Start Scan")));
    modbusMonitorPollButton->setText(readFunction ? uiText(QStringLiteral("单次读取"), QStringLiteral("Read Once"))
                                                   : uiText(QStringLiteral("写入寄存器"), QStringLiteral("Write")));
    modbusMonitorClearButton->setText(uiText(QStringLiteral("清空数据"), QStringLiteral("Clear")));
    modbusMonitorPollButton->setEnabled(portOpen);
    modbusMonitorStartButton->setEnabled(portOpen && readFunction);
    if (modbusMonitorIntervalUnitLabel != nullptr) {
        modbusMonitorIntervalUnitLabel->setEnabled(readFunction);
    }

    const QString statusText = portOpen
        ? uiText(QStringLiteral("响应 %1 次，错误 %2 次").arg(modbusMonitorResponseCount).arg(modbusMonitorErrorCount),
                 QStringLiteral("Responses %1, errors %2").arg(modbusMonitorResponseCount).arg(modbusMonitorErrorCount))
        : uiText(QStringLiteral("请先连接串口"), QStringLiteral("Connect serial port first"));
    modbusMonitorStatusLabel->setText(statusText);
}

void MainWindow::updateModbusMonitorFunctionState()
{
    if (modbusMonitorFunctionComboBox == nullptr) {
        return;
    }

    const int functionCode = modbusMonitorFunctionComboBox->currentData().toInt();
    const bool readFunction = isModbusReadFunction(functionCode);
    const bool multiWrite = functionCode == 0x10;
    if (functionCode == 0x06) {
        modbusMonitorQuantitySpinBox->setValue(1);
    }
    modbusMonitorQuantitySpinBox->setEnabled(functionCode != 0x06);
    modbusMonitorQuantitySpinBox->setRange(1, readFunction ? 125 : (multiWrite ? 123 : 1));
    modbusMonitorIntervalSpinBox->setEnabled(readFunction);
    modbusMonitorWriteValuesEdit->setEnabled(isModbusWriteFunction(functionCode));
    if (modbusMonitorWriteValuesEdit->isEnabled()) {
        modbusMonitorWriteValuesEdit->setPlaceholderText(
            functionCode == 0x06 ? uiText(QStringLiteral("例如：0x1234 或 4660"),
                                          QStringLiteral("Example: 0x1234 or 4660"))
                                 : uiText(QStringLiteral("例如：1, 0x0002, 3"),
                                          QStringLiteral("Example: 1, 0x0002, 3")));
    } else {
        modbusMonitorWriteValuesEdit->setPlaceholderText(uiText(QStringLiteral("写寄存器时填写"),
                                                                QStringLiteral("Used by write functions")));
    }
    updateModbusMonitorState();
}

/**
 * @brief 按当前查询参数刷新寄存器表格行
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateModbusMonitorRows()
{
    if (modbusRegisterTable == nullptr) {
        return;
    }

    const int startAddress = modbusMonitorStartAddressSpinBox->value();
    const int quantity = modbusMonitorQuantitySpinBox->value();
    QSignalBlocker blocker(modbusRegisterTable);
    modbusRegisterTable->setRowCount(quantity);
    modbusMonitorRegisterValues.fill(0, quantity);
    modbusMonitorRegisterValid.fill(false, quantity);
    for (int row = 0; row < quantity; ++row) {
        const int address = startAddress + row;
        modbusRegisterTable->setRowHeight(row, kModbusTableRowHeight);
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::number(address));
        addressItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        modbusRegisterTable->setItem(row, kModbusAddressColumn, addressItem);

        for (int column = 1; column < modbusRegisterTable->columnCount(); ++column) {
            QTableWidgetItem *item = new QTableWidgetItem(column == kModbusStatusColumn
                                                              ? uiText(QStringLiteral("等待数据"),
                                                                       QStringLiteral("Waiting"))
                                                              : QStringLiteral("--"));
            item->setTextAlignment(column == kModbusStatusColumn ? Qt::AlignLeft | Qt::AlignVCenter
                                                                 : Qt::AlignRight | Qt::AlignVCenter);
            modbusRegisterTable->setItem(row, column, item);
        }
    }
    configureModbusMonitorTableColumns(modbusRegisterTable);
}

void MainWindow::refreshModbusMonitorTableValues()
{
    if (modbusRegisterTable == nullptr) {
        return;
    }

    for (int row = 0; row < modbusRegisterTable->rowCount(); ++row) {
        if (QTableWidgetItem *rawItem = modbusRegisterTable->item(row, kModbusRawColumn)) {
            rawItem->setText(row < modbusMonitorRegisterValid.size() && modbusMonitorRegisterValid.at(row)
                                 ? formatWordHex(modbusMonitorRegisterValues.at(row))
                                 : QStringLiteral("--"));
        }
        if (QTableWidgetItem *valueItem = modbusRegisterTable->item(row, kModbusValueColumn)) {
            const int address = modbusMonitorStartAddressSpinBox->value() + row;
            const int format = modbusMonitorFormatForRow(row);
            const bool hasOverride = modbusMonitorAddressFormats.contains(address);
            QFont itemFont = valueItem->font();
            itemFont.setBold(hasOverride);
            valueItem->setFont(itemFont);
            valueItem->setText(formatModbusMonitorValue(row));
            valueItem->setToolTip(hasOverride
                                      ? uiText(QStringLiteral("当前地址单独解析为：%1").arg(modbusMonitorFormatName(format)),
                                               QStringLiteral("This address uses: %1").arg(modbusMonitorFormatName(format)))
                                      : uiText(QStringLiteral("跟随默认格式：%1").arg(modbusMonitorFormatName(format)),
                                               QStringLiteral("Uses default format: %1").arg(modbusMonitorFormatName(format))));
        }
        if (QTableWidgetItem *writeItem = modbusRegisterTable->item(row, kModbusWriteColumn)) {
            writeItem->setText(row < modbusMonitorLastWriteValues.size()
                                   ? formatWordHex(modbusMonitorLastWriteValues.at(row))
                                   : QStringLiteral("--"));
        }
    }
    configureModbusMonitorTableColumns(modbusRegisterTable);
}

/**
 * @brief 发送一次 Modbus 实时监视查询帧
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::sendModbusMonitorRequest()
{
    if (!portOpen) {
        cancelModbusMonitorReceiveSession(true);
        updateModbusMonitorState();
        return;
    }

    QString errorMessage;
    const QVector<quint16> writeValues = parseModbusMonitorWriteValues(&errorMessage);
    if (!errorMessage.isEmpty()) {
        cancelModbusMonitorReceiveSession(true);
        statusBar()->showMessage(errorMessage, 4000);
        ++modbusMonitorErrorCount;
        updateModbusMonitorState();
        return;
    }

    const QByteArray request = buildModbusMonitorRequest(writeValues);
    if (request.isEmpty()) {
        cancelModbusMonitorReceiveSession(true);
        return;
    }

    cancelModbusMonitorReceiveSession(false);
    modbusMonitorRequestSlave = modbusMonitorSlaveSpinBox->value();
    modbusMonitorRequestFunction = modbusMonitorFunctionComboBox->currentData().toInt();
    modbusMonitorRequestStartAddress = modbusMonitorStartAddressSpinBox->value();
    modbusMonitorRequestQuantity = modbusMonitorQuantitySpinBox->value();
    modbusMonitorLastWriteValues = writeValues;
    modbusMonitorBuffer.clear();
    modbusMonitorAwaitingResponse = true;
    const int modbusTimeoutMs = modbusMonitorTimeoutSpinBox != nullptr ? modbusMonitorTimeoutSpinBox->value() : 50;
    const int responseTimeoutMs = qMax(1000, modbusTimeoutMs * 4);
    modbusMonitorResponseTimer->start(responseTimeoutMs);

    appendModbusMonitorLog(request, true);
    // Modbus 扫描请求只是工具侧轮询边界，不能提前冲掉主接收区正在等待超时聚合的 RX 缓冲。
    appendTrafficLog(request, true, true, false);
    emit requestWriteData(request);
}

/**
 * @brief 按显示设置追加 Modbus 原始收发日志
 *
 * @param data: 需要显示的原始字节流
 * @param outgoing: `true` 表示发送方向，`false` 表示接收方向
 *
 * @return 无
 */
void MainWindow::appendModbusMonitorLog(const QByteArray &data, bool outgoing)
{
    if (modbusMonitorLogEdit == nullptr || data.isEmpty()) {
        return;
    }

    if (outgoing) {
        flushPendingModbusMonitorLogPacket();
        appendModbusMonitorLogLine(data, outgoing);
        return;
    }

    if (modbusMonitorPacketSplitCheckBox != nullptr && modbusMonitorPacketSplitCheckBox->isChecked()) {
        pendingModbusMonitorLogPacket.append(data);
        modbusMonitorPacketTimer->start(modbusMonitorTimeoutSpinBox != nullptr ? modbusMonitorTimeoutSpinBox->value() : 50);
        return;
    }

    appendModbusMonitorLogLine(data, outgoing);
}

/**
 * @brief 刷新 Modbus 原始日志中等待超时聚合的接收数据
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::flushPendingModbusMonitorLogPacket()
{
    if (modbusMonitorPacketTimer != nullptr) {
        modbusMonitorPacketTimer->stop();
    }
    if (pendingModbusMonitorLogPacket.isEmpty()) {
        return;
    }

    const QByteArray packet = pendingModbusMonitorLogPacket;
    pendingModbusMonitorLogPacket.clear();
    appendModbusMonitorLogLine(packet, false);
}

/**
 * @brief 按 Modbus 日志选项追加一条原始收发日志
 *
 * @param data: 需要显示的原始字节流
 * @param outgoing: `true` 表示发送方向，`false` 表示接收方向
 *
 * @return 无
 */
void MainWindow::appendModbusMonitorLogLine(const QByteArray &data, bool outgoing)
{
    if (modbusMonitorLogEdit == nullptr || data.isEmpty()) {
        return;
    }

    const bool timestampEnabled = modbusMonitorTimestampCheckBox == nullptr || modbusMonitorTimestampCheckBox->isChecked();
    const QString prefix = timestampEnabled
                               ? QStringLiteral("[%1] %2")
                                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
                                     .arg(trafficPrefix(outgoing))
                               : trafficPrefix(outgoing);
    const QString line = prefix + QString::fromLatin1(data.toHex(' ').toUpper());
    modbusMonitorLogEdit->appendPlainText(line);
}

/**
 * @brief 取消当前 Modbus 请求响应接收会话
 *
 * @param discardPendingLog: `true` 表示丢弃尚未刷新的 Modbus 原始日志缓存
 *
 * @return 无
 */
void MainWindow::cancelModbusMonitorReceiveSession(bool discardPendingLog)
{
    modbusMonitorAwaitingResponse = false;
    modbusMonitorBuffer.clear();
    if (modbusMonitorResponseTimer != nullptr) {
        modbusMonitorResponseTimer->stop();
    }
    if (discardPendingLog) {
        pendingModbusMonitorLogPacket.clear();
        if (modbusMonitorPacketTimer != nullptr) {
            modbusMonitorPacketTimer->stop();
        }
    }
}

/**
 * @brief 刷新 Modbus 原始日志选项控件状态
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateModbusMonitorLogOptionState()
{
    if (modbusMonitorTimestampCheckBox == nullptr
        || modbusMonitorPacketSplitCheckBox == nullptr
        || modbusMonitorTimeoutLabel == nullptr
        || modbusMonitorTimeoutSpinBox == nullptr
        || modbusMonitorTimeoutUnitLabel == nullptr) {
        return;
    }

    const bool packetSplitEnabled = modbusMonitorPacketSplitCheckBox->isChecked();
    modbusMonitorTimeoutLabel->setEnabled(packetSplitEnabled);
    modbusMonitorTimeoutSpinBox->setEnabled(packetSplitEnabled);
    modbusMonitorTimeoutUnitLabel->setEnabled(packetSplitEnabled);
}

QVector<quint16> MainWindow::parseModbusMonitorWriteValues(QString *errorMessage) const
{
    QVector<quint16> values;
    const int functionCode = modbusMonitorFunctionComboBox->currentData().toInt();
    if (!isModbusWriteFunction(functionCode)) {
        return values;
    }

    const QString text = modbusMonitorWriteValuesEdit->text().trimmed();
    if (text.isEmpty()) {
        if (errorMessage) {
            *errorMessage = uiText(QStringLiteral("请填写写入值。"), QStringLiteral("Please enter write values."));
        }
        return values;
    }

    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    const int expectedCount = functionCode == 0x06 ? 1 : modbusMonitorQuantitySpinBox->value();
    if (parts.size() != expectedCount) {
        if (errorMessage) {
            *errorMessage = uiText(QStringLiteral("写入值数量需要等于 %1。").arg(expectedCount),
                                   QStringLiteral("Write value count must be %1.").arg(expectedCount));
        }
        return values;
    }

    values.reserve(parts.size());
    for (const QString &part : parts) {
        bool ok = false;
        const QString valueText = part.trimmed();
        int base = 10;
        QString numberText = valueText;
        if (numberText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            base = 16;
            numberText = numberText.mid(2);
        }
        const uint value = numberText.toUInt(&ok, base);
        if (!ok || value > 0xFFFFu) {
            if (errorMessage) {
                *errorMessage = uiText(QStringLiteral("写入值必须是 0~65535 或 0x0000~0xFFFF。"),
                                       QStringLiteral("Write values must be 0-65535 or 0x0000-0xFFFF."));
            }
            values.clear();
            return values;
        }
        values.append(static_cast<quint16>(value));
    }
    return values;
}

QString MainWindow::formatModbusMonitorValue(int row) const
{
    if (row < 0 || row >= modbusMonitorRegisterValid.size() || !modbusMonitorRegisterValid.at(row)) {
        return QStringLiteral("--");
    }

    const quint16 value = modbusMonitorRegisterValues.at(row);
    switch (modbusMonitorFormatForRow(row)) {
    case kModbusFormatSigned:
        return QString::number(static_cast<qint16>(value));
    case kModbusFormatHex:
        return formatWordHex(value);
    case kModbusFormatBinary:
        return formatWordBinary(value);
    case kModbusFormatFloat32:
        if (row + 1 < modbusMonitorRegisterValid.size() && modbusMonitorRegisterValid.at(row + 1)) {
            const QString text = registerFloatText(value, modbusMonitorRegisterValues.at(row + 1));
            return text.isEmpty() ? QStringLiteral("--") : text;
        }
        return QStringLiteral("--");
    case kModbusFormatFloat32WordSwap:
        if (row + 1 < modbusMonitorRegisterValid.size() && modbusMonitorRegisterValid.at(row + 1)) {
            const QString text = registerFloatText(modbusMonitorRegisterValues.at(row + 1), value);
            return text.isEmpty() ? QStringLiteral("--") : text;
        }
        return QStringLiteral("--");
    case kModbusFormatUnsigned:
    default:
        return QString::number(value);
    }
}

/**
 * @brief 返回指定 Modbus 表格行实际使用的数据解析格式
 *
 * 单地址覆盖优先；没有覆盖时跟随顶部“默认格式”下拉框。
 *
 * @param row: 表格行号
 *
 * @return 数据格式编号
 */
int MainWindow::modbusMonitorFormatForRow(int row) const
{
    if (row >= 0 && modbusMonitorStartAddressSpinBox != nullptr) {
        const int address = modbusMonitorStartAddressSpinBox->value() + row;
        const auto iterator = modbusMonitorAddressFormats.constFind(address);
        if (iterator != modbusMonitorAddressFormats.constEnd()) {
            return iterator.value();
        }
    }

    return modbusMonitorFormatComboBox != nullptr ? modbusMonitorFormatComboBox->currentData().toInt()
                                                  : kModbusFormatUnsigned;
}

/**
 * @brief 返回 Modbus 解析格式的界面显示名称
 *
 * @param format: 数据格式编号
 *
 * @return 当前语言下的格式名称
 */
QString MainWindow::modbusMonitorFormatName(int format) const
{
    switch (format) {
    case kModbusFormatSigned:
        return uiText(QStringLiteral("有符号十进制"), QStringLiteral("Signed Decimal"));
    case kModbusFormatHex:
        return QStringLiteral("HEX");
    case kModbusFormatBinary:
        return uiText(QStringLiteral("二进制"), QStringLiteral("Binary"));
    case kModbusFormatFloat32:
        return QStringLiteral("Float32 AB CD");
    case kModbusFormatFloat32WordSwap:
        return QStringLiteral("Float32 CD AB");
    case kModbusFormatUnsigned:
    default:
        return uiText(QStringLiteral("无符号十进制"), QStringLiteral("Unsigned Decimal"));
    }
}

/**
 * @brief 处理 Modbus 寄存器表格右键格式菜单
 *
 * @param position: 表格视口内的右键位置
 *
 * @return 无
 */
void MainWindow::showModbusMonitorFormatMenu(const QPoint &position)
{
    if (modbusRegisterTable == nullptr) {
        return;
    }

    const QModelIndex clickedIndex = modbusRegisterTable->indexAt(position);
    const int clickedRow = clickedIndex.isValid() ? clickedIndex.row() : -1;
    const int clickedColumn = clickedIndex.isValid() ? clickedIndex.column() : -1;
    if (clickedIndex.isValid() && !modbusRegisterTable->selectionModel()->isSelected(clickedIndex)) {
        modbusRegisterTable->clearSelection();
        modbusRegisterTable->setCurrentCell(clickedRow, clickedColumn);
        if (QTableWidgetItem *item = modbusRegisterTable->item(clickedRow, clickedColumn)) {
            item->setSelected(true);
        }
    }

    const QList<int> rows = selectedModbusMonitorRows(clickedRow, clickedColumn);
    if (rows.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction *useDefaultAction = menu.addAction(uiText(QStringLiteral("跟随默认格式"),
                                                      QStringLiteral("Use Default Format")));
    useDefaultAction->setData(kModbusFormatUseDefault);
    menu.addSeparator();

    const QList<int> formats = {
        kModbusFormatUnsigned,
        kModbusFormatSigned,
        kModbusFormatHex,
        kModbusFormatBinary,
        kModbusFormatFloat32,
        kModbusFormatFloat32WordSwap,
    };
    for (int format : formats) {
        QAction *action = menu.addAction(modbusMonitorFormatName(format));
        action->setData(format);
        if (modbusFormatRegisterSpan(format) > 1) {
            action->setToolTip(uiText(QStringLiteral("从选中地址开始，连续使用 2 个 16 位寄存器解析。"),
                                      QStringLiteral("Uses two consecutive 16-bit registers from the selected address.")));
        }
    }

    QAction *selectedAction = menu.exec(modbusRegisterTable->viewport()->mapToGlobal(position));
    if (selectedAction == nullptr) {
        return;
    }

    const int selectedFormat = selectedAction->data().toInt();
    applyModbusMonitorFormatOverride(rows, selectedFormat, selectedFormat == kModbusFormatUseDefault);
}

/**
 * @brief 收集需要批量修改解析格式的 Modbus 表格行
 *
 * @param clickedRow: 右键命中的行号
 * @param clickedColumn: 右键命中的列号
 *
 * @return 去重后的表格行号
 */
QList<int> MainWindow::selectedModbusMonitorRows(int clickedRow, int clickedColumn) const
{
    QList<int> rows;
    if (modbusRegisterTable == nullptr) {
        return rows;
    }

    const QList<QTableWidgetItem *> items = modbusRegisterTable->selectedItems();
    bool hasValueColumnSelection = false;
    for (QTableWidgetItem *item : items) {
        if (item != nullptr && item->column() == kModbusValueColumn) {
            hasValueColumnSelection = true;
            break;
        }
    }

    const auto appendRow = [&rows](int row) {
        if (row >= 0 && !rows.contains(row)) {
            rows.append(row);
        }
    };

    for (QTableWidgetItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (hasValueColumnSelection && item->column() != kModbusValueColumn) {
            continue;
        }
        appendRow(item->row());
    }

    if (rows.isEmpty() && clickedRow >= 0) {
        appendRow(clickedRow);
    }

    std::sort(rows.begin(), rows.end());
    Q_UNUSED(clickedColumn);
    return rows;
}

/**
 * @brief 为选中的 Modbus 地址写入或清除解析格式覆盖
 *
 * @param rows: 目标表格行
 * @param format: 数据格式编号
 * @param useDefaultFormat: `true` 表示清除覆盖并跟随默认格式
 *
 * @return 无
 */
void MainWindow::applyModbusMonitorFormatOverride(const QList<int> &rows, int format, bool useDefaultFormat)
{
    if (modbusMonitorStartAddressSpinBox == nullptr || rows.isEmpty()) {
        return;
    }

    int changedCount = 0;
    for (int row : rows) {
        if (row < 0 || row >= modbusRegisterTable->rowCount()) {
            continue;
        }
        const int address = modbusMonitorStartAddressSpinBox->value() + row;
        if (useDefaultFormat) {
            changedCount += modbusMonitorAddressFormats.remove(address);
            continue;
        }
        modbusMonitorAddressFormats.insert(address, format);
        ++changedCount;
    }

    if (changedCount == 0) {
        return;
    }

    refreshModbusMonitorTableValues();
    saveDisplaySettings();
    statusBar()->showMessage(
        useDefaultFormat
            ? uiText(QStringLiteral("已清除 %1 个地址的单独解析格式").arg(changedCount),
                     QStringLiteral("Cleared per-address format for %1 address(es)").arg(changedCount))
            : uiText(QStringLiteral("已将 %1 个地址设置为 %2").arg(changedCount).arg(modbusMonitorFormatName(format)),
                     QStringLiteral("Set %1 address(es) to %2").arg(changedCount).arg(modbusMonitorFormatName(format))),
        3000);
}

/**
 * @brief 处理串口接收到的 Modbus 实时监视响应字节
 *
 * @param data: 串口输入片段
 *
 * @return 无
 */
void MainWindow::handleModbusMonitorBytes(const QByteArray &data)
{
    if (modbusMonitorDock == nullptr
        || !modbusMonitorDock->isVisible()
        || !modbusMonitorAwaitingResponse
        || data.isEmpty()) {
        return;
    }

    appendModbusMonitorLog(data, false);
    const int functionCode = modbusMonitorRequestFunction;
    modbusMonitorBuffer.append(data);
    if (modbusMonitorBuffer.size() > 512) {
        modbusMonitorBuffer.remove(0, modbusMonitorBuffer.size() - 512);
    }

    bool consumed = true;
    while (consumed) {
        consumed = false;
        while (!modbusMonitorBuffer.isEmpty()
               && static_cast<quint8>(modbusMonitorBuffer.at(0)) != modbusMonitorRequestSlave) {
            // 丢弃站号前的噪声字节，尝试从后续字节重新同步 RTU 帧。
            modbusMonitorBuffer.remove(0, 1);
            consumed = true;
        }
        if (modbusMonitorBuffer.size() < 3) {
            return;
        }

        const quint8 responseFunction = static_cast<quint8>(modbusMonitorBuffer.at(1));
        int frameSize = 0;
        if (responseFunction == static_cast<quint8>(functionCode)) {
            if (isModbusReadFunction(functionCode)) {
                const int byteCount = static_cast<quint8>(modbusMonitorBuffer.at(2));
                frameSize = 5 + byteCount;
                if (byteCount != modbusMonitorRequestQuantity * 2) {
                    modbusMonitorBuffer.remove(0, 1);
                    consumed = true;
                    continue;
                }
            } else if (functionCode == 0x06 || functionCode == 0x10) {
                frameSize = 8;
            } else {
                modbusMonitorBuffer.remove(0, 1);
                consumed = true;
                continue;
            }
        } else if (responseFunction == static_cast<quint8>(functionCode | 0x80)) {
            frameSize = 5;
        } else {
            modbusMonitorBuffer.remove(0, 1);
            consumed = true;
            continue;
        }

        if (modbusMonitorBuffer.size() < frameSize) {
            return;
        }

        const QByteArray frame = modbusMonitorBuffer.left(frameSize);
        modbusMonitorBuffer.remove(0, frameSize);
        consumed = true;
        cancelModbusMonitorReceiveSession(false);
        if (!parseModbusMonitorResponse(frame)) {
            ++modbusMonitorErrorCount;
            updateModbusMonitorState();
        }
    }
}

/**
 * @brief 生成当前 Modbus 实时监视查询帧
 *
 * @param 无
 *
 * @return 带 CRC 的 Modbus RTU 查询帧
 */
QByteArray MainWindow::buildModbusMonitorRequest(const QVector<quint16> &writeValues) const
{
    QByteArray frame;
    const int functionCode = modbusMonitorFunctionComboBox->currentData().toInt();
    frame.append(static_cast<char>(modbusMonitorSlaveSpinBox->value()));
    frame.append(static_cast<char>(functionCode));
    appendBigEndian16(frame, static_cast<quint16>(modbusMonitorStartAddressSpinBox->value()));

    if (functionCode == 0x06) {
        if (writeValues.size() != 1) {
            return {};
        }
        appendBigEndian16(frame, writeValues.first());
    } else if (functionCode == 0x10) {
        if (writeValues.isEmpty() || writeValues.size() != modbusMonitorQuantitySpinBox->value()) {
            return {};
        }
        appendBigEndian16(frame, static_cast<quint16>(writeValues.size()));
        frame.append(static_cast<char>(writeValues.size() * 2));
        for (const quint16 value : writeValues) {
            appendBigEndian16(frame, value);
        }
    } else {
        appendBigEndian16(frame, static_cast<quint16>(modbusMonitorQuantitySpinBox->value()));
    }

    const quint16 crc = modbusCrc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

/**
 * @brief 解析一帧 Modbus 实时监视响应并刷新表格
 *
 * @param frame: 完整 Modbus RTU 响应帧
 *
 * @return `true` 表示解析成功或收到合法异常帧
 */
bool MainWindow::parseModbusMonitorResponse(const QByteArray &frame)
{
    if (frame.size() < 5) {
        return false;
    }

    const QByteArray payload = frame.left(frame.size() - 2);
    const quint16 receivedCrc = static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 2))
                                                    | (static_cast<quint8>(frame.at(frame.size() - 1)) << 8));
    if (modbusCrc16(payload) != receivedCrc) {
        return false;
    }

    if (static_cast<quint8>(frame.at(0)) != modbusMonitorRequestSlave) {
        return false;
    }

    const quint8 functionCode = static_cast<quint8>(frame.at(1));
    if ((functionCode & 0x80) != 0) {
        ++modbusMonitorErrorCount;
        const QString message = uiText(QStringLiteral("异常码 %1").arg(formatByteHex(static_cast<quint8>(frame.at(2)))),
                                       QStringLiteral("Exception %1").arg(formatByteHex(static_cast<quint8>(frame.at(2)))));
        for (int row = 0; row < modbusRegisterTable->rowCount(); ++row) {
            if (QTableWidgetItem *item = modbusRegisterTable->item(row, 5)) {
                item->setText(message);
            }
        }
        updateModbusMonitorState();
        return true;
    }

    if (functionCode != modbusMonitorRequestFunction) {
        return false;
    }

    const QString updatedAt = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    if (functionCode == 0x06) {
        if (frame.size() != 8 || readBigEndian16(frame, 2) != modbusMonitorRequestStartAddress) {
            return false;
        }
        const quint16 value = readBigEndian16(frame, 4);
        if (!modbusMonitorRegisterValues.isEmpty()) {
            modbusMonitorRegisterValues[0] = value;
            modbusMonitorRegisterValid[0] = true;
        }
        if (modbusRegisterTable->rowCount() > 0) {
            modbusRegisterTable->item(0, 0)->setText(QString::number(modbusMonitorRequestStartAddress));
            modbusRegisterTable->item(0, 3)->setText(formatWordHex(value));
            modbusRegisterTable->item(0, 4)->setText(updatedAt);
            modbusRegisterTable->item(0, 5)->setText(uiText(QStringLiteral("写入确认"), QStringLiteral("Write OK")));
        }
        ++modbusMonitorResponseCount;
        refreshModbusMonitorTableValues();
        updateModbusMonitorState();
        return true;
    }

    if (functionCode == 0x10) {
        if (frame.size() != 8 || readBigEndian16(frame, 2) != modbusMonitorRequestStartAddress
            || readBigEndian16(frame, 4) != modbusMonitorRequestQuantity) {
            return false;
        }
        for (int row = 0; row < modbusRegisterTable->rowCount(); ++row) {
            if (row < modbusMonitorLastWriteValues.size()) {
                modbusMonitorRegisterValues[row] = modbusMonitorLastWriteValues.at(row);
                modbusMonitorRegisterValid[row] = true;
            }
            modbusRegisterTable->item(row, 4)->setText(updatedAt);
            modbusRegisterTable->item(row, 5)->setText(uiText(QStringLiteral("写入确认"), QStringLiteral("Write OK")));
        }
        ++modbusMonitorResponseCount;
        refreshModbusMonitorTableValues();
        updateModbusMonitorState();
        return true;
    }

    const int byteCount = static_cast<quint8>(frame.at(2));
    if (byteCount != modbusMonitorRequestQuantity * 2 || frame.size() != byteCount + 5) {
        return false;
    }

    for (int index = 0; index < modbusMonitorRequestQuantity; ++index) {
        const quint16 value = readBigEndian16(frame, 3 + index * 2);
        const int row = index;
        if (row >= modbusRegisterTable->rowCount()) {
            break;
        }

        modbusMonitorRegisterValues[row] = value;
        modbusMonitorRegisterValid[row] = true;
        modbusRegisterTable->item(row, 0)->setText(QString::number(modbusMonitorRequestStartAddress + index));
        modbusRegisterTable->item(row, 4)->setText(updatedAt);
        modbusRegisterTable->item(row, 5)->setText(uiText(QStringLiteral("正常"), QStringLiteral("OK")));
    }

    ++modbusMonitorResponseCount;
    refreshModbusMonitorTableValues();
    updateModbusMonitorState();
    return true;
}

/**
 * @brief 清空 Modbus 实时监视数据
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::resetModbusMonitorState()
{
    cancelModbusMonitorReceiveSession(true);
    modbusMonitorLastWriteValues.clear();
    modbusMonitorResponseCount = 0;
    modbusMonitorErrorCount = 0;
    if (modbusMonitorLogEdit != nullptr) {
        modbusMonitorLogEdit->clear();
    }
    updateModbusMonitorRows();
    updateModbusMonitorState();
}

/**
 * @brief 保存 Modbus 单地址解析格式覆盖
 *
 * @param settings: 目标配置对象
 *
 * @return 无
 */
void MainWindow::saveModbusMonitorFormatOverrides(QSettings &settings) const
{
    QStringList entries;
    entries.reserve(modbusMonitorAddressFormats.size());
    const auto end = modbusMonitorAddressFormats.constEnd();
    for (auto iterator = modbusMonitorAddressFormats.constBegin(); iterator != end; ++iterator) {
        if (iterator.key() < 0 || iterator.key() > 0xFFFF || !isModbusMonitorDisplayFormat(iterator.value())) {
            continue;
        }
        entries.append(QStringLiteral("%1=%2").arg(iterator.key()).arg(iterator.value()));
    }
    settings.setValue(QStringLiteral("modbus/format_overrides"), entries);
}

/**
 * @brief 读取 Modbus 单地址解析格式覆盖
 *
 * @param settings: 来源配置对象
 *
 * @return 无
 */
void MainWindow::loadModbusMonitorFormatOverrides(QSettings &settings)
{
    modbusMonitorAddressFormats.clear();

    const QStringList entries = settings.value(QStringLiteral("modbus/format_overrides")).toStringList();
    for (const QString &entry : entries) {
        const int separatorIndex = entry.indexOf(QLatin1Char('='));
        if (separatorIndex <= 0) {
            continue;
        }

        bool addressOk = false;
        bool formatOk = false;
        const int address = entry.left(separatorIndex).toInt(&addressOk);
        const int format = entry.mid(separatorIndex + 1).toInt(&formatOk);
        if (!addressOk || !formatOk || address < 0 || address > 0xFFFF || !isModbusMonitorDisplayFormat(format)) {
            continue;
        }

        modbusMonitorAddressFormats.insert(address, format);
    }
}

/**
 * @brief 保存窗口几何、分隔条和表格列宽布局
 *
 * @param settings: 目标配置对象
 *
 * @return 无
 */
void MainWindow::saveWindowLayoutSettings(QSettings &settings) const
{
    settings.setValue(QStringLiteral("layout/main_geometry"), saveGeometry());
    settings.setValue(QStringLiteral("layout/main_state"), saveState(1));

    if (topDisplaySplitter != nullptr && presetDock != nullptr && presetDock->isVisible()) {
        settings.setValue(QStringLiteral("layout/top_display_splitter_state"),
                          topDisplaySplitter->saveState());
    }

    if (modbusMonitorDock != nullptr) {
        settings.setValue(QStringLiteral("layout/modbus_monitor_geometry"),
                          modbusMonitorDock->saveGeometry());
        settings.setValue(QStringLiteral("layout/modbus_monitor_geometry_version"),
                          kModbusMonitorGeometryLayoutVersion);
    }
    if (modbusMonitorSplitter != nullptr) {
        settings.setValue(QStringLiteral("layout/modbus_monitor_splitter_state"),
                          modbusMonitorSplitter->saveState());
        settings.setValue(QStringLiteral("layout/modbus_monitor_splitter_state_version"),
                          kModbusMonitorSplitterLayoutVersion);
    }
    if (modbusRegisterTable != nullptr) {
        settings.setValue(QStringLiteral("layout/modbus_monitor_table_header"),
                          modbusRegisterTable->horizontalHeader()->saveState());
    }
}

/**
 * @brief 从配置恢复窗口几何、分隔条和表格列宽布局
 *
 * @param settings: 来源配置对象
 *
 * @return 无
 */
void MainWindow::loadWindowLayoutSettings(QSettings &settings)
{
    const QByteArray mainGeometry = settings.value(QStringLiteral("layout/main_geometry")).toByteArray();
    if (!mainGeometry.isEmpty()) {
        restoreGeometry(mainGeometry);
    }

    const QByteArray mainState = settings.value(QStringLiteral("layout/main_state")).toByteArray();
    if (!mainState.isEmpty()) {
        restoreState(mainState, 1);
    }

    const QByteArray modbusGeometry = settings.value(QStringLiteral("layout/modbus_monitor_geometry")).toByteArray();
    const int modbusGeometryVersion =
        settings.value(QStringLiteral("layout/modbus_monitor_geometry_version"), 0).toInt();
    if (modbusMonitorDock != nullptr
        && !modbusGeometry.isEmpty()
        && modbusGeometryVersion >= kModbusMonitorGeometryLayoutVersion) {
        modbusMonitorDock->restoreGeometry(modbusGeometry);
    }

    const QByteArray modbusSplitterState =
        settings.value(QStringLiteral("layout/modbus_monitor_splitter_state")).toByteArray();
    const int modbusSplitterVersion =
        settings.value(QStringLiteral("layout/modbus_monitor_splitter_state_version"), 0).toInt();
    if (modbusMonitorSplitter != nullptr
        && !modbusSplitterState.isEmpty()
        && modbusSplitterVersion >= kModbusMonitorSplitterLayoutVersion) {
        modbusMonitorSplitter->restoreState(modbusSplitterState);
    }

    const QByteArray modbusTableHeader =
        settings.value(QStringLiteral("layout/modbus_monitor_table_header")).toByteArray();
    if (modbusRegisterTable != nullptr && !modbusTableHeader.isEmpty()) {
        modbusRegisterTable->horizontalHeader()->restoreState(modbusTableHeader);
    }

    applyPresetDockVisibilityLayout(presetDock != nullptr && presetDock->isVisible());
}

/**
 * @brief 按多字符串面板可见状态恢复接收区分隔条比例
 *
 * @param visible: 多字符串面板是否可见
 *
 * @return 无
 */
void MainWindow::applyPresetDockVisibilityLayout(bool visible)
{
    if (topDisplaySplitter == nullptr) {
        return;
    }

    if (!visible) {
        topDisplaySplitter->setSizes({1, 0});
        return;
    }

    QSettings settings = createAppSettings();
    const QByteArray splitterState =
        settings.value(QStringLiteral("layout/top_display_splitter_state")).toByteArray();
    if (!splitterState.isEmpty() && topDisplaySplitter->restoreState(splitterState)) {
        return;
    }

    // 多字符串表格有 7 列，首次展开时需要给右侧面板足够宽度；
    // 保存过 splitter 状态时上面已经优先恢复用户拖拽后的比例。
    topDisplaySplitter->setSizes({580, 640});
}

/**
 * @brief 单独保存发送编辑框草稿
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::saveSendEditorText() const
{
    QSettings settings = createAppSettings();
    settings.setValue(QStringLiteral("send/editor_text"),
                      sendEdit != nullptr ? sendEdit->toPlainText() : QString());
    settings.sync();
}

/**
 * @brief 保存显示与发送相关的本地配置
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::saveDisplaySettings() const
{
    // 保存配置必须保持纯持久化，不改变主接收区的超时分包边界；
    // 格式切换、导出和关闭程序等真实边界由调用方显式刷新 pendingReceivePacket。
    QSettings settings = createAppSettings();
    settings.setValue(QStringLiteral("display/font_point_size"), consoleFontPointSize);
    settings.setValue(QStringLiteral("display/font_family"), consoleFontFamily);
    settings.setValue(QStringLiteral("display/filter_keyword"), receiveFilterKeyword);
    settings.setValue(QStringLiteral("display/highlight_keyword"), receiveHighlightKeyword);
    settings.setValue(QStringLiteral("display/hex_enabled"),
                      hexDisplayCheckBox != nullptr && hexDisplayCheckBox->isChecked());
    settings.setValue(QStringLiteral("display/timestamp_enabled"),
                      timestampCheckBox != nullptr && timestampCheckBox->isChecked());
    settings.setValue(QStringLiteral("display/wrap_enabled"),
                      wrapCheckBox != nullptr && wrapCheckBox->isChecked());
    settings.setValue(QStringLiteral("display/packet_split_enabled"),
                      packetSplitCheckBox != nullptr && packetSplitCheckBox->isChecked());
    settings.setValue(QStringLiteral("display/packet_timeout_ms"),
                      receiveTimeoutSpinBox != nullptr ? receiveTimeoutSpinBox->value() : 50);
    settings.setValue(QStringLiteral("display/preset_dock_visible"),
                      presetDock != nullptr && presetDock->isVisible());
    settings.setValue(QStringLiteral("display/modbus_monitor_visible"),
                      modbusMonitorDock != nullptr && modbusMonitorDock->isVisible());
    settings.setValue(QStringLiteral("serial/port_name"),
                      portComboBox != nullptr ? extractPortName(portComboBox->currentText()) : QString());
    settings.setValue(QStringLiteral("serial/baud_rate"), currentBaudRate());
    settings.setValue(QStringLiteral("serial/data_bits"), currentDataBits());
    settings.setValue(QStringLiteral("serial/parity_code"), currentParityCode());
    settings.setValue(QStringLiteral("serial/stop_bits"),
                      stopBitsComboBox != nullptr ? stopBitsComboBox->currentText() : QStringLiteral("1"));
    settings.setValue(QStringLiteral("serial/rts_enabled"),
                      rtsCheckBox != nullptr && rtsCheckBox->isChecked());
    settings.setValue(QStringLiteral("serial/dtr_enabled"),
                      dtrCheckBox != nullptr && dtrCheckBox->isChecked());
    settings.setValue(QStringLiteral("send/hex_enabled"),
                      hexSendCheckBox != nullptr && hexSendCheckBox->isChecked());
    settings.setValue(QStringLiteral("send/append_new_line"),
                      appendNewLineCheckBox != nullptr && appendNewLineCheckBox->isChecked());
    settings.setValue(QStringLiteral("send/auto_send_enabled"),
                      autoSendCheckBox != nullptr && autoSendCheckBox->isChecked());
    settings.setValue(QStringLiteral("send/auto_send_interval_ms"),
                      autoSendIntervalSpinBox != nullptr ? autoSendIntervalSpinBox->value() : 500);
    settings.setValue(QStringLiteral("send/editor_text"),
                      sendEdit != nullptr ? sendEdit->toPlainText() : QString());
    settings.setValue(QStringLiteral("send/checksum_mode"),
                      checksumComboBox != nullptr ? checksumComboBox->currentData().toInt() : kChecksumNone);
    settings.setValue(QStringLiteral("send/file_chunk_bytes"), fileSendChunkBytes);
    settings.setValue(QStringLiteral("send/file_delay_ms"), fileSendDelayMs);
    settings.setValue(QStringLiteral("modbus/default_format"),
                      modbusMonitorFormatComboBox != nullptr
                          ? modbusMonitorFormatComboBox->currentData().toInt()
                          : kModbusFormatUnsigned);
    settings.setValue(QStringLiteral("modbus/log_timestamp_enabled"),
                      modbusMonitorTimestampCheckBox == nullptr || modbusMonitorTimestampCheckBox->isChecked());
    settings.setValue(QStringLiteral("modbus/log_packet_split_enabled"),
                      modbusMonitorPacketSplitCheckBox != nullptr && modbusMonitorPacketSplitCheckBox->isChecked());
    settings.setValue(QStringLiteral("modbus/log_packet_timeout_ms"),
                      modbusMonitorTimeoutSpinBox != nullptr ? modbusMonitorTimeoutSpinBox->value() : 50);
    saveModbusMonitorFormatOverrides(settings);
    saveWindowLayoutSettings(settings);
    settings.sync();
}

/**
 * @brief 读取显示与发送相关的本地配置
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::loadDisplaySettings()
{
    QSettings settings = createAppSettings();
    const QFont defaultFont = defaultConsoleFont();
    consoleFontPointSize = settings.value(QStringLiteral("display/font_point_size"), defaultFont.pointSize()).toInt();
    consoleFontFamily = settings.value(QStringLiteral("display/font_family"), defaultFont.family()).toString();
    receiveFilterKeyword = settings.value(QStringLiteral("display/filter_keyword")).toString();
    receiveHighlightKeyword = settings.value(QStringLiteral("display/highlight_keyword")).toString();
    hexDisplayCheckBox->setChecked(settings.value(QStringLiteral("display/hex_enabled"), false).toBool());
    timestampCheckBox->setChecked(settings.value(QStringLiteral("display/timestamp_enabled"), true).toBool());
    wrapCheckBox->setChecked(settings.value(QStringLiteral("display/wrap_enabled"), true).toBool());
    packetSplitCheckBox->setChecked(settings.value(QStringLiteral("display/packet_split_enabled"), false).toBool());
    receiveTimeoutSpinBox->setValue(settings.value(QStringLiteral("display/packet_timeout_ms"), 50).toInt());
    const QString savedPort = settings.value(QStringLiteral("serial/port_name")).toString();
    if (!savedPort.trimmed().isEmpty()) {
        refreshPortList(savedPort);
    }
    const int savedBaudRate = settings.value(QStringLiteral("serial/baud_rate"), 115200).toInt();
    setCurrentBaudRateText(QString::number(savedBaudRate > 0 ? savedBaudRate : 115200));
    dataBitsComboBox->setCurrentText(settings.value(QStringLiteral("serial/data_bits"), 8).toString());
    const QString parityCode = settings.value(QStringLiteral("serial/parity_code"), QStringLiteral("None")).toString();
    if (parityComboBox->findData(parityCode) >= 0) {
        parityComboBox->setCurrentIndex(parityComboBox->findData(parityCode));
    }
    stopBitsComboBox->setCurrentText(settings.value(QStringLiteral("serial/stop_bits"), QStringLiteral("1")).toString());
    rtsCheckBox->setChecked(settings.value(QStringLiteral("serial/rts_enabled"), false).toBool());
    dtrCheckBox->setChecked(settings.value(QStringLiteral("serial/dtr_enabled"), false).toBool());
    hexSendCheckBox->setChecked(settings.value(QStringLiteral("send/hex_enabled"), false).toBool());
    appendNewLineCheckBox->setChecked(settings.value(QStringLiteral("send/append_new_line"), false).toBool());
    autoSendCheckBox->setChecked(settings.value(QStringLiteral("send/auto_send_enabled"), false).toBool());
    autoSendIntervalSpinBox->setValue(settings.value(QStringLiteral("send/auto_send_interval_ms"), 500).toInt());
    {
        QSignalBlocker blocker(sendEdit);
        sendEdit->setPlainText(settings.value(QStringLiteral("send/editor_text")).toString());
    }
    fileSendChunkBytes = settings.value(QStringLiteral("send/file_chunk_bytes"), 1024).toInt();
    fileSendDelayMs = settings.value(QStringLiteral("send/file_delay_ms"), 2).toInt();
    const int checksumMode = settings.value(QStringLiteral("send/checksum_mode"), kChecksumNone).toInt();
    if (checksumComboBox->findData(checksumMode) >= 0) {
        checksumComboBox->setCurrentIndex(checksumComboBox->findData(checksumMode));
    }
    loadModbusMonitorFormatOverrides(settings);
    const int modbusDefaultFormat =
        settings.value(QStringLiteral("modbus/default_format"), kModbusFormatUnsigned).toInt();
    if (modbusMonitorFormatComboBox != nullptr && modbusMonitorFormatComboBox->findData(modbusDefaultFormat) >= 0) {
        modbusMonitorFormatComboBox->setCurrentIndex(modbusMonitorFormatComboBox->findData(modbusDefaultFormat));
    }
    if (modbusMonitorTimestampCheckBox != nullptr) {
        modbusMonitorTimestampCheckBox->setChecked(
            settings.value(QStringLiteral("modbus/log_timestamp_enabled"), true).toBool());
    }
    if (modbusMonitorPacketSplitCheckBox != nullptr) {
        modbusMonitorPacketSplitCheckBox->setChecked(
            settings.value(QStringLiteral("modbus/log_packet_split_enabled"), false).toBool());
    }
    if (modbusMonitorTimeoutSpinBox != nullptr) {
        modbusMonitorTimeoutSpinBox->setValue(
            settings.value(QStringLiteral("modbus/log_packet_timeout_ms"), 50).toInt());
    }

    updateConsoleFont();
    receiveTimeoutLabel->setEnabled(packetSplitCheckBox->isChecked());
    receiveTimeoutSpinBox->setEnabled(packetSplitCheckBox->isChecked());
    if (receiveTimeoutUnitLabel != nullptr) {
        receiveTimeoutUnitLabel->setEnabled(packetSplitCheckBox->isChecked());
    }
    if (presetDock != nullptr) {
        presetDock->setVisible(settings.value(QStringLiteral("display/preset_dock_visible"), true).toBool());
    }
    if (modbusMonitorDock != nullptr) {
        modbusMonitorDock->setVisible(settings.value(QStringLiteral("display/modbus_monitor_visible"), false).toBool());
    }
    loadWindowLayoutSettings(settings);
    hexDisplayAction->setChecked(hexDisplayCheckBox->isChecked());
    timestampAction->setChecked(timestampCheckBox->isChecked());
    wrapAction->setChecked(wrapCheckBox->isChecked());
    updateModbusMonitorLogOptionState();
    refreshModbusMonitorTableValues();
    refreshReceiveView();
}

/**
 * @brief 打开工具箱并切换到指定页签
 *
 * @param pageIndex: 目标页索引
 *
 * @return 无
 */
void MainWindow::openToolboxPage(int pageIndex)
{
    if (toolboxDialog != nullptr) {
        toolboxDialog->openPage(pageIndex);
    }
}

/**
 * @brief 发送一条来自多字符串面板的独立命令
 *
 * @param payloadText: 待发送内容
 * @param hexMode: `true` 表示按 HEX 解析后发送
 * @param appendNewLine: `true` 表示末尾附加 CRLF
 *
 * @return 无
 */
void MainWindow::sendPresetPayload(const QString &payloadText, bool hexMode, bool appendNewLine)
{
    if (!portOpen) {
        statusBar()->showMessage(uiText(QStringLiteral("请先连接串口"), QStringLiteral("Please connect a serial port first")),
                                 3000);
        return;
    }

    QString errorMessage;
    const QByteArray payload = buildPayload(payloadText, hexMode, appendNewLine, &errorMessage);
    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("发送失败"), QStringLiteral("Send Failed")),
                             errorMessage);
        return;
    }
    if (payload.isEmpty()) {
        return;
    }

    appendTrafficLog(payload, true, hexMode);
    emit requestWriteData(payload);
}

/**
 * @brief 显示最近一次导入文件的提示卡信息
 *
 * @param filePath: 文件完整路径
 * @param fileSize: 文件总字节数
 * @param previewOnly: `true` 表示当前仅展示安全预览
 * @param binaryLike: `true` 表示检测结果更像二进制文件
 * @param loadedAsHex: `true` 表示以 HEX 文本形式装载到发送区
 * @param previewBytes: 预览字节数
 *
 * @return 无
 */
/**
 * @brief 发送一条来自协议工具箱的独立报文
 *
 * 工具箱页中的 Modbus 和自定义协议已经完成组包与校验，
 * 这里直接按指定格式发送，不再叠加主发送区的附加校验。
 *
 * @param payloadText: 待发送内容
 * @param hexMode: `true` 表示按 HEX 解析后发送
 * @param appendNewLine: `true` 表示末尾附加 CRLF
 *
 * @return 无
 */
void MainWindow::sendToolboxPayload(const QString &payloadText, bool hexMode, bool appendNewLine)
{
    if (!portOpen) {
        statusBar()->showMessage(uiText(QStringLiteral("请先连接串口"), QStringLiteral("Please connect a serial port first")),
                                 3000);
        return;
    }

    QString errorMessage;
    QByteArray payload;
    if (hexMode) {
        payload = parseHexPayload(payloadText, &errorMessage);
    } else {
        payload = payloadText.toLocal8Bit();
    }

    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("发送失败"), QStringLiteral("Send Failed")),
                             errorMessage);
        return;
    }

    if (appendNewLine) {
        payload.append("\r\n");
    }
    if (payload.isEmpty()) {
        return;
    }

    appendTrafficLog(payload, true, hexMode);
    emit requestWriteData(payload);
}

/**
 * @brief 显示最近一次导入文件的提示卡信息
 *
 * @param filePath: 文件完整路径
 * @param fileSize: 文件总字节数
 * @param previewOnly: `true` 表示当前仅显示安全预览
 * @param binaryLike: `true` 表示检测结果更像二进制文件
 * @param loadedAsHex: `true` 表示以 HEX 文本形式装载到发送区
 * @param previewBytes: 预览字节数
 *
 * @return 无
 */
void MainWindow::showImportedFileNotice(const QString &filePath,
                                        qint64 fileSize,
                                        bool previewOnly,
                                        bool binaryLike,
                                        bool loadedAsHex,
                                        qint64 previewBytes)
{
    // 最近一次导入文件的信息需要独立保存。
    // 这样提示卡可以持续显示，也能在中英文切换时重新生成说明文字。
    importedFilePath = filePath;
    importedFileSize = fileSize;
    importedPreviewBytes = previewBytes;
    importedPreviewOnly = previewOnly;
    importedBinaryLike = binaryLike;
    importedLoadedAsHex = loadedAsHex;

    updateImportedFileNoticeTexts();
    importNoticeFrame->setVisible(!importedFilePath.isEmpty());
    updateUiState();
}

/**
 * @brief 根据当前语言和导入状态刷新提示卡文本
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::updateImportedFileNoticeTexts()
{
    if (importNoticeFrame == nullptr || importNoticeTitleLabel == nullptr || importNoticeDetailLabel == nullptr
        || importNoticeHintLabel == nullptr || importNoticeActionButton == nullptr) {
        return;
    }

    if (importedFilePath.isEmpty()) {
        importNoticeFrame->setVisible(false);
        return;
    }

    const QFileInfo fileInfo(importedFilePath);
    const QString fileName = fileInfo.fileName().isEmpty() ? importedFilePath : fileInfo.fileName();

    if (importedPreviewOnly) {
        importNoticeTitleLabel->setText(
            uiText(QStringLiteral("已导入文件“%1”，当前仅显示安全预览").arg(fileName),
                   QStringLiteral("Imported \"%1\". Safe preview is shown instead of the full file.").arg(fileName)));
        importNoticeDetailLabel->setText(
            uiText(QStringLiteral("文件大小：%1 字节\n识别结果：%2\n当前处理：仅载入前 %3 字节 HEX 预览")
                       .arg(importedFileSize)
                       .arg(importedBinaryLike ? QStringLiteral("二进制 / 固件文件")
                                               : QStringLiteral("文件体积较大"))
                       .arg(importedPreviewBytes),
                   QStringLiteral("Size: %1 bytes\nDetected as: %2\nCurrent handling: first %3 bytes shown as HEX preview")
                       .arg(importedFileSize)
                       .arg(importedBinaryLike ? QStringLiteral("binary / firmware file")
                                               : QStringLiteral("large file"))
                       .arg(importedPreviewBytes)));
        importNoticeHintLabel->setText(
            uiText(QStringLiteral("为了避免界面卡死，完整文件没有直接写入发送框。需要整文件发送时，请点击右侧“直接发送该文件”。"),
                   QStringLiteral("To avoid UI freezing, the full file was not inserted into the send editor. Use \"Send This File\" on the right to transmit the original file.")));
        importNoticeActionButton->setVisible(true);
        importNoticeActionButton->setText(uiText(QStringLiteral("直接发送该文件"), QStringLiteral("Send This File")));
    } else {
        importNoticeTitleLabel->setText(
            uiText(QStringLiteral("已导入文件“%1”到发送区").arg(fileName),
                   QStringLiteral("Imported \"%1\" into the send editor.").arg(fileName)));
        importNoticeDetailLabel->setText(
            uiText(QStringLiteral("文件大小：%1 字节\n装入方式：%2")
                       .arg(importedFileSize)
                       .arg(importedLoadedAsHex ? QStringLiteral("HEX 文本") : QStringLiteral("文本内容")),
                   QStringLiteral("Size: %1 bytes\nLoaded as: %2")
                       .arg(importedFileSize)
                       .arg(importedLoadedAsHex ? QStringLiteral("HEX text")
                                                : QStringLiteral("plain text"))));
        importNoticeHintLabel->setText(
            uiText(QStringLiteral("当前内容已经写入发送框，可继续编辑后再发送。"),
                   QStringLiteral("The content is now in the send editor and can be edited before sending.")));
        importNoticeActionButton->setVisible(false);
    }
}

/**
 * @brief 清空导入文件提示卡及相关状态
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::clearImportedFileNotice()
{
    importedFilePath.clear();
    importedFileSize = 0;
    importedPreviewBytes = 0;
    importedPreviewOnly = false;
    importedBinaryLike = false;
    importedLoadedAsHex = false;

    if (importNoticeFrame != nullptr) {
        importNoticeFrame->setVisible(false);
    }
}

/**
 * @brief 将指定文件按原始字节流加入发送队列
 *
 * @param fileName: 待发送文件路径
 *
 * @return `true` 表示加入发送队列成功，`false` 表示失败
 */
bool MainWindow::queueFileForSending(const QString &fileName)
{
    if (fileName.isEmpty()) {
        return false;
    }
    if (!portOpen) {
        statusBar()->showMessage(uiText(QStringLiteral("请先连接串口，再直接发送文件"),
                                        QStringLiteral("Connect a serial port before sending a file directly")),
                                 3000);
        return false;
    }

    const QFileInfo fileInfo(fileName);
    int chunkBytes = fileSendChunkBytes;
    int delayMs = fileSendDelayMs;
    if (!promptFileSendOptions(fileInfo, &chunkBytes, &delayMs)) {
        return false;
    }

    fileSendChunkBytes = chunkBytes;
    fileSendDelayMs = delayMs;
    saveDisplaySettings();
    return sendFileInChunks(fileName, chunkBytes, delayMs);
}

/**
 * @brief 选择文件并按原始字节流直接发送
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::sendFileData()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        uiText(QStringLiteral("选择待发送文件"), QStringLiteral("Choose File To Send")),
        QString(),
        uiText(QStringLiteral("所有文件 (*.*)"), QStringLiteral("All Files (*.*)")));
    if (fileName.isEmpty()) {
        return;
    }
    if (!portOpen) {
        statusBar()->showMessage(uiText(QStringLiteral("请先连接串口"), QStringLiteral("Please connect a serial port first")),
                                 3000);
        return;
    }
    queueFileForSending(fileName);
}

/**
 * @brief 开启接收窗口持续保存功能
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::startLiveCapture()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        uiText(QStringLiteral("持续保存接收数据"), QStringLiteral("Save Receive Stream Continuously")),
        QStringLiteral("serial_live_log.txt"),
        uiText(QStringLiteral("文本文件 (*.txt);;所有文件 (*.*)"),
               QStringLiteral("Text Files (*.txt);;All Files (*.*)")));
    if (fileName.isEmpty()) {
        return;
    }

    stopLiveCapture();
    liveCaptureFile = new QFile(fileName, this);
    if (!liveCaptureFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete liveCaptureFile;
        liveCaptureFile = nullptr;
        QMessageBox::warning(this,
                             uiText(QStringLiteral("持续保存失败"), QStringLiteral("Continuous Capture Failed")),
                             uiText(QStringLiteral("无法打开目标文件。"), QStringLiteral("Unable to open the target file.")));
        return;
    }

    liveCapturePath = fileName;
    statusBar()->showMessage(uiText(QStringLiteral("已开启持续保存"), QStringLiteral("Continuous capture started")),
                             2500);
}

/**
 * @brief 停止接收窗口持续保存功能
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::stopLiveCapture()
{
    if (liveCaptureFile == nullptr) {
        return;
    }

    liveCaptureFile->close();
    delete liveCaptureFile;
    liveCaptureFile = nullptr;
    liveCapturePath.clear();
    statusBar()->showMessage(uiText(QStringLiteral("已停止持续保存"), QStringLiteral("Continuous capture stopped")),
                             2500);
}

/**
 * @brief 将新增显示内容实时写入持续保存文件
 *
 * @param chunk: 待写入的文本片段
 *
 * @return 无
 */
void MainWindow::writeLiveCaptureChunk(const QString &chunk)
{
    if (liveCaptureFile == nullptr || chunk.isEmpty()) {
        return;
    }

    liveCaptureFile->write(chunk.toUtf8());
    liveCaptureFile->flush();
}

/**
 * @brief 把当前接收窗口内容导出到文本文件
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::saveReceiveContent()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        uiText(QStringLiteral("导出接收文本"), QStringLiteral("Export Receive Text")),
        QStringLiteral("serial_log.txt"),
        uiText(QStringLiteral("文本文件 (*.txt);;所有文件 (*.*)"),
               QStringLiteral("Text Files (*.txt);;All Files (*.*)")));

    if (fileName.isEmpty()) {
        return;
    }

    // 导出前显式刷新一次，确保开启超时分包时最后一包也写入文件。
    flushPendingReceivePacket();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("导出失败"), QStringLiteral("Export Failed")),
                             uiText(QStringLiteral("无法写入目标文件。"),
                                    QStringLiteral("Unable to write the target file.")));
        return;
    }

    file.write(receiveEdit->toPlainText().toUtf8());
    statusBar()->showMessage(uiText(QStringLiteral("接收文本已导出"),
                                    QStringLiteral("Receive text exported")),
                             2500);
}

/**
 * @brief 选择文件并装载到发送区或预览卡片
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::loadSendFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        uiText(QStringLiteral("导入发送文件"), QStringLiteral("Import Send File")),
        QString(),
        uiText(QStringLiteral("所有文件 (*.*)"), QStringLiteral("All Files (*.*)")));

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("导入失败"), QStringLiteral("Import Failed")),
                             uiText(QStringLiteral("无法读取所选文件。"),
                                    QStringLiteral("Unable to read the selected file.")));
        return;
    }

    const QFileInfo fileInfo(file);
    const QByteArray previewData = file.read(qMin<qint64>(fileInfo.size(), kPreviewByteLimit));
    int suspiciousBytes = 0;
    for (const auto byte : previewData) {
        const quint8 value = static_cast<quint8>(byte);
        if (value == 0 || (value < 0x09) || (value > 0x0D && value < 0x20)) {
            ++suspiciousBytes;
        }
    }

    const bool binaryLike = !previewData.isEmpty() && suspiciousBytes * 5 > previewData.size();
    const bool hugeForEditor =
        fileInfo.size() > (hexSendCheckBox->isChecked() ? (32 * 1024) : kLargeFileThreshold);

    if (binaryLike || hugeForEditor) {
        const QString summary =
            uiText(QStringLiteral("[文件导入提示]\n"
                                  "文件名: %1\n"
                                  "文件大小: %2 字节\n"
                                  "检测结果: %3\n"
                                  "说明: 为了避免界面卡死，软件没有把整个文件直接塞进发送编辑框。\n"
                                  "建议: 如需发送完整文件，请使用菜单里的“直接发送文件...”。\n"
                                  "HEX 预览(前 %4 字节):\n%5"),
                   QStringLiteral("[Import Notice]\n"
                                  "File: %1\n"
                                  "Size: %2 bytes\n"
                                  "Detected as: %3\n"
                                  "Note: The whole file was not inserted into the editor to avoid freezing the UI.\n"
                                  "Suggestion: Use \"Send File Directly...\" to transmit the full file.\n"
                                  "HEX Preview (first %4 bytes):\n%5"))
                .arg(fileInfo.fileName())
                .arg(fileInfo.size())
                .arg(binaryLike ? uiText(QStringLiteral("二进制 / 固件文件"), QStringLiteral("binary / firmware file"))
                                : uiText(QStringLiteral("文件过大"), QStringLiteral("file too large")))
                .arg(previewData.size())
                .arg(QString::fromLatin1(previewData.toHex(' ').toUpper()));

        sendEdit->setPlainText(summary);
        showImportedFileNotice(fileName,
                               fileInfo.size(),
                               true,
                               binaryLike,
                               false,
                               previewData.size());
        statusBar()->showMessage(uiText(QStringLiteral("已载入文件摘要预览，请改用“直接发送文件”发送完整内容"),
                                        QStringLiteral("Loaded a file summary preview. Use \"Send File Directly\" for the full content.")),
                                 4000);
        return;
    }

    file.seek(0);
    const QByteArray fileData = file.readAll();
    if (hexSendCheckBox->isChecked()) {
        sendEdit->setPlainText(QString::fromLatin1(fileData.toHex(' ').toUpper()));
    } else {
        sendEdit->setPlainText(QString::fromLocal8Bit(fileData));
    }
    showImportedFileNotice(fileName,
                           fileInfo.size(),
                           false,
                           false,
                           hexSendCheckBox->isChecked(),
                           fileData.size());

    statusBar()->showMessage(uiText(QStringLiteral("发送区已载入文件内容"),
                                    QStringLiteral("File content loaded into transmit panel")),
                             2500);
}

/**
 * @brief 显示软件说明或关于信息对话框
 *
 * @param title: 对话框标题
 * @param text: 对话框正文
 *
 * @return 无
 */
void MainWindow::showInfoDialog(const QString &title, const QString &text)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.resize(760, 520);
    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog {
            background: #f8fbff;
        }
        QTextBrowser {
            background: #ffffff;
            color: #16324f;
            border: 1px solid #bfd2e6;
            padding: 6px;
            font-size: 13px;
        }
        QPushButton {
            min-width: 84px;
            min-height: 30px;
            padding: 0 12px;
            color: #16324f;
            background: #ffffff;
            border: 1px solid #bfd2e6;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: #eef5ff;
        }
    )"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *browser = new QTextBrowser(&dialog);
    browser->setOpenLinks(true);
    browser->setOpenExternalLinks(true);
    browser->setText(text);
    layout->addWidget(browser, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    if (QAbstractButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(uiText(QStringLiteral("确定"), QStringLiteral("OK")));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}

/**
 * @brief 返回当前窗口标题文本
 *
 * @param 无
 *
 * @return 带当前版本号的标题栏文本
 */
QString MainWindow::windowTitleText() const
{
    return uiText(QStringLiteral("X-Ray 串口调试台 v%1"),
                  QStringLiteral("X-Ray Serial Console v%1"))
        .arg(QString::fromLatin1(AppVersion::kSemanticVersion));
}

/**
 * @brief 返回使用说明对话框需要显示的富文本
 *
 * @param 无
 *
 * @return 覆盖基础收发、多字符串、Modbus 和打包流程的说明文本
 */
QString MainWindow::quickGuideText() const
{
    return uiText(
        QStringLiteral(R"(
<h2 style="margin:0 0 8px 0;">X-Ray 串口调试台使用说明</h2>
<h3>基础串口收发</h3>
<ol>
  <li>选择串口号、波特率、数据位、校验位和停止位后，点击“连接串口”。</li>
  <li>在发送面板输入普通文本或十六进制字节，按需勾选“HEX发送”“追加换行”和校验附加选项，然后点击“发送数据”。</li>
  <li>勾选“循环发送”后，当前发送区内容会按设定毫秒间隔重复发送；固定单位显示在输入框外侧。</li>
  <li>“直接发送文件...”适合发送较大的二进制或文本文件，可设置分块字节数和块间延时。</li>
</ol>
<h3>接收监视区</h3>
<ul>
  <li>“HEX显示”“时间戳”“自动换行”只改变显示方式，不影响底层收发计数。</li>
  <li>开启“超时分包显示”后，接收片段会先缓存，等达到主接收区超时值后再统一落屏。</li>
  <li>筛选关键字生效时，标题会显示“筛选中”；如果没有匹配内容，空白提示会说明当前被筛选。</li>
  <li>关闭时间戳不会隐藏发送帧，发送方向仍会以“发→”显示。</li>
</ul>
<h3>多字符串面板</h3>
<ul>
  <li>“多字符串”用于保存多条常用发送内容，可单条发送，也可按顺序轮询发送。</li>
  <li>新增空白项默认按普通文本发送；需要按 HEX 发送时勾选“HEX发送”列。</li>
  <li>发送、HEX发送、换行列使用独立勾选框，表格列宽和条目内容会保存到本地配置。</li>
</ul>
<h3>Modbus 读写工具</h3>
<ul>
  <li>“工具 > Modbus 读写工具”提供独立窗口，支持 03/04 读取寄存器、06/16 写寄存器和定时扫描。</li>
  <li>寄存器表支持右键为单个地址设置解析格式，也支持多选单元格批量设置。</li>
  <li>Float32 格式会从当前地址和下一地址两个 16 位寄存器合成 4 字节浮点数。</li>
  <li>Modbus 原始收发日志有独立的时间戳和超时分包设置，不会改写主接收区显示选项。</li>
</ul>
<h3>配置与打包</h3>
<ul>
  <li>程序会自动保存串口参数、窗口布局、发送草稿、显示选项、多字符串预设和 Modbus 设置。</li>
  <li>从 Qt Creator / CMake 构建目录运行时，配置写入稳定应用配置目录；便携版会优先使用程序目录 INI。</li>
  <li>Windows 打包脚本位于 scripts 目录，可生成便携版，也可在安装 Inno Setup 后生成安装包。</li>
</ul>
)"),
        QStringLiteral(R"(
<h2 style="margin:0 0 8px 0;">X-Ray Serial Console Quick Guide</h2>
<h3>Basic Serial TX/RX</h3>
<ol>
  <li>Select the COM port, baud rate, data bits, parity, and stop bits, then click "Connect".</li>
  <li>Enter plain text or hexadecimal bytes in the send panel, choose HEX send, newline, or checksum options as needed, then click "Send".</li>
  <li>Enable loop sending to repeat the current send payload at the configured millisecond interval; fixed units are shown beside inputs.</li>
  <li>Use "Send File Directly..." for larger binary or text files, with configurable chunk size and delay.</li>
</ol>
<h3>Receive Monitor</h3>
<ul>
  <li>HEX display, timestamps, and word wrap only affect rendering, not the underlying TX/RX counters.</li>
  <li>When timeout packet splitting is enabled, received fragments are buffered and displayed after the main receive timeout elapses.</li>
  <li>When a filter is active, the title shows the filtered state; if nothing matches, the empty hint explains that filtering is active.</li>
  <li>Disabling timestamps does not hide TX frames; outgoing data still appears with a TX direction prefix.</li>
</ul>
<h3>Multi String Panel</h3>
<ul>
  <li>Use Multi String to store common payloads, send individual rows, or poll enabled rows in order.</li>
  <li>New blank rows send as plain text by default; enable the "HEX Send" column when hexadecimal sending is required.</li>
  <li>Send, HEX Send, and newline columns use real checkboxes, and row content plus column widths are persisted locally.</li>
</ul>
<h3>Modbus Read / Write Tool</h3>
<ul>
  <li>"Tools > Modbus Read / Write Tool" opens an independent window for 03/04 register reads, 06/16 register writes, and timed scanning.</li>
  <li>The register table supports per-address format overrides from the context menu and batch formatting for selected cells.</li>
  <li>Float32 formats combine two adjacent 16-bit registers into one 4-byte floating-point value.</li>
  <li>The Modbus raw TX/RX log has independent timestamp and timeout-split settings and does not rewrite the main receive options.</li>
</ul>
<h3>Settings and Packaging</h3>
<ul>
  <li>The application persists serial parameters, window layout, send drafts, display options, multi-string presets, and Modbus settings.</li>
  <li>Qt Creator / CMake development runs use a stable app config location, while portable builds prefer an INI beside the executable.</li>
  <li>Windows packaging scripts live under scripts and can produce a portable build or an optional Inno Setup installer.</li>
</ul>
)"));
}

/**
 * @brief 返回关于对话框需要显示的富文本
 *
 * @param 无
 *
 * @return 包含当前版本和首次发布说明的说明文本
 */
QString MainWindow::aboutDialogText() const
{
    return uiText(
        QStringLiteral(R"(
<h2 style="margin:0 0 8px 0;">X-Ray 串口调试台</h2>
<p><b>当前版本：</b>v%1<br/>
<b>日期版本：</b>%2<br/>
<b>发布日期：</b>%3</p>
<p><b>发布说明：</b>首次发布。</p>
)")
            .arg(QString::fromLatin1(AppVersion::kSemanticVersion),
                 QString::fromLatin1(AppVersion::kDateVersion),
                 QString::fromLatin1(AppVersion::kReleaseDate)),
        QStringLiteral(R"(
<h2 style="margin:0 0 8px 0;">X-Ray Serial Console</h2>
<p><b>Current version:</b> v%1<br/>
<b>Date version:</b> %2<br/>
<b>Release date:</b> %3</p>
<p><b>Release note:</b> First release.</p>
)")
            .arg(QString::fromLatin1(AppVersion::kSemanticVersion),
                 QString::fromLatin1(AppVersion::kDateVersion),
                 QString::fromLatin1(AppVersion::kReleaseDate)));
}

/**
 * @brief 弹出自定义波特率输入框
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::promptCustomBaudRate()
{
    if (baudRateComboBox == nullptr) {
        return;
    }

    bool ok = false;
    const int baudRate = QInputDialog::getInt(this,
                                              uiText(QStringLiteral("自定义波特率"), QStringLiteral("Custom Baud Rate")),
                                              uiText(QStringLiteral("请输入整数波特率"),
                                                     QStringLiteral("Enter an integer baud rate")),
                                              currentBaudRate(),
                                              110,
                                              12000000,
                                              1,
                                              &ok);
    if (!ok) {
        setCurrentBaudRateText(baudRateComboBox->property("lastValidBaud").toString());
        return;
    }

    setCurrentBaudRateText(QString::number(baudRate));
    statusBar()->showMessage(uiText(QStringLiteral("已设置自定义波特率 %1").arg(baudRate),
                                    QStringLiteral("Custom baud rate set to %1").arg(baudRate)),
                             2500);
}

/**
 * @brief 设置当前波特率文本，并确保自定义值保留在下拉列表中
 *
 * @param baudText: 目标波特率文本
 *
 * @return 无
 */
void MainWindow::setCurrentBaudRateText(const QString &baudText)
{
    if (baudRateComboBox == nullptr) {
        return;
    }

    bool ok = false;
    const int baudValue = baudText.trimmed().toInt(&ok);
    const QString normalizedBaud = ok && baudValue > 0 ? QString::number(baudValue) : QStringLiteral("115200");
    const int customIndex = baudRateComboBox->findData(QString::fromLatin1(kCustomBaudRateToken));
    if (baudRateComboBox->findText(normalizedBaud, Qt::MatchFixedString) < 0) {
        const int insertIndex = customIndex >= 0 ? customIndex : baudRateComboBox->count();
        baudRateComboBox->insertItem(insertIndex, normalizedBaud, normalizedBaud);
    }

    baudRateComboBox->setCurrentText(normalizedBaud);
    baudRateComboBox->setProperty("lastValidBaud", normalizedBaud);
}

/**
 * @brief 选择发送区与接收区共用的控制台字体
 *
 * @param 无
 *
 * @return 无
 */
void MainWindow::chooseConsoleFont()
{
    bool ok = false;
    QFont initialFont = receiveEdit->font();
    if (initialFont.family().trimmed().isEmpty()) {
        initialFont = defaultConsoleFont();
    }

    const QFont selectedFont = QFontDialog::getFont(&ok,
                                                    initialFont,
                                                    this,
                                                    uiText(QStringLiteral("设置字体"),
                                                           QStringLiteral("Set Font")));
    if (!ok) {
        return;
    }

    consoleFontFamily = selectedFont.family().trimmed();
    if (selectedFont.pointSize() > 0) {
        consoleFontPointSize = qBound(9, selectedFont.pointSize(), 72);
    }
    updateConsoleFont();
    saveDisplaySettings();
}

/**
 * @brief 弹出文件发送参数对话框
 *
 * @param fileInfo: 待发送文件信息
 * @param chunkBytes: 输出的分块字节数
 * @param delayMs: 输出的块间延时
 *
 * @return `true` 表示用户确认发送参数
 */
bool MainWindow::promptFileSendOptions(const QFileInfo &fileInfo, int *chunkBytes, int *delayMs)
{
    if (chunkBytes == nullptr || delayMs == nullptr) {
        return false;
    }

    struct SendPreset {
        const char *chineseName;
        const char *englishName;
        int chunkBytes;
        int delayMs;
    };

    static const SendPreset kPresets[] = {
        {"极速直发", "Max Speed", 4096, 0},
        {"通用平衡", "Balanced", 1024, 2},
        {"单片机稳妥", "MCU Safe", 256, 10},
        {"Bootloader", "Bootloader", 64, 20}
    };
    const int presetCount = int(sizeof(kPresets) / sizeof(kPresets[0]));

    QDialog dialog(this);
    dialog.setWindowTitle(uiText(QStringLiteral("文件发送选项"), QStringLiteral("File Send Options")));
    dialog.setModal(true);
    dialog.resize(460, 260);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *summaryLabel = new QLabel(
        uiText(QStringLiteral("文件：%1\n大小：%2")
                   .arg(fileInfo.fileName().isEmpty() ? fileInfo.filePath() : fileInfo.fileName(),
                        formattedByteCount(fileInfo.size())),
               QStringLiteral("File: %1\nSize: %2")
                   .arg(fileInfo.fileName().isEmpty() ? fileInfo.filePath() : fileInfo.fileName(),
                        formattedByteCount(fileInfo.size()))),
        &dialog);
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(8);

    auto *presetComboBox = new QComboBox(&dialog);
    for (const SendPreset &preset : kPresets) {
        presetComboBox->addItem(uiText(QString::fromUtf8(preset.chineseName),
                                       QString::fromLatin1(preset.englishName)));
    }
    presetComboBox->addItem(uiText(QStringLiteral("自定义"), QStringLiteral("Custom")));

    auto *chunkSpinBox = new QSpinBox(&dialog);
    chunkSpinBox->setRange(1, 65536);
    chunkSpinBox->setSuffix(QStringLiteral(" B"));
    chunkSpinBox->setValue(qBound(1, fileSendChunkBytes, 65536));

    auto *delaySpinBox = new QSpinBox(&dialog);
    delaySpinBox->setRange(0, 5000);
    delaySpinBox->setMinimumWidth(76);
    delaySpinBox->setValue(qBound(0, fileSendDelayMs, 5000));
    auto *delayEditor = new QWidget(&dialog);
    auto *delayEditorLayout = new QHBoxLayout(delayEditor);
    delayEditorLayout->setContentsMargins(0, 0, 0, 0);
    delayEditorLayout->setSpacing(6);
    delayEditorLayout->addWidget(delaySpinBox);
    delayEditorLayout->addWidget(new QLabel(QStringLiteral("ms"), delayEditor));
    delayEditorLayout->addStretch();

    formLayout->addRow(uiText(QStringLiteral("发送方案"), QStringLiteral("Preset")), presetComboBox);
    formLayout->addRow(uiText(QStringLiteral("每块字节数"), QStringLiteral("Bytes Per Chunk")), chunkSpinBox);
    formLayout->addRow(uiText(QStringLiteral("块间延时"), QStringLiteral("Delay Between Chunks")), delayEditor);
    layout->addLayout(formLayout);

    auto *hintLabel = new QLabel(
        uiText(QStringLiteral("规则：每发送指定字节数后暂停指定毫秒，再继续下一块。0 ms 表示连续发送。"),
               QStringLiteral("Rule: after each chunk is queued, wait for the specified delay before the next chunk. 0 ms means continuous sending.")),
        &dialog);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    const int customPresetIndex = presetComboBox->count() - 1;
    const auto syncPresetCombo = [&]() {
        int matchedIndex = customPresetIndex;
        for (int index = 0; index < presetCount; ++index) {
            if (kPresets[index].chunkBytes == chunkSpinBox->value()
                && kPresets[index].delayMs == delaySpinBox->value()) {
                matchedIndex = index;
                break;
            }
        }

        const QSignalBlocker blocker(presetComboBox);
        presetComboBox->setCurrentIndex(matchedIndex);
    };

    const auto applyPreset = [&](int index) {
        if (index < 0 || index >= presetCount) {
            return;
        }
        const QSignalBlocker chunkBlocker(chunkSpinBox);
        const QSignalBlocker delayBlocker(delaySpinBox);
        chunkSpinBox->setValue(kPresets[index].chunkBytes);
        delaySpinBox->setValue(kPresets[index].delayMs);
    };

    connect(presetComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int index) {
        if (index != customPresetIndex) {
            applyPreset(index);
        }
    });
    connect(chunkSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [&](int) { syncPresetCombo(); });
    connect(delaySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [&](int) { syncPresetCombo(); });

    syncPresetCombo();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QAbstractButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(uiText(QStringLiteral("开始发送"), QStringLiteral("Start Sending")));
    }
    if (QAbstractButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(uiText(QStringLiteral("取消"), QStringLiteral("Cancel")));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *chunkBytes = chunkSpinBox->value();
    *delayMs = delaySpinBox->value();
    return true;
}

/**
 * @brief 按分块和延时参数发送文件，并显示进度条
 *
 * @param fileName: 待发送文件路径
 * @param chunkBytes: 每次发送的字节数
 * @param delayMs: 每块之间的延时
 *
 * @return `true` 表示完整发送完毕
 */
bool MainWindow::sendFileInChunks(const QString &fileName, int chunkBytes, int delayMs)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("发送文件失败"), QStringLiteral("Send File Failed")),
                             uiText(QStringLiteral("无法读取所选文件。"),
                                    QStringLiteral("Unable to read the selected file.")));
        return false;
    }

    const QFileInfo fileInfo(fileName);
    const qint64 totalBytes = file.size();
    if (totalBytes <= 0) {
        QMessageBox::warning(this,
                             uiText(QStringLiteral("发送文件失败"), QStringLiteral("Send File Failed")),
                             uiText(QStringLiteral("所选文件为空，无法发送。"),
                                    QStringLiteral("The selected file is empty.")));
        return false;
    }

    QProgressDialog progressDialog(
        uiText(QStringLiteral("准备发送文件..."), QStringLiteral("Preparing file transfer...")),
        uiText(QStringLiteral("取消"), QStringLiteral("Cancel")),
        0,
        100,
        this);
    progressDialog.setWindowTitle(uiText(QStringLiteral("发送文件"), QStringLiteral("Send File")));
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setMinimumDuration(0);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);

    const auto updateProgress = [&](qint64 processedBytes) {
        const int percentage = totalBytes > 0
                                   ? int(qMin<qint64>(100, (processedBytes * 100) / totalBytes))
                                   : 100;
        progressDialog.setValue(percentage);
        progressDialog.setLabelText(
            uiText(QStringLiteral("正在发送“%1”\n已发送：%2 / %3\n分块：%4 字节，块间延时：%5 ms")
                       .arg(fileInfo.fileName())
                       .arg(formattedByteCount(processedBytes))
                       .arg(formattedByteCount(totalBytes))
                       .arg(chunkBytes)
                       .arg(delayMs),
                   QStringLiteral("Sending \"%1\"\nProcessed: %2 / %3\nChunk size: %4 bytes, delay: %5 ms")
                       .arg(fileInfo.fileName())
                       .arg(formattedByteCount(processedBytes))
                       .arg(formattedByteCount(totalBytes))
                       .arg(chunkBytes)
                       .arg(delayMs)));
        QCoreApplication::processEvents();
    };

    qint64 processedBytes = 0;
    updateProgress(0);

    while (!file.atEnd()) {
        if (progressDialog.wasCanceled()) {
            statusBar()->showMessage(
                uiText(QStringLiteral("文件发送已取消，已处理 %1 / %2")
                           .arg(formattedByteCount(processedBytes), formattedByteCount(totalBytes)),
                       QStringLiteral("File transfer canceled after %1 / %2")
                           .arg(formattedByteCount(processedBytes), formattedByteCount(totalBytes))),
                4000);
            return false;
        }

        if (!portOpen) {
            QMessageBox::warning(this,
                                 uiText(QStringLiteral("发送中断"), QStringLiteral("Transfer Interrupted")),
                                 uiText(QStringLiteral("串口已断开，文件发送已停止。"),
                                        QStringLiteral("The serial port disconnected during file transfer.")));
            return false;
        }

        const QByteArray chunk = file.read(chunkBytes);
        if (chunk.isEmpty()) {
            break;
        }

        emit requestWriteData(chunk);
        processedBytes += chunk.size();
        updateProgress(processedBytes);

        if (delayMs > 0 && processedBytes < totalBytes) {
            QElapsedTimer waitTimer;
            waitTimer.start();
            while (waitTimer.elapsed() < delayMs) {
                if (progressDialog.wasCanceled()) {
                    break;
                }
                QThread::msleep(static_cast<unsigned long>(qMin<qint64>(10, delayMs - waitTimer.elapsed())));
                QCoreApplication::processEvents();
                if (!portOpen) {
                    break;
                }
            }
        }
    }

    progressDialog.setValue(100);
    statusBar()->showMessage(
        uiText(QStringLiteral("文件“%1”已发送完成，共 %2").arg(fileInfo.fileName(), formattedByteCount(processedBytes)),
               QStringLiteral("Finished sending \"%1\", total %2").arg(fileInfo.fileName(), formattedByteCount(processedBytes))),
        4000);
    return true;
}

/**
 * @brief 发送当前发送区内容
 *
 * @param autoTriggered: `true` 表示由循环发送定时器触发
 *
 * @return 无
 */
void MainWindow::sendCurrentPayload(bool autoTriggered)
{
    if (!portOpen) {
        if (!autoTriggered) {
            statusBar()->showMessage(uiText(QStringLiteral("请先连接串口"),
                                            QStringLiteral("Please connect a serial port first")),
                                     3000);
        }
        return;
    }

    QString errorMessage;
    const QByteArray payload = currentSendPayload(&errorMessage);

    if (!errorMessage.isEmpty()) {
        if (autoTriggered) {
            autoSendCheckBox->setChecked(false);
            statusBar()->showMessage(uiText(QStringLiteral("循环发送已停止：%1"),
                                            QStringLiteral("Loop sending stopped: %1"))
                                         .arg(errorMessage),
                                     5000);
        } else {
            QMessageBox::warning(this,
                                 uiText(QStringLiteral("发送失败"), QStringLiteral("Send Failed")),
                                 errorMessage);
        }
        return;
    }

    if (payload.isEmpty()) {
        if (!autoTriggered) {
            statusBar()->showMessage(uiText(QStringLiteral("发送区为空"),
                                            QStringLiteral("Transmit panel is empty")),
                                     2500);
        }
        return;
    }

    // 发送动作也沿用同一套显示规则，关闭时间戳时仍显示带方向前缀的发送帧。
    appendTrafficLog(payload, true, hexSendCheckBox->isChecked());
    emit requestWriteData(payload);
}

/**
 * @brief 读取当前波特率
 *
 * @param 无
 *
 * @return 当前选择的波特率，异常时回退到 115200
 */
int MainWindow::currentBaudRate() const
{
    bool ok = false;
    const int value = baudRateComboBox->currentText().trimmed().toInt(&ok);
    return ok && value > 0 ? value : 115200;
}

/**
 * @brief 读取当前数据位
 *
 * @param 无
 *
 * @return 当前选择的数据位，异常时回退到 8
 */
int MainWindow::currentDataBits() const
{
    bool ok = false;
    const int value = dataBitsComboBox->currentText().trimmed().toInt(&ok);
    return ok ? value : 8;
}

/**
 * @brief 获取当前串口校验位代码
 *
 * @param 无
 *
 * @return 当前校验位代码
 */
QString MainWindow::currentParityCode() const
{
    return parityComboBox->currentData().toString();
}

/**
 * @brief 获取发送区当前选择的校验附加模式
 *
 * @param 无
 *
 * @return 校验模式编号
 */
int MainWindow::currentSendChecksumMode() const
{
    if (checksumComboBox == nullptr) {
        return kChecksumNone;
    }

    const QVariant checksumData = checksumComboBox->currentData();
    return checksumData.isValid() ? checksumData.toInt() : kChecksumNone;
}

/**
 * @brief 构建当前发送区最终要发送的字节流
 *
 * @param errorMessage: 解析失败时返回错误描述
 *
 * @return 最终发送字节流
 */
QByteArray MainWindow::currentSendPayload(QString *errorMessage) const
{
    return buildPayload(sendEdit->toPlainText(),
                        hexSendCheckBox->isChecked(),
                        appendNewLineCheckBox->isChecked(),
                        errorMessage);
}

/**
 * @brief 根据输入内容与发送选项构建最终载荷
 *
 * @param content: 原始输入文本
 * @param hexMode: `true` 表示按 HEX 文本解析
 * @param appendNewLine: `true` 表示末尾附加 CRLF
 * @param errorMessage: 解析失败时返回错误描述
 *
 * @return 组装完成的最终发送字节流
 */
QByteArray MainWindow::buildPayload(const QString &content,
                                    bool hexMode,
                                    bool appendNewLine,
                                    QString *errorMessage) const
{
    if (content.trimmed().isEmpty()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return {};
    }

    QByteArray payload;
    if (hexMode) {
        payload = parseHexPayload(content, errorMessage);
        if (errorMessage != nullptr && !errorMessage->isEmpty()) {
            return {};
        }
    } else {
        if (errorMessage) {
            errorMessage->clear();
        }
        payload = content.toLocal8Bit();
    }

    payload.append(checksumSuffix(payload, currentSendChecksumMode()));

    if (appendNewLine) {
        payload.append("\r\n", 2);
    }

    return payload;
}

/**
 * @brief 根据方向生成日志前缀
 *
 * @param outgoing: `true` 表示发送方向
 *
 * @return 带方向含义的前缀文本
 */
QString MainWindow::trafficPrefix(bool outgoing) const
{
    return outgoing ? uiText(QStringLiteral("发→ "), QStringLiteral("TX-> "))
                    : uiText(QStringLiteral("收← "), QStringLiteral("RX<- "));
}

/**
 * @brief 将字节流转换成用于显示的文本
 *
 * @param data: 原始字节流
 * @param hexMode: `true` 表示按十六进制文本显示
 *
 * @return 可直接显示的文本
 */
QString MainWindow::trafficPayloadText(const QByteArray &data, bool hexMode) const
{
    if (hexMode) {
        return QString::fromLatin1(data.toHex(' ').toUpper());
    }

    return QString::fromLocal8Bit(data);
}

/**
 * @brief 根据当前语言返回对应文案
 *
 * @param chinese: 中文文案
 * @param english: 英文文案
 *
 * @return 当前语言对应的文本
 */
QString MainWindow::uiText(const QString &chinese, const QString &english) const
{
    return currentLanguage == UiLanguage::Chinese ? chinese : english;
}

/**
 * @brief 枚举当前系统可用的串口列表
 *
 * @param 无
 *
 * @return 排序并去重后的串口名列表
 */
QStringList MainWindow::availablePorts()
{
    QSettings settings(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                       QSettings::NativeFormat);

    QStringList ports;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys) {
        const QString port = settings.value(key).toString().trimmed().toUpper();
        if (!port.isEmpty()) {
            ports << port;
        }
    }

    ports.append(queryDosDevicePorts());
    ports.removeDuplicates();

    std::sort(ports.begin(), ports.end(), [](const QString &left, const QString &right) {
        static const QRegularExpression portPattern(QStringLiteral("^COM(\\d+)$"),
                                                    QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch leftMatch = portPattern.match(left);
        const QRegularExpressionMatch rightMatch = portPattern.match(right);
        const int leftIndex = leftMatch.hasMatch() ? leftMatch.captured(1).toInt()
                                                   : std::numeric_limits<int>::max();
        const int rightIndex = rightMatch.hasMatch() ? rightMatch.captured(1).toInt()
                                                     : std::numeric_limits<int>::max();
        if (leftIndex != rightIndex) {
            return leftIndex < rightIndex;
        }
        return left < right;
    });

    return ports;
}

/**
 * @brief 解析 COM 端口号中的数字部分
 *
 * @param portName: 端口名称，例如 `COM13`
 *
 * @return 端口序号，无法解析时返回极大值
 */
int MainWindow::comPortIndex(const QString &portName) const
{
    static const QRegularExpression portPattern(QStringLiteral("^COM(\\d+)$"),
                                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = portPattern.match(portName.trimmed());
    if (!match.hasMatch()) {
        return std::numeric_limits<int>::max();
    }

    return match.captured(1).toInt();
}

/**
 * @brief 将 HEX 文本解析为原始字节流
 *
 * @param input: 待解析的 HEX 文本
 * @param errorMessage: 解析失败时返回错误描述
 *
 * @return 解析后的字节流
 */
QByteArray MainWindow::parseHexPayload(const QString &input, QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QString compact = input;
    compact.remove(QRegularExpression(QStringLiteral("0[xX]")));
    compact.remove(QRegularExpression(QStringLiteral("[\\s,;:]+")));

    if (compact.isEmpty()) {
        return {};
    }

    const QRegularExpression hexPattern(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!hexPattern.match(compact).hasMatch()) {
        if (errorMessage) {
            *errorMessage = uiText(QStringLiteral("HEX 数据中包含非十六进制字符。"),
                                   QStringLiteral("HEX data contains non-hex characters."));
        }
        return {};
    }

    if (compact.size() % 2 != 0) {
        if (errorMessage) {
            *errorMessage = uiText(QStringLiteral("HEX 数据长度必须为偶数。"),
                                   QStringLiteral("HEX data length must be even."));
        }
        return {};
    }

    QByteArray payload;
    payload.reserve(compact.size() / 2);

    for (int index = 0; index < compact.size(); index += 2) {
        bool ok = false;
        const int value = compact.mid(index, 2).toInt(&ok, 16);
        if (!ok) {
            if (errorMessage) {
                *errorMessage = uiText(QStringLiteral("HEX 数据解析失败。"),
                                       QStringLiteral("Failed to parse HEX data."));
            }
            return {};
        }
        payload.append(static_cast<char>(value));
    }

    return payload;
}
