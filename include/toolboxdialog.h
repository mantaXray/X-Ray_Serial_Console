/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: toolboxdialog.h
 * Version: 1.0.0
 * Description: 协议与工具箱对话框类声明
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#ifndef TOOLBOXDIALOG_H
#define TOOLBOXDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTimer;

/**
 * @brief 协议工具箱对话框
 *
 * 该对话框集中放置计算器、文本 HEX 转换、Modbus RTU 组包解析
 * 以及自定义协议组包解析等辅助工具。
 */
class ToolboxDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造协议工具箱
     *
     * @param parent: 父对象指针
     *
     * @return 无
     */
    explicit ToolboxDialog(QWidget *parent = nullptr);

    /**
     * @brief 切换界面语言
     *
     * @param chineseModeEnabled: `true` 为中文，`false` 为英文
     *
     * @return 无
     */
    void setChineseMode(bool chineseModeEnabled);

    /**
     * @brief 打开指定页签
     *
     * @param pageIndex: 需要显示的页签索引
     *
     * @return 无
     */
    void openPage(int pageIndex);

    /**
     * @brief 设置串口发送链路是否可用
     *
     * @param ready: `true` 表示当前允许独立发送报文
     *
     * @return 无
     */
    void setTransportReady(bool ready);

signals:
    /**
     * @brief 请求把工具页生成的报文装入主发送区
     *
     * @param payloadText: 生成后的报文文本
     * @param hexMode: 是否按 HEX 发送
     * @param appendNewLine: 是否追加 CRLF
     *
     * @return 无
     */
    void requestLoadPayload(const QString &payloadText, bool hexMode, bool appendNewLine);

    /**
     * @brief 请求独立发送工具页当前报文
     *
     * @param payloadText: 需要发送的报文文本
     * @param hexMode: 是否按 HEX 发送
     * @param appendNewLine: 是否追加 CRLF
     *
     * @return 无
     */
    void requestSendPayload(const QString &payloadText, bool hexMode, bool appendNewLine);

