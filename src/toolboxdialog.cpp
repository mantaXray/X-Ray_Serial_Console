/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: toolboxdialog.cpp
 * Version: 1.0.0
 * Description: 协议与工具箱对话框实现
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#include "toolboxdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QList>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>

#include <cmath>
#include <limits>
#include <utility>

namespace {

struct ModbusFunctionDescriptor {
    int code = 0;
    const char *chineseName = "";
    const char *englishName = "";
    const char *chineseHint = "";
    const char *englishHint = "";
};

/**
 * @brief 返回所有内置 Modbus 功能码描述
 *
 * @param 无
 *
 * @return 功能码描述数组
 */
const QList<ModbusFunctionDescriptor> &modbusFunctionDescriptors()
{
    static const QList<ModbusFunctionDescriptor> descriptors = {
        {0x01,
         "01 读线圈",
         "01 Read Coils",
         "地址字段表示线圈起始地址，数量字段表示要读取的线圈个数。",
         "Address is the coil start address. Quantity is the number of coils to read."},
        {0x02,
         "02 读离散输入",
         "02 Read Discrete Inputs",
         "地址字段表示离散输入起始地址，数量字段表示要读取的输入点个数。",
         "Address is the discrete-input start address. Quantity is the number of inputs to read."},
        {0x03,
         "03 读保持寄存器",
         "03 Read Holding Registers",
         "地址字段表示保持寄存器起始地址，数量字段表示寄存器个数。",
         "Address is the holding-register start address. Quantity is the register count."},
        {0x04,
         "04 读输入寄存器",
         "04 Read Input Registers",
         "地址字段表示输入寄存器起始地址，数量字段表示寄存器个数。",
         "Address is the input-register start address. Quantity is the register count."},
        {0x05,
         "05 写单线圈",
         "05 Write Single Coil",
         "数量/数值字段按 0 或 1 填写，软件会自动转换成 0000 / FF00。",
         "Put 0 or 1 in the value field. The tool converts it to 0000 / FF00 automatically."},
        {0x06,
         "06 写单寄存器",
         "06 Write Single Register",
         "数量/数值字段表示 16 位寄存器写入值。",
         "The value field is the 16-bit register value to write."},
        {0x0F,
         "15 写多个线圈",
         "15 Write Multiple Coils",
         "数据字节请填写打包后的线圈位图，字节数需等于 ceil(数量 / 8)。",
         "Enter packed coil bitmap bytes. Byte count must equal ceil(quantity / 8)."},
        {0x10,
         "16 写多个寄存器",
         "16 Write Multiple Registers",
         "数据字节请按寄存器高字节在前填写，字节数必须等于 数量 x 2。",
         "Enter register bytes in big-endian order. Byte count must equal quantity x 2."},
        {0x11,
         "17 报告从站标识",
         "17 Report Slave ID",
         "该请求只有站号和功能码，不使用地址、数量和数据字段。",
         "This request only uses slave ID and function code. Address, quantity and data are ignored."},
        {0x16,
         "22 掩码写寄存器",
         "22 Mask Write Register",
         "数据字节需填写 4 个字节：AND Mask(2 字节) + OR Mask(2 字节)。",
         "Data must contain 4 bytes: AND mask (2 bytes) + OR mask (2 bytes)."},
        {0x17,
         "23 读写多个寄存器",
         "23 Read/Write Multiple Registers",
         "地址/数量表示读取起始地址和读取数量；数据字节填写“写起始地址(2字节) + 写入寄存器数据”。写数量将自动从数据长度推导。",
         "Address/quantity describe the read start and read count. Data must be \"write start address (2 bytes) + register bytes\". Write quantity is derived automatically."}
    };
    return descriptors;
}

/**
 * @brief 根据功能码查找 Modbus 描述
 *
 * @param functionCode: 需要查找的功能码
 *
 * @return 对应描述指针，未找到则返回空指针
 */
const ModbusFunctionDescriptor *findModbusDescriptor(int functionCode)
{
    const auto &descriptors = modbusFunctionDescriptors();
    for (const auto &descriptor : descriptors) {
        if (descriptor.code == functionCode) {
            return &descriptor;
        }
    }
    return nullptr;
}

/**
 * @brief 把字节数组格式化为带空格的 HEX 文本
 *
 * @param data: 需要显示的字节数组
 *
 * @return 形如 “01 03 00 00” 的大写 HEX 字符串
 */
QString formatHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

/**
 * @brief 解析 HEX 文本为原始字节数组
 *
 * @param input: 用户输入的 HEX 文本
 * @param chineseMode: 是否返回中文错误提示
 * @param errorMessage: 输出错误信息
 *
 * @return 解析后的字节数组，失败时返回空数组
 */
QByteArray parseHexString(const QString &input, bool chineseMode, QString *errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    QString compact = input;
    compact.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    compact.remove(QRegularExpression(QStringLiteral("[\\s,;:]+")));

    if (compact.isEmpty()) {
        return {};
    }

    const QRegularExpression hexPattern(QStringLiteral("^[0-9A-Fa-f]+$"));
    if (!hexPattern.match(compact).hasMatch()) {
        if (errorMessage != nullptr) {
            *errorMessage = chineseMode ? QStringLiteral("HEX 数据中包含非十六进制字符。")
                                        : QStringLiteral("HEX data contains non-hex characters.");
        }
        return {};
    }

    if (compact.size() % 2 != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = chineseMode ? QStringLiteral("HEX 数据长度必须为偶数。")
                                        : QStringLiteral("HEX data length must be even.");
        }
        return {};
    }

    QByteArray result;
    result.reserve(compact.size() / 2);
    for (int index = 0; index < compact.size(); index += 2) {
        bool ok = false;
        const int value = compact.mid(index, 2).toInt(&ok, 16);
        if (!ok) {
            if (errorMessage != nullptr) {
                *errorMessage = chineseMode ? QStringLiteral("HEX 数据解析失败。")
                                            : QStringLiteral("Failed to parse HEX data.");
            }
            return {};
        }
        result.append(static_cast<char>(value));
    }

    return result;
}

/**
 * @brief 按大端格式向缓冲区追加 16 位值
 *
 * @param buffer: 目标缓冲区
 * @param value: 需要追加的 16 位数值
 *
 * @return 无
 */
void appendBigEndian16(QByteArray &buffer, quint16 value)
{
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

/**
 * @brief 从指定偏移读取大端 16 位无符号值
 *
 * @param buffer: 源缓冲区
 * @param offset: 偏移位置
 *
 * @return 读取到的 16 位数值
 */
quint16 readBigEndian16(const QByteArray &buffer, int offset)
{
    return (static_cast<quint8>(buffer.at(offset)) << 8)
           | static_cast<quint8>(buffer.at(offset + 1));
}

/**
 * @brief 计算 Modbus RTU CRC16
 *
 * @param data: 需要计算 CRC 的报文体
 *
 * @return CRC16 值，低字节在前发送
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
 * @brief 根据所选算法生成自定义协议校验字节
 *
 * @param body: 参与校验的报文体
 * @param algorithmIndex: 校验算法索引
 *
 * @return 对应算法生成的校验字节
 */
QByteArray checksumBytes(const QByteArray &body, int algorithmIndex)
{
    QByteArray checksum;

    switch (algorithmIndex) {
    case 1: {
        quint8 sum = 0;
        for (const auto byte : body) {
            sum = static_cast<quint8>(sum + static_cast<quint8>(byte));
        }
        checksum.append(static_cast<char>(sum));
        break;
    }
    case 2: {
        quint8 xorValue = 0;
        for (const auto byte : body) {
            xorValue ^= static_cast<quint8>(byte);
        }
        checksum.append(static_cast<char>(xorValue));
        break;
    }
    case 3: {
        const quint16 crc = modbusCrc16(body);
        checksum.append(static_cast<char>(crc & 0xFF));
        checksum.append(static_cast<char>((crc >> 8) & 0xFF));
        break;
    }
    default:
        break;
    }

    return checksum;
}

/**
 * @brief 根据功能码返回说明名称
 *
 * @param functionCode: 功能码
 * @param chineseMode: 是否返回中文文本
 *
 * @return 功能码名称
 */
QString modbusFunctionName(int functionCode, bool chineseMode)
{
    if (const auto *descriptor = findModbusDescriptor(functionCode)) {
        return QString::fromUtf8(chineseMode ? descriptor->chineseName : descriptor->englishName);
    }

    return chineseMode ? QStringLiteral("未知功能码") : QStringLiteral("Unknown Function");
}

/**
 * @brief 根据功能码返回详细说明
 *
 * @param functionCode: 功能码
 * @param chineseMode: 是否返回中文文本
 *
 * @return 功能码说明
 */
QString modbusFunctionHint(int functionCode, bool chineseMode)
{
    if (const auto *descriptor = findModbusDescriptor(functionCode)) {
        return QString::fromUtf8(chineseMode ? descriptor->chineseHint : descriptor->englishHint);
    }

    return chineseMode ? QStringLiteral("当前功能码未内置说明，请按原始 Modbus RTU 帧格式手动确认。")
                       : QStringLiteral("No built-in note for this function code. Please verify the raw Modbus RTU frame manually.");
}

/**
 * @brief 根据异常码返回说明
 *
 * @param exceptionCode: 异常码
 * @param chineseMode: 是否返回中文文本
 *
 * @return 异常码含义
 */
QString modbusExceptionDescription(quint8 exceptionCode, bool chineseMode)
{
    switch (exceptionCode) {
    case 0x01:
        return chineseMode ? QStringLiteral("非法功能") : QStringLiteral("Illegal Function");
    case 0x02:
        return chineseMode ? QStringLiteral("非法数据地址") : QStringLiteral("Illegal Data Address");
    case 0x03:
        return chineseMode ? QStringLiteral("非法数据值") : QStringLiteral("Illegal Data Value");
    case 0x04:
        return chineseMode ? QStringLiteral("从站设备故障") : QStringLiteral("Slave Device Failure");
    case 0x05:
        return chineseMode ? QStringLiteral("确认，稍后完成") : QStringLiteral("Acknowledge");
    case 0x06:
        return chineseMode ? QStringLiteral("从站忙") : QStringLiteral("Slave Device Busy");
    case 0x08:
        return chineseMode ? QStringLiteral("存储奇偶校验错误") : QStringLiteral("Memory Parity Error");
    case 0x0A:
        return chineseMode ? QStringLiteral("网关路径不可用") : QStringLiteral("Gateway Path Unavailable");
    case 0x0B:
        return chineseMode ? QStringLiteral("网关目标设备无响应") : QStringLiteral("Gateway Target Device Failed to Respond");
    default:
        return chineseMode ? QStringLiteral("未知异常") : QStringLiteral("Unknown Exception");
    }
}

struct ExpressionResult {
    bool ok = false;
    double value = 0.0;
    QString error;
};

class ExpressionParser
{
public:
    /**
     * @brief 构造表达式解析器
     *
     * @param expression: 需要解析的数学表达式
     *
     * @return 无
     */
    explicit ExpressionParser(QString expression)
        : input(std::move(expression))
    {
    }

    /**
     * @brief 解析完整表达式
     *
     * @param 无
     *
     * @return 表达式解析结果
     */
    ExpressionResult parse()
    {
        ExpressionResult result;
        skipSpaces();
        const double value = parseExpression(result);
        skipSpaces();
        if (!result.ok) {
            return result;
        }
        if (position != input.size()) {
            result.ok = false;
            result.error = QStringLiteral("表达式中存在无法识别的字符。");
            return result;
        }
        result.ok = true;
        result.value = value;
        return result;
    }

private:
    /**
     * @brief 解析加减表达式
     *
     * @param result: 输出解析状态
     *
     * @return 当前层级的计算结果
     */
    double parseExpression(ExpressionResult &result)
    {
        double value = parseTerm(result);
        while (result.ok) {
            skipSpaces();
            if (match('+')) {
                value += parseTerm(result);
            } else if (match('-')) {
                value -= parseTerm(result);
            } else {
                break;
            }
        }
        return value;
    }

    /**
     * @brief 解析乘除模表达式
     *
     * @param result: 输出解析状态
     *
     * @return 当前层级的计算结果
     */
    double parseTerm(ExpressionResult &result)
    {
        double value = parseFactor(result);
        while (result.ok) {
            skipSpaces();
            if (match('*')) {
                value *= parseFactor(result);
            } else if (match('/')) {
                const double divisor = parseFactor(result);
                if (qFuzzyIsNull(divisor)) {
                    result.ok = false;
                    result.error = QStringLiteral("除数不能为 0。");
                    return 0.0;
                }
                value /= divisor;
            } else if (match('%')) {
                const double divisor = parseFactor(result);
                if (qFuzzyIsNull(divisor)) {
                    result.ok = false;
                    result.error = QStringLiteral("取模除数不能为 0。");
                    return 0.0;
                }
                value = std::fmod(value, divisor);
            } else {
                break;
            }
        }
        return value;
    }

    /**
     * @brief 解析一元运算和括号
     *
     * @param result: 输出解析状态
     *
     * @return 当前层级的计算结果
     */
    double parseFactor(ExpressionResult &result)
    {
        skipSpaces();
        if (match('+')) {
            return parseFactor(result);
        }
        if (match('-')) {
            return -parseFactor(result);
        }
        if (match('(')) {
            const double value = parseExpression(result);
            skipSpaces();
            if (!match(')')) {
                result.ok = false;
                result.error = QStringLiteral("括号没有正确闭合。");
                return 0.0;
            }
            return value;
        }
        return parseNumber(result);
    }

    /**
     * @brief 解析数字
     *
     * @param result: 输出解析状态
     *
     * @return 解析到的数值
     */
    double parseNumber(ExpressionResult &result)
    {
        skipSpaces();
        const int start = position;
        while (position < input.size()) {
            const QChar ch = input.at(position);
            if (ch.isDigit() || ch == QLatin1Char('.')) {
                ++position;
            } else {
                break;
            }
        }
        if (start == position) {
            result.ok = false;
            result.error = QStringLiteral("缺少数字。");
            return 0.0;
        }

        bool ok = false;
        const double value = input.mid(start, position - start).toDouble(&ok);
        if (!ok) {
            result.ok = false;
            result.error = QStringLiteral("数字解析失败。");
            return 0.0;
        }

        result.ok = true;
        return value;
    }

    /**
     * @brief 匹配指定字符
     *
     * @param expected: 需要匹配的目标字符
     *
     * @return `true` 表示匹配成功
     */
    bool match(QChar expected)
    {
        if (position < input.size() && input.at(position) == expected) {
            ++position;
            return true;
        }
        return false;
    }

    /**
     * @brief 跳过表达式中的空白字符
     *
     * @param 无
     *
     * @return 无
     */
    void skipSpaces()
    {
        while (position < input.size() && input.at(position).isSpace()) {
            ++position;
        }
    }

    QString input;
    int position = 0;
};

} // namespace

/**
 * @brief 构造协议工具箱
 *
 * @param parent: 父对象指针
 *
 * @return 无
 */
ToolboxDialog::ToolboxDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    updateTexts();
    updateModbusHints();
    evaluateCalculator();
    convertTextToHex();
    convertHexToText();
    buildModbusFrame();
    parseModbusFrame();
    buildCustomFrame();
    parseCustomFrame();
    refreshProtocolSendState();
}

/**
 * @brief 切换界面语言
 *
 * @param chineseModeEnabled: `true` 为中文，`false` 为英文
 *
 * @return 无
 */
void ToolboxDialog::setChineseMode(bool chineseModeEnabled)
{
    chineseMode = chineseModeEnabled;
    updateTexts();
    updateModbusHints();
    buildModbusFrame();
    parseModbusFrame();
    buildCustomFrame();
    parseCustomFrame();
    refreshProtocolSendState();
}

/**
 * @brief 打开指定页签
 *
 * @param pageIndex: 需要显示的页签索引
 *
 * @return 无
 */
void ToolboxDialog::openPage(int pageIndex)
{
    tabWidget->setCurrentIndex(qBound(0, pageIndex, tabWidget->count() - 1));
    show();
    raise();
    activateWindow();
}

/**
 * @brief 设置串口发送链路是否可用
 *
 * @param ready: `true` 表示当前允许独立发送报文
 *
 * @return 无
 */
void ToolboxDialog::setTransportReady(bool ready)
{
    if (transportReady == ready) {
        return;
    }

    transportReady = ready;
    refreshProtocolSendState();
}

/**
 * @brief 根据当前语言返回对应文本
 *
 * @param chinese: 中文文本
 * @param english: 英文文本
 *
 * @return 当前语言下应显示的文本
 */
QString ToolboxDialog::uiText(const QString &chinese, const QString &english) const
{
    return chineseMode ? chinese : english;
}