private:
    /**
     * @brief 根据当前语言返回对应文本
     *
     * @param chinese: 中文文本
     * @param english: 英文文本
     *
     * @return 当前语言下应显示的文本
     */
    QString uiText(const QString &chinese, const QString &english) const;

    /**
     * @brief 创建工具箱界面
     *
     * @param 无
     *
     * @return 无
     */
    void buildUi();

    /**
     * @brief 刷新所有界面文案
     *
     * @param 无
     *
     * @return 无
     */
    void updateTexts();

    /**
     * @brief 重建 Modbus 功能码下拉框文本
     *
     * @param 无
     *
     * @return 无
     */
    void populateModbusFunctionOptions();

    /**
     * @brief 重建自定义校验算法下拉框文本
     *
     * @param 无
     *
     * @return 无
     */
    void populateCustomChecksumOptions();

    /**
     * @brief 刷新 Modbus 页的功能码说明
     *
     * @param 无
     *
     * @return 无
     */
    void updateModbusHints();

    /**
     * @brief 刷新协议页独立发送状态
     *
     * @param 无
     *
     * @return 无
     */
    void refreshProtocolSendState();

    /**
     * @brief 刷新计算器输出
     *
     * @param 无
     *
     * @return 无
     */
    void evaluateCalculator();

    /**
     * @brief 实时把文本转成 HEX
     *
     * @param 无
     *
     * @return 无
     */
    void convertTextToHex();

    /**
     * @brief 实时把 HEX 转成文本
     *
     * @param 无
     *
     * @return 无
     */
    void convertHexToText();

    /**
     * @brief 生成 Modbus RTU 报文
     *
     * @param 无
     *
     * @return 无
     */
    void buildModbusFrame();

    /**
     * @brief 解析 Modbus RTU 报文
     *
     * @param 无
     *
     * @return 无
     */
    void parseModbusFrame();

    /**
     * @brief 立即独立发送当前 Modbus 报文
     *
     * @param 无
     *
     * @return 无
     */
    void sendModbusFrameNow();

    /**
     * @brief 生成自定义协议报文
     *
     * @param 无
     *
     * @return 无
     */
    void buildCustomFrame();

    /**
     * @brief 解析自定义协议报文
     *
     * @param 无
     *
     * @return 无
     */
    void parseCustomFrame();

    /**
     * @brief 立即独立发送当前自定义协议报文
     *
     * @param 无
     *
     * @return 无
     */
    void sendCustomFrameNow();

    QTabWidget *tabWidget = nullptr;

    // 计算器页
    QLabel *calculatorHintLabel = nullptr;
    QLabel *calculatorExpressionLabel = nullptr;
    QLabel *calculatorDecimalLabel = nullptr;
    QLabel *calculatorHexLabel = nullptr;
    QLabel *calculatorBinaryLabel = nullptr;
    QLineEdit *calculatorExpressionEdit = nullptr;
    QLineEdit *calculatorDecimalEdit = nullptr;
    QLineEdit *calculatorHexEdit = nullptr;
    QLineEdit *calculatorBinaryEdit = nullptr;
    QPushButton *calculatorEvaluateButton = nullptr;
    QPushButton *calculatorClearButton = nullptr;

    // 文本 / HEX 页
    QLabel *textSourceLabel = nullptr;
    QLabel *hexSourceLabel = nullptr;
    QPlainTextEdit *textSourceEdit = nullptr;
    QPlainTextEdit *hexSourceEdit = nullptr;
    QPushButton *textToHexButton = nullptr;
    QPushButton *hexToTextButton = nullptr;
    QPushButton *hexClearButton = nullptr;
    QPushButton *hexLoadButton = nullptr;

    // Modbus 页
    QLabel *modbusBuildLabel = nullptr;
    QLabel *modbusParseLabel = nullptr;
    QLabel *modbusFunctionHintLabel = nullptr;
    QLabel *modbusDataHintLabel = nullptr;
    QLabel *modbusSlaveLabel = nullptr;
    QLabel *modbusFunctionLabel = nullptr;
    QLabel *modbusAddressLabel = nullptr;
    QLabel *modbusQuantityLabel = nullptr;
    QLabel *modbusDataLabel = nullptr;
    QLabel *modbusFrameLabel = nullptr;
    QLabel *modbusParseInputLabel = nullptr;
    QLabel *modbusIntervalLabel = nullptr;
    QLabel *modbusSendStatusLabel = nullptr;
    QSpinBox *modbusSlaveSpinBox = nullptr;
    QComboBox *modbusFunctionComboBox = nullptr;
    QSpinBox *modbusAddressSpinBox = nullptr;
    QSpinBox *modbusQuantitySpinBox = nullptr;
    QSpinBox *modbusIntervalSpinBox = nullptr;
    QLineEdit *modbusDataEdit = nullptr;
    QLineEdit *modbusFrameEdit = nullptr;
    QLineEdit *modbusParseInputEdit = nullptr;
    QPlainTextEdit *modbusParseResultEdit = nullptr;
    QPushButton *modbusBuildButton = nullptr;
    QPushButton *modbusLoadButton = nullptr;
    QPushButton *modbusParseButton = nullptr;
    QPushButton *modbusSendButton = nullptr;
    QCheckBox *modbusLoopSendCheckBox = nullptr;
    QTimer *modbusSendTimer = nullptr;

    // 自定义协议页
    QLabel *customBuildLabel = nullptr;
    QLabel *customParseLabel = nullptr;
    QLabel *customBuildHintLabel = nullptr;
    QLabel *customParseHintLabel = nullptr;
    QLabel *customHeaderLabel = nullptr;
    QLabel *customCommandLabel = nullptr;
    QLabel *customPayloadLabel = nullptr;
    QLabel *customChecksumLabel = nullptr;
    QLabel *customFooterLabel = nullptr;
    QLabel *customFrameLabel = nullptr;
    QLabel *customParseInputLabel = nullptr;
    QLabel *customCommandLengthLabel = nullptr;
    QLabel *customIntervalLabel = nullptr;
    QLabel *customSendStatusLabel = nullptr;
    QLineEdit *customHeaderEdit = nullptr;
    QLineEdit *customCommandEdit = nullptr;
    QLineEdit *customPayloadEdit = nullptr;
    QComboBox *customChecksumComboBox = nullptr;
    QLineEdit *customFooterEdit = nullptr;
    QLineEdit *customFrameEdit = nullptr;
    QLineEdit *customParseInputEdit = nullptr;
    QSpinBox *customCommandLengthSpinBox = nullptr;
    QSpinBox *customIntervalSpinBox = nullptr;
    QPlainTextEdit *customParseResultEdit = nullptr;
    QPushButton *customBuildButton = nullptr;
    QPushButton *customLoadButton = nullptr;
    QPushButton *customParseButton = nullptr;
    QPushButton *customSendButton = nullptr;
    QCheckBox *customLoopSendCheckBox = nullptr;
    QTimer *customSendTimer = nullptr;

    bool chineseMode = true;
    bool transportReady = false;
};

#endif // TOOLBOXDIALOG_H