/**
 * @brief 创建工具箱界面
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::buildUi()
{
    resize(860, 680);
    setModal(false);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    tabWidget = new QTabWidget(this);
    rootLayout->addWidget(tabWidget, 1);

    auto *calculatorPage = new QWidget(tabWidget);
    auto *calculatorLayout = new QVBoxLayout(calculatorPage);
    calculatorHintLabel = new QLabel(calculatorPage);
    calculatorHintLabel->setWordWrap(true);
    calculatorExpressionLabel = new QLabel(calculatorPage);
    calculatorExpressionEdit = new QLineEdit(calculatorPage);
    calculatorDecimalLabel = new QLabel(calculatorPage);
    calculatorDecimalEdit = new QLineEdit(calculatorPage);
    calculatorDecimalEdit->setReadOnly(true);
    calculatorHexLabel = new QLabel(calculatorPage);
    calculatorHexEdit = new QLineEdit(calculatorPage);
    calculatorHexEdit->setReadOnly(true);
    calculatorBinaryLabel = new QLabel(calculatorPage);
    calculatorBinaryEdit = new QLineEdit(calculatorPage);
    calculatorBinaryEdit->setReadOnly(true);
    calculatorEvaluateButton = new QPushButton(calculatorPage);
    calculatorClearButton = new QPushButton(calculatorPage);

    auto *calculatorForm = new QFormLayout;
    calculatorForm->addRow(calculatorExpressionLabel, calculatorExpressionEdit);
    calculatorForm->addRow(calculatorDecimalLabel, calculatorDecimalEdit);
    calculatorForm->addRow(calculatorHexLabel, calculatorHexEdit);
    calculatorForm->addRow(calculatorBinaryLabel, calculatorBinaryEdit);

    auto *calculatorButtonLayout = new QHBoxLayout;
    calculatorButtonLayout->addWidget(calculatorEvaluateButton);
    calculatorButtonLayout->addWidget(calculatorClearButton);
    calculatorButtonLayout->addStretch();

    calculatorLayout->addWidget(calculatorHintLabel);
    calculatorLayout->addLayout(calculatorForm);
    calculatorLayout->addLayout(calculatorButtonLayout);
    calculatorLayout->addStretch();
    tabWidget->addTab(calculatorPage, QString());

    auto *hexPage = new QWidget(tabWidget);
    auto *hexLayout = new QVBoxLayout(hexPage);
    auto *hexLabelLayout = new QHBoxLayout;
    textSourceLabel = new QLabel(hexPage);
    hexSourceLabel = new QLabel(hexPage);
    hexLabelLayout->addWidget(textSourceLabel, 1);
    hexLabelLayout->addWidget(hexSourceLabel, 1);
    textSourceEdit = new QPlainTextEdit(hexPage);
    hexSourceEdit = new QPlainTextEdit(hexPage);
    auto *hexEditLayout = new QHBoxLayout;
    hexEditLayout->addWidget(textSourceEdit, 1);
    hexEditLayout->addWidget(hexSourceEdit, 1);
    auto *hexButtonLayout = new QHBoxLayout;
    textToHexButton = new QPushButton(hexPage);
    hexToTextButton = new QPushButton(hexPage);
    hexClearButton = new QPushButton(hexPage);
    hexLoadButton = new QPushButton(hexPage);
    hexButtonLayout->addWidget(textToHexButton);
    hexButtonLayout->addWidget(hexToTextButton);
    hexButtonLayout->addWidget(hexClearButton);
    hexButtonLayout->addWidget(hexLoadButton);
    hexButtonLayout->addStretch();
    hexLayout->addLayout(hexLabelLayout);
    hexLayout->addLayout(hexEditLayout, 1);
    hexLayout->addLayout(hexButtonLayout);
    tabWidget->addTab(hexPage, QString());

    auto *modbusPage = new QWidget(tabWidget);
    auto *modbusLayout = new QVBoxLayout(modbusPage);
    modbusBuildLabel = new QLabel(modbusPage);
    modbusParseLabel = new QLabel(modbusPage);
    modbusFunctionHintLabel = new QLabel(modbusPage);
    modbusFunctionHintLabel->setWordWrap(true);
    modbusDataHintLabel = new QLabel(modbusPage);
    modbusDataHintLabel->setWordWrap(true);
    modbusSlaveLabel = new QLabel(modbusPage);
    modbusFunctionLabel = new QLabel(modbusPage);
    modbusAddressLabel = new QLabel(modbusPage);
    modbusQuantityLabel = new QLabel(modbusPage);
    modbusDataLabel = new QLabel(modbusPage);
    modbusFrameLabel = new QLabel(modbusPage);
    modbusParseInputLabel = new QLabel(modbusPage);
    modbusSlaveSpinBox = new QSpinBox(modbusPage);
    modbusSlaveSpinBox->setRange(1, 247);
    modbusSlaveSpinBox->setValue(1);
    modbusFunctionComboBox = new QComboBox(modbusPage);
    modbusAddressSpinBox = new QSpinBox(modbusPage);
    modbusAddressSpinBox->setRange(0, 65535);
    modbusQuantitySpinBox = new QSpinBox(modbusPage);
    modbusQuantitySpinBox->setRange(0, 65535);
    modbusQuantitySpinBox->setValue(2);
    modbusDataEdit = new QLineEdit(modbusPage);
    modbusFrameEdit = new QLineEdit(modbusPage);
    modbusFrameEdit->setReadOnly(true);
    modbusParseInputEdit = new QLineEdit(modbusPage);
    modbusParseResultEdit = new QPlainTextEdit(modbusPage);
    modbusParseResultEdit->setReadOnly(true);
    modbusBuildButton = new QPushButton(modbusPage);
    modbusLoadButton = new QPushButton(modbusPage);
    modbusParseButton = new QPushButton(modbusPage);
    modbusSendButton = new QPushButton(modbusPage);
    modbusLoopSendCheckBox = new QCheckBox(modbusPage);
    modbusIntervalLabel = new QLabel(modbusPage);
    modbusSendStatusLabel = new QLabel(modbusPage);
    modbusIntervalSpinBox = new QSpinBox(modbusPage);
    modbusIntervalSpinBox->setRange(10, 60000);
    modbusIntervalSpinBox->setValue(500);
    modbusIntervalSpinBox->setSuffix(QStringLiteral(" ms"));
    modbusSendTimer = new QTimer(this);
    modbusSendTimer->setInterval(modbusIntervalSpinBox->value());

    auto *modbusBuildForm = new QFormLayout;
    modbusBuildForm->addRow(modbusSlaveLabel, modbusSlaveSpinBox);
    modbusBuildForm->addRow(modbusFunctionLabel, modbusFunctionComboBox);
    modbusBuildForm->addRow(modbusAddressLabel, modbusAddressSpinBox);
    modbusBuildForm->addRow(modbusQuantityLabel, modbusQuantitySpinBox);
    modbusBuildForm->addRow(modbusDataLabel, modbusDataEdit);
    modbusBuildForm->addRow(modbusFrameLabel, modbusFrameEdit);

    auto *modbusBuildButtonLayout = new QHBoxLayout;
    modbusBuildButtonLayout->addWidget(modbusBuildButton);
    modbusBuildButtonLayout->addWidget(modbusLoadButton);
    modbusBuildButtonLayout->addWidget(modbusSendButton);
    modbusBuildButtonLayout->addStretch();

    auto *modbusLoopLayout = new QHBoxLayout;
    modbusLoopLayout->addWidget(modbusLoopSendCheckBox);
    modbusLoopLayout->addWidget(modbusIntervalLabel);
    modbusLoopLayout->addWidget(modbusIntervalSpinBox);
    modbusLoopLayout->addWidget(modbusSendStatusLabel, 1);

    auto *modbusBuildGroup = new QGroupBox(modbusPage);
    auto *modbusBuildGroupLayout = new QVBoxLayout(modbusBuildGroup);
    modbusBuildGroupLayout->addWidget(modbusBuildLabel);
    modbusBuildGroupLayout->addWidget(modbusFunctionHintLabel);
    modbusBuildGroupLayout->addWidget(modbusDataHintLabel);
    modbusBuildGroupLayout->addLayout(modbusBuildForm);
    modbusBuildGroupLayout->addLayout(modbusBuildButtonLayout);
    modbusBuildGroupLayout->addLayout(modbusLoopLayout);

    auto *modbusParseGroup = new QGroupBox(modbusPage);
    auto *modbusParseGroupLayout = new QVBoxLayout(modbusParseGroup);
    auto *modbusParseForm = new QFormLayout;
    modbusParseForm->addRow(modbusParseInputLabel, modbusParseInputEdit);
    modbusParseGroupLayout->addWidget(modbusParseLabel);
    modbusParseGroupLayout->addLayout(modbusParseForm);
    modbusParseGroupLayout->addWidget(modbusParseButton);
    modbusParseGroupLayout->addWidget(modbusParseResultEdit, 1);

    auto *modbusGroupLayout = new QHBoxLayout;
    modbusGroupLayout->addWidget(modbusBuildGroup, 1);
    modbusGroupLayout->addWidget(modbusParseGroup, 1);
    modbusLayout->addLayout(modbusGroupLayout, 1);
    modbusPage->hide();

    auto *customPage = new QWidget(tabWidget);
    auto *customLayout = new QVBoxLayout(customPage);
    customBuildLabel = new QLabel(customPage);
    customParseLabel = new QLabel(customPage);
    customBuildHintLabel = new QLabel(customPage);
    customBuildHintLabel->setWordWrap(true);
    customParseHintLabel = new QLabel(customPage);
    customParseHintLabel->setWordWrap(true);
    customHeaderLabel = new QLabel(customPage);
    customCommandLabel = new QLabel(customPage);
    customPayloadLabel = new QLabel(customPage);
    customChecksumLabel = new QLabel(customPage);
    customFooterLabel = new QLabel(customPage);
    customFrameLabel = new QLabel(customPage);
    customParseInputLabel = new QLabel(customPage);
    customCommandLengthLabel = new QLabel(customPage);
    customHeaderEdit = new QLineEdit(customPage);
    customCommandEdit = new QLineEdit(customPage);
    customPayloadEdit = new QLineEdit(customPage);
    customChecksumComboBox = new QComboBox(customPage);
    customFooterEdit = new QLineEdit(customPage);
    customFrameEdit = new QLineEdit(customPage);
    customFrameEdit->setReadOnly(true);
    customParseInputEdit = new QLineEdit(customPage);
    customCommandLengthSpinBox = new QSpinBox(customPage);
    customCommandLengthSpinBox->setRange(1, 8);
    customCommandLengthSpinBox->setValue(1);
    customParseResultEdit = new QPlainTextEdit(customPage);
    customParseResultEdit->setReadOnly(true);
    customBuildButton = new QPushButton(customPage);
    customLoadButton = new QPushButton(customPage);
    customParseButton = new QPushButton(customPage);
    customSendButton = new QPushButton(customPage);
    customLoopSendCheckBox = new QCheckBox(customPage);
    customIntervalLabel = new QLabel(customPage);
    customSendStatusLabel = new QLabel(customPage);
    customIntervalSpinBox = new QSpinBox(customPage);
    customIntervalSpinBox->setRange(10, 60000);
    customIntervalSpinBox->setValue(500);
    customIntervalSpinBox->setSuffix(QStringLiteral(" ms"));
    customSendTimer = new QTimer(this);
    customSendTimer->setInterval(customIntervalSpinBox->value());

    auto *customBuildForm = new QFormLayout;
    customBuildForm->addRow(customHeaderLabel, customHeaderEdit);
    customBuildForm->addRow(customCommandLabel, customCommandEdit);
    customBuildForm->addRow(customPayloadLabel, customPayloadEdit);
    customBuildForm->addRow(customChecksumLabel, customChecksumComboBox);
    customBuildForm->addRow(customFooterLabel, customFooterEdit);
    customBuildForm->addRow(customFrameLabel, customFrameEdit);

    auto *customBuildButtonLayout = new QHBoxLayout;
    customBuildButtonLayout->addWidget(customBuildButton);
    customBuildButtonLayout->addWidget(customLoadButton);
    customBuildButtonLayout->addWidget(customSendButton);
    customBuildButtonLayout->addStretch();

    auto *customLoopLayout = new QHBoxLayout;
    customLoopLayout->addWidget(customLoopSendCheckBox);
    customLoopLayout->addWidget(customIntervalLabel);
    customLoopLayout->addWidget(customIntervalSpinBox);
    customLoopLayout->addWidget(customSendStatusLabel, 1);

    auto *customBuildGroup = new QGroupBox(customPage);
    auto *customBuildGroupLayout = new QVBoxLayout(customBuildGroup);
    customBuildGroupLayout->addWidget(customBuildLabel);
    customBuildGroupLayout->addWidget(customBuildHintLabel);
    customBuildGroupLayout->addLayout(customBuildForm);
    customBuildGroupLayout->addLayout(customBuildButtonLayout);
    customBuildGroupLayout->addLayout(customLoopLayout);

    auto *customParseForm = new QFormLayout;
    customParseForm->addRow(customParseInputLabel, customParseInputEdit);
    customParseForm->addRow(customCommandLengthLabel, customCommandLengthSpinBox);

    auto *customParseGroup = new QGroupBox(customPage);
    auto *customParseGroupLayout = new QVBoxLayout(customParseGroup);
    customParseGroupLayout->addWidget(customParseLabel);
    customParseGroupLayout->addWidget(customParseHintLabel);
    customParseGroupLayout->addLayout(customParseForm);
    customParseGroupLayout->addWidget(customParseButton);
    customParseGroupLayout->addWidget(customParseResultEdit, 1);

    auto *customGroupLayout = new QHBoxLayout;
    customGroupLayout->addWidget(customBuildGroup, 1);
    customGroupLayout->addWidget(customParseGroup, 1);
    customLayout->addLayout(customGroupLayout, 1);
    tabWidget->addTab(customPage, QString());

    connect(calculatorEvaluateButton, &QPushButton::clicked, this, &ToolboxDialog::evaluateCalculator);
    connect(calculatorClearButton, &QPushButton::clicked, this, [this] {
        calculatorExpressionEdit->clear();
        calculatorDecimalEdit->clear();
        calculatorHexEdit->clear();
        calculatorBinaryEdit->clear();
    });
    connect(calculatorExpressionEdit, &QLineEdit::textChanged, this, [this] { evaluateCalculator(); });

    connect(textToHexButton, &QPushButton::clicked, this, &ToolboxDialog::convertTextToHex);
    connect(hexToTextButton, &QPushButton::clicked, this, &ToolboxDialog::convertHexToText);
    connect(hexClearButton, &QPushButton::clicked, this, [this] {
        textSourceEdit->clear();
        hexSourceEdit->clear();
    });
    connect(hexLoadButton, &QPushButton::clicked, this, [this] {
        emit requestLoadPayload(hexSourceEdit->toPlainText().trimmed(), true, false);
    });
    connect(textSourceEdit, &QPlainTextEdit::textChanged, this, &ToolboxDialog::convertTextToHex);
    connect(hexSourceEdit, &QPlainTextEdit::textChanged, this, &ToolboxDialog::convertHexToText);

    connect(modbusBuildButton, &QPushButton::clicked, this, &ToolboxDialog::buildModbusFrame);
    connect(modbusLoadButton, &QPushButton::clicked, this, [this] {
        emit requestLoadPayload(modbusFrameEdit->text().trimmed(), true, false);
    });
    connect(modbusSendButton, &QPushButton::clicked, this, &ToolboxDialog::sendModbusFrameNow);
    connect(modbusParseButton, &QPushButton::clicked, this, &ToolboxDialog::parseModbusFrame);
    connect(modbusLoopSendCheckBox, &QCheckBox::toggled, this, [this](bool) { refreshProtocolSendState(); });
    connect(modbusIntervalSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        modbusSendTimer->setInterval(value);
        refreshProtocolSendState();
    });
    connect(modbusSlaveSpinBox, &QSpinBox::valueChanged, this, [this](int) { buildModbusFrame(); });
    connect(modbusFunctionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateModbusHints();
        buildModbusFrame();
        parseModbusFrame();
    });
    connect(modbusAddressSpinBox, &QSpinBox::valueChanged, this, [this](int) { buildModbusFrame(); });
    connect(modbusQuantitySpinBox, &QSpinBox::valueChanged, this, [this](int) { buildModbusFrame(); });
    connect(modbusDataEdit, &QLineEdit::textChanged, this, [this](const QString &) { buildModbusFrame(); });
    connect(modbusFrameEdit, &QLineEdit::textChanged, this, [this](const QString &) { refreshProtocolSendState(); });
    connect(modbusParseInputEdit, &QLineEdit::textChanged, this, [this](const QString &) { parseModbusFrame(); });
    connect(modbusSendTimer, &QTimer::timeout, this, &ToolboxDialog::sendModbusFrameNow);

    connect(customBuildButton, &QPushButton::clicked, this, &ToolboxDialog::buildCustomFrame);
    connect(customLoadButton, &QPushButton::clicked, this, [this] {
        emit requestLoadPayload(customFrameEdit->text().trimmed(), true, false);
    });
    connect(customSendButton, &QPushButton::clicked, this, &ToolboxDialog::sendCustomFrameNow);
    connect(customParseButton, &QPushButton::clicked, this, &ToolboxDialog::parseCustomFrame);
    connect(customLoopSendCheckBox, &QCheckBox::toggled, this, [this](bool) { refreshProtocolSendState(); });
    connect(customIntervalSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        customSendTimer->setInterval(value);
        refreshProtocolSendState();
    });
    connect(customHeaderEdit, &QLineEdit::textChanged, this, [this](const QString &) { buildCustomFrame(); });
    connect(customCommandEdit, &QLineEdit::textChanged, this, [this](const QString &) { buildCustomFrame(); });
    connect(customPayloadEdit, &QLineEdit::textChanged, this, [this](const QString &) { buildCustomFrame(); });
    connect(customChecksumComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        buildCustomFrame();
        parseCustomFrame();
    });
    connect(customFooterEdit, &QLineEdit::textChanged, this, [this](const QString &) { buildCustomFrame(); });
    connect(customFrameEdit, &QLineEdit::textChanged, this, [this](const QString &) { refreshProtocolSendState(); });
    connect(customParseInputEdit, &QLineEdit::textChanged, this, [this](const QString &) { parseCustomFrame(); });
    connect(customCommandLengthSpinBox, &QSpinBox::valueChanged, this, [this](int) { parseCustomFrame(); });
    connect(customSendTimer, &QTimer::timeout, this, &ToolboxDialog::sendCustomFrameNow);
}

/**
 * @brief 刷新所有界面文字
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::updateTexts()
{
    setWindowTitle(uiText(QStringLiteral("协议与数据工具箱"), QStringLiteral("Protocol Toolbox")));

    tabWidget->setTabText(0, uiText(QStringLiteral("计算器"), QStringLiteral("Calculator")));
    tabWidget->setTabText(1, uiText(QStringLiteral("文本 / HEX"), QStringLiteral("Text / HEX")));
    tabWidget->setTabText(2, uiText(QStringLiteral("自定义协议"), QStringLiteral("Custom Protocol")));

    calculatorHintLabel->setText(
        uiText(QStringLiteral("支持 + - * / % 和括号，适合快速计算地址偏移、寄存器长度和分包范围。"),
               QStringLiteral("Supports + - * / % and parentheses for quick address, length and offset calculations.")));
    calculatorExpressionLabel->setText(uiText(QStringLiteral("表达式"), QStringLiteral("Expression")));
    calculatorDecimalLabel->setText(uiText(QStringLiteral("十进制结果"), QStringLiteral("Decimal")));
    calculatorHexLabel->setText(uiText(QStringLiteral("十六进制结果"), QStringLiteral("Hex")));
    calculatorBinaryLabel->setText(uiText(QStringLiteral("二进制结果"), QStringLiteral("Binary")));
    calculatorEvaluateButton->setText(uiText(QStringLiteral("计算"), QStringLiteral("Evaluate")));
    calculatorClearButton->setText(uiText(QStringLiteral("清空"), QStringLiteral("Clear")));

    textSourceLabel->setText(uiText(QStringLiteral("文本内容"), QStringLiteral("Text")));
    hexSourceLabel->setText(uiText(QStringLiteral("HEX 内容"), QStringLiteral("HEX")));
    textToHexButton->setText(uiText(QStringLiteral("文本转 HEX"), QStringLiteral("Text -> HEX")));
    hexToTextButton->setText(uiText(QStringLiteral("HEX 转文本"), QStringLiteral("HEX -> Text")));
    hexClearButton->setText(uiText(QStringLiteral("清空"), QStringLiteral("Clear")));
    hexLoadButton->setText(uiText(QStringLiteral("HEX 装入发送区"), QStringLiteral("Load HEX To Send")));

    modbusBuildLabel->setText(uiText(QStringLiteral("实时组包"), QStringLiteral("Realtime Builder")));
    modbusParseLabel->setText(uiText(QStringLiteral("实时解析"), QStringLiteral("Realtime Parser")));
    modbusSlaveLabel->setText(uiText(QStringLiteral("站号"), QStringLiteral("Slave")));
    modbusFunctionLabel->setText(uiText(QStringLiteral("功能码"), QStringLiteral("Function")));
    modbusAddressLabel->setText(uiText(QStringLiteral("起始地址"), QStringLiteral("Address")));
    modbusFrameLabel->setText(uiText(QStringLiteral("输出帧"), QStringLiteral("Frame")));
    modbusParseInputLabel->setText(uiText(QStringLiteral("待解析帧"), QStringLiteral("Frame To Parse")));
    modbusBuildButton->setText(uiText(QStringLiteral("立即刷新"), QStringLiteral("Refresh Now")));
    modbusLoadButton->setText(uiText(QStringLiteral("装入发送区"), QStringLiteral("Load To Send")));
    modbusParseButton->setText(uiText(QStringLiteral("重新解析"), QStringLiteral("Parse Again")));
    modbusSendButton->setText(uiText(QStringLiteral("独立发送"), QStringLiteral("Send Direct")));
    modbusLoopSendCheckBox->setText(uiText(QStringLiteral("定时发送"), QStringLiteral("Loop Send")));
    modbusIntervalLabel->setText(uiText(QStringLiteral("间隔"), QStringLiteral("Interval")));
    populateModbusFunctionOptions();

    customBuildLabel->setText(uiText(QStringLiteral("实时组包"), QStringLiteral("Realtime Builder")));
    customParseLabel->setText(uiText(QStringLiteral("实时解析"), QStringLiteral("Realtime Parser")));
    customBuildHintLabel->setText(
        uiText(QStringLiteral("适合带帧头、命令字、载荷和校验尾的私有协议。输入变化后，报文会实时重组。"),
               QStringLiteral("Useful for private frames with header, command, payload and checksum. The frame updates immediately when inputs change.")));
    customParseHintLabel->setText(
        uiText(QStringLiteral("解析区会按“帧头 + 命令字 + 载荷 + 校验 + 帧尾”的结构实时拆分。"),
               QStringLiteral("The parser splits frames in realtime as header + command + payload + checksum + footer.")));
    customHeaderLabel->setText(uiText(QStringLiteral("帧头 HEX"), QStringLiteral("Header HEX")));
    customCommandLabel->setText(uiText(QStringLiteral("命令字 HEX"), QStringLiteral("Command HEX")));
    customPayloadLabel->setText(uiText(QStringLiteral("载荷 HEX"), QStringLiteral("Payload HEX")));
    customChecksumLabel->setText(uiText(QStringLiteral("校验算法"), QStringLiteral("Checksum")));
    customFooterLabel->setText(uiText(QStringLiteral("帧尾 HEX"), QStringLiteral("Footer HEX")));
    customFrameLabel->setText(uiText(QStringLiteral("输出帧"), QStringLiteral("Frame")));
    customParseInputLabel->setText(uiText(QStringLiteral("待解析帧"), QStringLiteral("Frame To Parse")));
    customCommandLengthLabel->setText(uiText(QStringLiteral("命令字长度"), QStringLiteral("Command Length")));
    customBuildButton->setText(uiText(QStringLiteral("立即刷新"), QStringLiteral("Refresh Now")));
    customLoadButton->setText(uiText(QStringLiteral("装入发送区"), QStringLiteral("Load To Send")));
    customParseButton->setText(uiText(QStringLiteral("重新解析"), QStringLiteral("Parse Again")));
    customSendButton->setText(uiText(QStringLiteral("独立发送"), QStringLiteral("Send Direct")));
    customLoopSendCheckBox->setText(uiText(QStringLiteral("定时发送"), QStringLiteral("Loop Send")));
    customIntervalLabel->setText(uiText(QStringLiteral("间隔"), QStringLiteral("Interval")));
    populateCustomChecksumOptions();
    refreshProtocolSendState();
}

/**
 * @brief 重建 Modbus 功能码下拉框文字
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::populateModbusFunctionOptions()
{
    const int currentCode = modbusFunctionComboBox->currentData().toInt();
    QSignalBlocker blocker(modbusFunctionComboBox);
    modbusFunctionComboBox->clear();

    const auto &descriptors = modbusFunctionDescriptors();
    for (const auto &descriptor : descriptors) {
        modbusFunctionComboBox->addItem(
            QString::fromUtf8(chineseMode ? descriptor.chineseName : descriptor.englishName),
            descriptor.code);
    }

    const int targetIndex = modbusFunctionComboBox->findData(currentCode);
    modbusFunctionComboBox->setCurrentIndex(targetIndex >= 0 ? targetIndex : 2);
}

/**
 * @brief 重建自定义校验算法下拉框文字
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::populateCustomChecksumOptions()
{
    const int currentValue = customChecksumComboBox->currentData().toInt();
    QSignalBlocker blocker(customChecksumComboBox);
    customChecksumComboBox->clear();
    customChecksumComboBox->addItem(uiText(QStringLiteral("无"), QStringLiteral("None")), 0);
    customChecksumComboBox->addItem(QStringLiteral("SUM8"), 1);
    customChecksumComboBox->addItem(QStringLiteral("XOR8"), 2);
    customChecksumComboBox->addItem(QStringLiteral("CRC16"), 3);

    const int targetIndex = customChecksumComboBox->findData(currentValue);
    customChecksumComboBox->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
}

/**
 * @brief 刷新 Modbus 页的功能码说明
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::updateModbusHints()
{
    const int functionCode = modbusFunctionComboBox->currentData().toInt();
    modbusFunctionHintLabel->setText(modbusFunctionHint(functionCode, chineseMode));

    switch (functionCode) {
    case 0x05:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数值"), QStringLiteral("Value")));
        modbusDataLabel->setText(uiText(QStringLiteral("附加数据"), QStringLiteral("Extra Data")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("附加数据字段对 05 功能码不使用，可保持为空。数值填 0 表示 OFF，填非 0 表示 ON。"),
                   QStringLiteral("The extra-data field is unused by function 05 and may stay empty. Value 0 means OFF, non-zero means ON.")));
        break;
    case 0x06:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数值"), QStringLiteral("Value")));
        modbusDataLabel->setText(uiText(QStringLiteral("附加数据"), QStringLiteral("Extra Data")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("附加数据字段对 06 功能码不使用，可保持为空。数值字段表示 0~65535 的 16 位寄存器值。"),
                   QStringLiteral("The extra-data field is unused by function 06. Value is the 16-bit register content in the range 0~65535.")));
        break;
    case 0x0F:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数量"), QStringLiteral("Quantity")));
        modbusDataLabel->setText(uiText(QStringLiteral("线圈位图"), QStringLiteral("Coil Bitmap")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("数据字节按位表示线圈状态，最低位对应起始地址的第一点。"),
                   QStringLiteral("Data bytes are packed coil states. The least significant bit maps to the first addressed coil.")));
        break;
    case 0x10:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数量"), QStringLiteral("Quantity")));
        modbusDataLabel->setText(uiText(QStringLiteral("寄存器数据"), QStringLiteral("Register Data")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("每个寄存器占 2 字节，高字节在前，例如 12 34 表示 0x1234。"),
                   QStringLiteral("Each register takes 2 bytes in big-endian order. Example: 12 34 means 0x1234.")));
        break;
    case 0x11:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数量"), QStringLiteral("Quantity")));
        modbusDataLabel->setText(uiText(QStringLiteral("附加数据"), QStringLiteral("Extra Data")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("该命令无地址、数量和数据负载，软件仅会发送站号、功能码和 CRC。"),
                   QStringLiteral("This command has no address, quantity or payload. The tool only sends slave, function and CRC.")));
        break;
    case 0x16:
        modbusQuantityLabel->setText(uiText(QStringLiteral("预留"), QStringLiteral("Reserved")));
        modbusDataLabel->setText(uiText(QStringLiteral("AND/OR 掩码"), QStringLiteral("AND/OR Masks")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("数据字段固定为 4 字节：AND Mask(2) + OR Mask(2)。数量字段不会参与报文。"),
                   QStringLiteral("Data must be 4 bytes: AND mask (2) + OR mask (2). The quantity field is ignored.")));
        break;
    case 0x17:
        modbusQuantityLabel->setText(uiText(QStringLiteral("读取数量"), QStringLiteral("Read Count")));
        modbusDataLabel->setText(uiText(QStringLiteral("写起始地址 + 写数据"), QStringLiteral("Write Address + Data")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("示例：00 10 12 34 56 78，表示写起始地址 0x0010，随后写入两个寄存器 0x1234 和 0x5678。"),
                   QStringLiteral("Example: 00 10 12 34 56 78 means write-start 0x0010 followed by two registers 0x1234 and 0x5678.")));
        break;
    default:
        modbusQuantityLabel->setText(uiText(QStringLiteral("数量"), QStringLiteral("Quantity")));
        modbusDataLabel->setText(uiText(QStringLiteral("数据字节"), QStringLiteral("Data Bytes")));
        modbusDataHintLabel->setText(
            uiText(QStringLiteral("读命令通常忽略数据字段；写命令会根据功能码解释该字段。"),
                   QStringLiteral("Read commands usually ignore the data field, while write commands interpret it by function.")));
        break;
    }
}

/**
 * @brief 刷新计算器输出
 *
 * @param 无
 *
 * @return 无
 */
/**
 * @brief 刷新协议页独立发送状态
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::refreshProtocolSendState()
{
    QString errorMessage;
    const QByteArray modbusFrame = parseHexString(modbusFrameEdit->text().trimmed(), chineseMode, &errorMessage);
    const bool modbusFrameReady = errorMessage.isEmpty() && !modbusFrame.isEmpty();

    modbusSendButton->setEnabled(transportReady && modbusFrameReady);
    modbusIntervalSpinBox->setEnabled(modbusLoopSendCheckBox->isChecked());
    if (modbusLoopSendCheckBox->isChecked() && transportReady && modbusFrameReady) {
        modbusSendTimer->start(modbusIntervalSpinBox->value());
    } else {
        modbusSendTimer->stop();
    }

    if (!transportReady) {
        modbusSendStatusLabel->setText(uiText(QStringLiteral("等待串口连接"), QStringLiteral("Waiting for port")));
    } else if (!modbusFrameReady) {
        modbusSendStatusLabel->setText(uiText(QStringLiteral("报文未就绪"), QStringLiteral("Frame not ready")));
    } else if (modbusLoopSendCheckBox->isChecked()) {
        modbusSendStatusLabel->setText(
            uiText(QStringLiteral("定时发送中 %1 ms"), QStringLiteral("Loop sending %1 ms"))
                .arg(modbusIntervalSpinBox->value()));
    } else {
        modbusSendStatusLabel->setText(uiText(QStringLiteral("可独立发送"), QStringLiteral("Ready to send")));
    }

    errorMessage.clear();
    const QByteArray customFrame = parseHexString(customFrameEdit->text().trimmed(), chineseMode, &errorMessage);
    const bool customFrameReady = errorMessage.isEmpty() && !customFrame.isEmpty();

    customSendButton->setEnabled(transportReady && customFrameReady);
    customIntervalSpinBox->setEnabled(customLoopSendCheckBox->isChecked());
    if (customLoopSendCheckBox->isChecked() && transportReady && customFrameReady) {
        customSendTimer->start(customIntervalSpinBox->value());
    } else {
        customSendTimer->stop();
    }

    if (!transportReady) {
        customSendStatusLabel->setText(uiText(QStringLiteral("等待串口连接"), QStringLiteral("Waiting for port")));
    } else if (!customFrameReady) {
        customSendStatusLabel->setText(uiText(QStringLiteral("报文未就绪"), QStringLiteral("Frame not ready")));
    } else if (customLoopSendCheckBox->isChecked()) {
        customSendStatusLabel->setText(
            uiText(QStringLiteral("定时发送中 %1 ms"), QStringLiteral("Loop sending %1 ms"))
                .arg(customIntervalSpinBox->value()));
    } else {
        customSendStatusLabel->setText(uiText(QStringLiteral("可独立发送"), QStringLiteral("Ready to send")));
    }
}

/**
 * @brief 刷新计算器输出
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::evaluateCalculator()
{
    const QString expression = calculatorExpressionEdit->text().trimmed();
    if (expression.isEmpty()) {
        calculatorDecimalEdit->clear();
        calculatorHexEdit->clear();
        calculatorBinaryEdit->clear();
        return;
    }

    ExpressionParser parser(expression);
    ExpressionResult result = parser.parse();
    if (!result.ok) {
        calculatorDecimalEdit->setText(result.error);
        calculatorHexEdit->clear();
        calculatorBinaryEdit->clear();
        return;
    }

    calculatorDecimalEdit->setText(QString::number(result.value, 'g', 15));

    const double rounded = std::round(result.value);
    if (std::fabs(result.value - rounded) < 1e-9
        && rounded >= static_cast<double>(std::numeric_limits<qint64>::min())
        && rounded <= static_cast<double>(std::numeric_limits<qint64>::max())) {
        const qint64 integerValue = static_cast<qint64>(rounded);
        calculatorHexEdit->setText(QStringLiteral("0x%1").arg(qulonglong(integerValue), 0, 16).toUpper());
        calculatorBinaryEdit->setText(QString::number(qulonglong(integerValue), 2));
    } else {
        calculatorHexEdit->setText(uiText(QStringLiteral("仅整数显示"), QStringLiteral("Integers only")));
        calculatorBinaryEdit->setText(uiText(QStringLiteral("仅整数显示"), QStringLiteral("Integers only")));
    }
}

/**
 * @brief 实时把文本转成 HEX
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::convertTextToHex()
{
    QSignalBlocker blocker(hexSourceEdit);
    hexSourceEdit->setPlainText(formatHex(textSourceEdit->toPlainText().toLocal8Bit()));
}

/**
 * @brief 实时把 HEX 转成文本
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::convertHexToText()
{
    const QString source = hexSourceEdit->toPlainText().trimmed();
    if (source.isEmpty()) {
        QSignalBlocker blocker(textSourceEdit);
        textSourceEdit->clear();
        return;
    }

    QString errorMessage;
    const QByteArray payload = parseHexString(source, chineseMode, &errorMessage);
    QSignalBlocker blocker(textSourceEdit);
    if (!errorMessage.isEmpty()) {
        textSourceEdit->setPlainText(errorMessage);
        return;
    }
    textSourceEdit->setPlainText(QString::fromLocal8Bit(payload));
}

/**
 * @brief 生成 Modbus RTU 报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::buildModbusFrame()
{
    QByteArray frame;
    frame.append(static_cast<char>(modbusSlaveSpinBox->value()));

    const int functionCode = modbusFunctionComboBox->currentData().toInt();
    frame.append(static_cast<char>(functionCode));

    QString errorMessage;
    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, static_cast<quint16>(modbusQuantitySpinBox->value()));
        break;
    case 0x05:
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, modbusQuantitySpinBox->value() == 0 ? 0x0000u : 0xFF00u);
        break;
    case 0x06:
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, static_cast<quint16>(modbusQuantitySpinBox->value()));
        break;
    case 0x0F: {
        const quint16 quantity = static_cast<quint16>(modbusQuantitySpinBox->value());
        const QByteArray dataBytes = parseHexString(modbusDataEdit->text(), chineseMode, &errorMessage);
        if (!errorMessage.isEmpty()) {
            modbusFrameEdit->setText(errorMessage);
            return;
        }
        const int expectedBytes = (quantity + 7) / 8;
        if (dataBytes.size() != expectedBytes) {
            modbusFrameEdit->setText(
                uiText(QStringLiteral("写多个线圈时，数据字节数必须等于 ceil(数量 / 8)。"),
                       QStringLiteral("For function 0x0F, byte count must equal ceil(quantity / 8).")));
            return;
        }
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, quantity);
        frame.append(static_cast<char>(dataBytes.size()));
        frame.append(dataBytes);
        break;
    }
    case 0x10: {
        const quint16 quantity = static_cast<quint16>(modbusQuantitySpinBox->value());
        const QByteArray dataBytes = parseHexString(modbusDataEdit->text(), chineseMode, &errorMessage);
        if (!errorMessage.isEmpty()) {
            modbusFrameEdit->setText(errorMessage);
            return;
        }
        const int expectedBytes = quantity * 2;
        if (dataBytes.size() != expectedBytes) {
            modbusFrameEdit->setText(
                uiText(QStringLiteral("写多个寄存器时，数据字节数必须等于 数量 x 2。"),
                       QStringLiteral("For function 0x10, byte count must equal quantity x 2.")));
            return;
        }
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, quantity);
        frame.append(static_cast<char>(dataBytes.size()));
        frame.append(dataBytes);
        break;
    }
    case 0x11:
        break;
    case 0x16: {
        const QByteArray maskBytes = parseHexString(modbusDataEdit->text(), chineseMode, &errorMessage);
        if (!errorMessage.isEmpty()) {
            modbusFrameEdit->setText(errorMessage);
            return;
        }
        if (maskBytes.size() != 4) {
            modbusFrameEdit->setText(
                uiText(QStringLiteral("掩码写寄存器必须填写 4 个字节：AND Mask + OR Mask。"),
                       QStringLiteral("Function 0x16 requires exactly 4 bytes: AND mask + OR mask.")));
            return;
        }
        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        frame.append(maskBytes);
        break;
    }
    case 0x17: {
        const QByteArray extraBytes = parseHexString(modbusDataEdit->text(), chineseMode, &errorMessage);
        if (!errorMessage.isEmpty()) {
            modbusFrameEdit->setText(errorMessage);
            return;
        }
        if (extraBytes.size() < 2 || ((extraBytes.size() - 2) % 2) != 0) {
            modbusFrameEdit->setText(
                uiText(QStringLiteral("23 功能码的数据字段需为：写起始地址(2字节) + N 个寄存器数据(偶数字节)。"),
                       QStringLiteral("Function 0x17 expects: write start address (2 bytes) + N register bytes (even byte count).")));
            return;
        }

        const QByteArray writePayload = extraBytes.mid(2);
        const quint16 writeQuantity = static_cast<quint16>(writePayload.size() / 2);
        if (writeQuantity == 0) {
            modbusFrameEdit->setText(
                uiText(QStringLiteral("23 功能码至少需要写入 1 个寄存器。"),
                       QStringLiteral("Function 0x17 requires at least one register to write.")));
            return;
        }

        appendBigEndian16(frame, static_cast<quint16>(modbusAddressSpinBox->value()));
        appendBigEndian16(frame, static_cast<quint16>(modbusQuantitySpinBox->value()));
        frame.append(extraBytes.left(2));
        appendBigEndian16(frame, writeQuantity);
        frame.append(static_cast<char>(writePayload.size()));
        frame.append(writePayload);
        break;
    }
    default:
        modbusFrameEdit->setText(uiText(QStringLiteral("当前功能码暂不支持。"),
                                        QStringLiteral("This function code is not supported yet.")));
        return;
    }

    const quint16 crc = modbusCrc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    modbusFrameEdit->setText(formatHex(frame));
}

/**
 * @brief 解析 Modbus RTU 报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::parseModbusFrame()
{
    const QString input = modbusParseInputEdit->text().trimmed();
    if (input.isEmpty()) {
        modbusParseResultEdit->clear();
        return;
    }

    QString errorMessage;
    const QByteArray frame = parseHexString(input, chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        modbusParseResultEdit->setPlainText(errorMessage);
        return;
    }

    if (frame.size() < 4) {
        modbusParseResultEdit->setPlainText(
            uiText(QStringLiteral("帧长度不足，至少需要站号、功能码和 CRC。"),
                   QStringLiteral("Frame is too short. Need slave, function and CRC.")));
        return;
    }

    const QByteArray payload = frame.left(frame.size() - 2);
    const quint16 receivedCrc = static_cast<quint8>(frame.at(frame.size() - 2))
                                | (static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 1))) << 8);
    const quint16 computedCrc = modbusCrc16(payload);
    const quint8 functionCode = static_cast<quint8>(frame.at(1));

    QStringList lines;
    lines << uiText(QStringLiteral("站号: %1"), QStringLiteral("Slave: %1"))
                 .arg(static_cast<quint8>(frame.at(0)));
    lines << uiText(QStringLiteral("功能码: 0x%1 - %2"), QStringLiteral("Function: 0x%1 - %2"))
                 .arg(functionCode, 2, 16, QLatin1Char('0'))
                 .arg(modbusFunctionName(functionCode & 0x7F, chineseMode))
                 .toUpper();
    lines << uiText(QStringLiteral("说明: %1"), QStringLiteral("Note: %1"))
                 .arg(modbusFunctionHint(functionCode & 0x7F, chineseMode));
    lines << uiText(QStringLiteral("CRC: %1 (%2)"), QStringLiteral("CRC: %1 (%2)"))
                 .arg(QStringLiteral("0x%1").arg(receivedCrc, 4, 16, QLatin1Char('0')).toUpper())
                 .arg(receivedCrc == computedCrc
                          ? uiText(QStringLiteral("校验通过"), QStringLiteral("valid"))
                          : uiText(QStringLiteral("校验失败"), QStringLiteral("invalid")));

    if (functionCode >= 0x80) {
        const quint8 exceptionCode = frame.size() >= 5 ? static_cast<quint8>(frame.at(2)) : 0x00;
        lines << uiText(QStringLiteral("异常码: 0x%1"), QStringLiteral("Exception Code: 0x%1"))
                     .arg(exceptionCode, 2, 16, QLatin1Char('0')).toUpper();
        lines << uiText(QStringLiteral("异常含义: %1"), QStringLiteral("Exception: %1"))
                     .arg(modbusExceptionDescription(exceptionCode, chineseMode));
        modbusParseResultEdit->setPlainText(lines.join(QLatin1Char('\n')));
        return;
    }

    switch (functionCode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
        if (frame.size() == 8) {
            lines << uiText(QStringLiteral("类型: 请求帧"), QStringLiteral("Type: request"));
            lines << uiText(QStringLiteral("起始地址: %1"), QStringLiteral("Start Address: %1"))
                         .arg(readBigEndian16(frame, 2));
            lines << uiText(QStringLiteral("读取数量: %1"), QStringLiteral("Quantity: %1"))
                         .arg(readBigEndian16(frame, 4));
        } else if (frame.size() >= 5) {
            lines << uiText(QStringLiteral("类型: 响应帧"), QStringLiteral("Type: response"));
            const int byteCount = static_cast<quint8>(frame.at(2));
            lines << uiText(QStringLiteral("字节数: %1"), QStringLiteral("Byte Count: %1")).arg(byteCount);
            lines << uiText(QStringLiteral("数据: %1"), QStringLiteral("Data: %1"))
                         .arg(formatHex(frame.mid(3, qMax(0, frame.size() - 5))));
        }
        break;
    case 0x05:
        lines << uiText(QStringLiteral("类型: 写单线圈请求/响应"), QStringLiteral("Type: write single coil request/response"));
        lines << uiText(QStringLiteral("线圈地址: %1"), QStringLiteral("Coil Address: %1"))
                     .arg(readBigEndian16(frame, 2));
        lines << uiText(QStringLiteral("线圈状态: %1"), QStringLiteral("Coil State: %1"))
                     .arg(readBigEndian16(frame, 4) == 0xFF00
                              ? uiText(QStringLiteral("ON"), QStringLiteral("ON"))
                              : uiText(QStringLiteral("OFF"), QStringLiteral("OFF")));
        break;
    case 0x06:
        lines << uiText(QStringLiteral("类型: 写单寄存器请求/响应"), QStringLiteral("Type: write single register request/response"));
        lines << uiText(QStringLiteral("寄存器地址: %1"), QStringLiteral("Register Address: %1"))
                     .arg(readBigEndian16(frame, 2));
        lines << uiText(QStringLiteral("写入数值: %1 (0x%2)"), QStringLiteral("Value: %1 (0x%2)"))
                     .arg(readBigEndian16(frame, 4))
                     .arg(readBigEndian16(frame, 4), 4, 16, QLatin1Char('0')).toUpper();
        break;
    case 0x0F:
    case 0x10:
        if (frame.size() == 8) {
            lines << uiText(QStringLiteral("类型: 响应帧"), QStringLiteral("Type: response"));
            lines << uiText(QStringLiteral("起始地址: %1"), QStringLiteral("Start Address: %1"))
                         .arg(readBigEndian16(frame, 2));
            lines << uiText(QStringLiteral("完成数量: %1"), QStringLiteral("Quantity: %1"))
                         .arg(readBigEndian16(frame, 4));
        } else if (frame.size() >= 9) {
            lines << uiText(QStringLiteral("类型: 请求帧"), QStringLiteral("Type: request"));
            lines << uiText(QStringLiteral("起始地址: %1"), QStringLiteral("Start Address: %1"))
                         .arg(readBigEndian16(frame, 2));
            lines << uiText(QStringLiteral("数量: %1"), QStringLiteral("Quantity: %1"))
                         .arg(readBigEndian16(frame, 4));
            lines << uiText(QStringLiteral("字节数: %1"), QStringLiteral("Byte Count: %1"))
                         .arg(static_cast<quint8>(frame.at(6)));
            lines << uiText(QStringLiteral("数据: %1"), QStringLiteral("Data: %1"))
                         .arg(formatHex(frame.mid(7, qMax(0, frame.size() - 9))));
        }
        break;
    case 0x11:
        if (frame.size() == 4) {
            lines << uiText(QStringLiteral("类型: 请求帧"), QStringLiteral("Type: request"));
            lines << uiText(QStringLiteral("说明: 该请求不带额外负载。"), QStringLiteral("Note: this request carries no extra payload."));
        } else {
            lines << uiText(QStringLiteral("类型: 响应帧"), QStringLiteral("Type: response"));
            const int byteCount = static_cast<quint8>(frame.at(2));
            lines << uiText(QStringLiteral("字节数: %1"), QStringLiteral("Byte Count: %1")).arg(byteCount);
            lines << uiText(QStringLiteral("从站信息: %1"), QStringLiteral("Slave Info: %1"))
                         .arg(formatHex(frame.mid(3, qMax(0, frame.size() - 5))));
        }
        break;
    case 0x16:
        if (frame.size() >= 10) {
            lines << uiText(QStringLiteral("寄存器地址: %1"), QStringLiteral("Register Address: %1"))
                         .arg(readBigEndian16(frame, 2));
            lines << uiText(QStringLiteral("AND Mask: 0x%1"), QStringLiteral("AND Mask: 0x%1"))
                         .arg(readBigEndian16(frame, 4), 4, 16, QLatin1Char('0')).toUpper();
            lines << uiText(QStringLiteral("OR Mask: 0x%1"), QStringLiteral("OR Mask: 0x%1"))
                         .arg(readBigEndian16(frame, 6), 4, 16, QLatin1Char('0')).toUpper();
        }
        break;
    case 0x17:
        if (frame.size() >= 13) {
            lines << uiText(QStringLiteral("类型: 请求帧"), QStringLiteral("Type: request"));
            lines << uiText(QStringLiteral("读取起始地址: %1"), QStringLiteral("Read Start: %1"))
                         .arg(readBigEndian16(frame, 2));
            lines << uiText(QStringLiteral("读取数量: %1"), QStringLiteral("Read Quantity: %1"))
                         .arg(readBigEndian16(frame, 4));
            lines << uiText(QStringLiteral("写起始地址: %1"), QStringLiteral("Write Start: %1"))
                         .arg(readBigEndian16(frame, 6));
            lines << uiText(QStringLiteral("写数量: %1"), QStringLiteral("Write Quantity: %1"))
                         .arg(readBigEndian16(frame, 8));
            lines << uiText(QStringLiteral("写字节数: %1"), QStringLiteral("Write Byte Count: %1"))
                         .arg(static_cast<quint8>(frame.at(10)));
            lines << uiText(QStringLiteral("写数据: %1"), QStringLiteral("Write Data: %1"))
                         .arg(formatHex(frame.mid(11, qMax(0, frame.size() - 13))));
        } else if (frame.size() >= 5) {
            lines << uiText(QStringLiteral("类型: 响应帧"), QStringLiteral("Type: response"));
            lines << uiText(QStringLiteral("字节数: %1"), QStringLiteral("Byte Count: %1"))
                         .arg(static_cast<quint8>(frame.at(2)));
            lines << uiText(QStringLiteral("读取数据: %1"), QStringLiteral("Read Data: %1"))
                         .arg(formatHex(frame.mid(3, qMax(0, frame.size() - 5))));
        }
        break;
    default:
        lines << uiText(QStringLiteral("原始负载: %1"), QStringLiteral("Raw Payload: %1"))
                     .arg(formatHex(payload.mid(2)));
        break;
    }

    modbusParseResultEdit->setPlainText(lines.join(QLatin1Char('\n')));
}

/**
 * @brief 生成自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
/**
 * @brief 立即独立发送当前 Modbus 报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::sendModbusFrameNow()
{
    if (!transportReady) {
        refreshProtocolSendState();
        return;
    }

    QString errorMessage;
    parseHexString(modbusFrameEdit->text().trimmed(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty() || modbusFrameEdit->text().trimmed().isEmpty()) {
        refreshProtocolSendState();
        return;
    }

    emit requestSendPayload(modbusFrameEdit->text().trimmed(), true, false);
}

/**
 * @brief 生成自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::buildCustomFrame()
{
    const QString emptyMessage = uiText(QStringLiteral("请输入至少一个命令字节。"),
                                        QStringLiteral("Enter at least one command byte."));

    if (customHeaderEdit->text().trimmed().isEmpty()
        && customCommandEdit->text().trimmed().isEmpty()
        && customPayloadEdit->text().trimmed().isEmpty()
        && customFooterEdit->text().trimmed().isEmpty()) {
        customFrameEdit->clear();
        return;
    }

    QString errorMessage;
    const QByteArray header = parseHexString(customHeaderEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customFrameEdit->setText(errorMessage);
        return;
    }

    const QByteArray command = parseHexString(customCommandEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customFrameEdit->setText(errorMessage);
        return;
    }
    if (command.isEmpty()) {
        customFrameEdit->setText(emptyMessage);
        return;
    }

    const QByteArray payload = parseHexString(customPayloadEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customFrameEdit->setText(errorMessage);
        return;
    }

    const QByteArray footer = parseHexString(customFooterEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customFrameEdit->setText(errorMessage);
        return;
    }

    const QByteArray body = command + payload;
    const QByteArray checksum = checksumBytes(body, customChecksumComboBox->currentData().toInt());
    const QByteArray frame = header + body + checksum + footer;
    customFrameEdit->setText(formatHex(frame));
}

/**
 * @brief 解析自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
/**
 * @brief 解析自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::parseCustomFrame()
{
    const QString input = customParseInputEdit->text().trimmed();
    if (input.isEmpty()) {
        customParseResultEdit->clear();
        return;
    }

    QString errorMessage;
    QByteArray frame = parseHexString(input, chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customParseResultEdit->setPlainText(errorMessage);
        return;
    }

    const QByteArray header = parseHexString(customHeaderEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customParseResultEdit->setPlainText(errorMessage);
        return;
    }
    const QByteArray footer = parseHexString(customFooterEdit->text(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty()) {
        customParseResultEdit->setPlainText(errorMessage);
        return;
    }

    if (!header.isEmpty()) {
        if (!frame.startsWith(header)) {
            customParseResultEdit->setPlainText(
                uiText(QStringLiteral("帧头不匹配。"), QStringLiteral("Header does not match.")));
            return;
        }
        frame.remove(0, header.size());
    }

    if (!footer.isEmpty()) {
        if (!frame.endsWith(footer)) {
            customParseResultEdit->setPlainText(
                uiText(QStringLiteral("帧尾不匹配。"), QStringLiteral("Footer does not match.")));
            return;
        }
        frame.chop(footer.size());
    }

    const int checksumMode = customChecksumComboBox->currentData().toInt();
    const int checksumLength = checksumMode == 3 ? 2 : (checksumMode == 0 ? 0 : 1);
    if (frame.size() < checksumLength + customCommandLengthSpinBox->value()) {
        customParseResultEdit->setPlainText(
            uiText(QStringLiteral("帧长度不足，无法拆分命令字和校验。"),
                   QStringLiteral("Frame is too short for command and checksum.")));
        return;
    }

    QByteArray checksum;
    if (checksumLength > 0) {
        checksum = frame.right(checksumLength);
        frame.chop(checksumLength);
    }

    const QByteArray expectedChecksum = checksumBytes(frame, checksumMode);
    const QByteArray command = frame.left(customCommandLengthSpinBox->value());
    const QByteArray payload = frame.mid(customCommandLengthSpinBox->value());

    QStringList lines;
    lines << uiText(QStringLiteral("命令字: %1"), QStringLiteral("Command: %1")).arg(formatHex(command));
    lines << uiText(QStringLiteral("载荷: %1"), QStringLiteral("Payload: %1")).arg(formatHex(payload));
    if (checksumLength > 0) {
        lines << uiText(QStringLiteral("校验: %1 (%2)"), QStringLiteral("Checksum: %1 (%2)"))
                     .arg(formatHex(checksum))
                     .arg(checksum == expectedChecksum
                              ? uiText(QStringLiteral("通过"), QStringLiteral("valid"))
                              : uiText(QStringLiteral("失败"), QStringLiteral("invalid")));
    } else {
        lines << uiText(QStringLiteral("校验: 未启用"), QStringLiteral("Checksum: disabled"));
    }

    customParseResultEdit->setPlainText(lines.join(QLatin1Char('\n')));
}

/**
 * @brief 立即独立发送当前自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
/**
 * @brief 立即独立发送当前自定义协议报文
 *
 * @param 无
 *
 * @return 无
 */
void ToolboxDialog::sendCustomFrameNow()
{
    if (!transportReady) {
        refreshProtocolSendState();
        return;
    }

    QString errorMessage;
    parseHexString(customFrameEdit->text().trimmed(), chineseMode, &errorMessage);
    if (!errorMessage.isEmpty() || customFrameEdit->text().trimmed().isEmpty()) {
        refreshProtocolSendState();
        return;
    }

    emit requestSendPayload(customFrameEdit->text().trimmed(), true, false);
}
