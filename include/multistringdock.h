/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: multistringdock.h
 * Version: 1.00.00
 * Description: 多字符串停靠面板类声明
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#ifndef MULTISTRINGDOCK_H
#define MULTISTRINGDOCK_H

#include <QDockWidget>
#include <QList>
#include <QString>

class QLabel;
class QCheckBox;
class QPushButton;
class QTableWidget;
class QTimer;

/**
 * @brief 多字符串发送面板
 *
 * 该面板独立维护一组可轮询发送的字符串命令。
 * 每一行都可以单独设置是否参与发送、顺序、HEX 模式、换行和间隔。
 */
class MultiStringDock : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造多字符串发送面板
     *
     * @param parent: 父对象指针
     *
     * @return 无
     */
    explicit MultiStringDock(QWidget *parent = nullptr);

    /**
     * @brief 切换界面语言
     *
     * @param chineseModeEnabled: `true` 为中文，`false` 为英文
     *
     * @return 无
     */
    void setChineseMode(bool chineseModeEnabled);

    /**
     * @brief 把发送区内容保存为一条预设命令
     *
     * @param payloadText: 需要保存的发送内容
     * @param hexMode: 是否按 HEX 发送
     * @param appendNewLine: 是否追加 CRLF
     * @param replaceCurrentRow: 是否覆盖当前选中行
     *
     * @return 无
     */
    void captureEditorPayload(const QString &payloadText,
                              bool hexMode,
                              bool appendNewLine,
                              bool replaceCurrentRow);

    /**
     * @brief 设置底层串口是否允许发送
     *
     * @param ready: `true` 表示串口已连接且允许发送
     *
     * @return 无
     */
    void setTransportReady(bool ready);

    /**
     * @brief 获取当前已勾选参与轮询的命令数量
     *
     * @param 无
     *
     * @return 勾选发送的命令条数
     */
    int activePresetCount() const;

    /**
     * @brief 判断轮询定时器是否正在运行
     *
     * @param 无
     *
     * @return `true` 表示正在按计划轮询发送
     */
    bool isSchedulerRunning() const;

    /**
     * @brief 判断发送链路是否处于可用状态
     *
     * @param 无
     *
     * @return `true` 表示主窗口已告知串口可发送
     */
    bool isTransportReady() const;

signals:
    /**
     * @brief 请求从主发送区抓取当前内容
     *
     * @param replaceCurrentRow: `true` 表示覆盖当前选中行
     *
     * @return 无
     */
    void requestCaptureFromEditor(bool replaceCurrentRow);

    /**
     * @brief 请求立即发送一条独立字符串命令
     *
     * @param payloadText: 发送内容
     * @param hexMode: 是否按 HEX 发送
     * @param appendNewLine: 是否追加 CRLF
     *
     * @return 无
     */
    void requestSendPayload(const QString &payloadText, bool hexMode, bool appendNewLine);

    /**
     * @brief 轮询发送状态变化时通知主窗口刷新状态
     *
     * @param 无
     *
     * @return 无
     */
    void schedulerStateChanged();

private:
    enum Column {
        EnabledColumn = 0,
        OrderColumn,
        NameColumn,
        PayloadColumn,
        IntervalColumn,
        HexColumn,
        NewLineColumn,
        ColumnCount
    };

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
     * @brief 返回复选框列的提示文本
     *
     * @param column: 列号
     *
     * @return 当前语言下的提示文本
     */
    QString checkColumnToolTip(int column) const;

    /**
     * @brief 创建停靠面板界面
     *
     * @param 无
     *
     * @return 无
     */
    void buildUi();

    /**
     * @brief 刷新界面文案
     *
     * @param 无
     *
     * @return 无
     */
    void updateTexts();

    /**
     * @brief 按当前语言刷新所有复选框单元格提示
     *
     * @param 无
     *
     * @return 无
     */
    void refreshCheckItemToolTips();

    /**
     * @brief 根据当前选择和轮询状态刷新按钮可用性
     *
     * @param 无
     *
     * @return 无
     */
    void updateButtonStates();

    /**
     * @brief 从配置中恢复多字符串列表
     *
     * @param 无
     *
     * @return 无
     */
    void loadSettings();

    /**
     * @brief 把当前多字符串列表保存到配置
     *
     * @param 无
     *
     * @return 无
     */
    void saveSettings() const;

    /**
     * @brief 按当前顺序列重新整理发送顺序
     *
     * 该函数会先按“顺序”列从小到大排序，再自动重排为 1、2、3...
     *
     * @param 无
     *
     * @return 无
     */
    void applyDefaultOrder();

    /**
     * @brief 获取当前选中行
     *
     * @param 无
     *
     * @return 当前选中行索引，若无选中则返回 `-1`
     */
    int currentPresetRow() const;

    /**
     * @brief 获取当前所有勾选发送的行
     *
     * @param 无
     *
     * @return 所有启用发送的行号列表
     */
    QList<int> enabledRows() const;

    /**
     * @brief 读取指定单元格的勾选状态
     *
     * @param row: 行号
     * @param column: 列号
     *
     * @return `true` 表示该单元格处于勾选状态
     */
    bool rowCheckState(int row, int column) const;

    /**
     * @brief 获取指定复选框单元格中的控件
     *
     * @param row: 行号
     * @param column: 列号
     *
     * @return 单元格中的复选框；不存在时返回 `nullptr`
     */
    QCheckBox *cellCheckBox(int row, int column) const;

    /**
     * @brief 读取指定复选框单元格控件状态
     *
     * @param row: 行号
     * @param column: 列号
     *
     * @return `true` 表示控件已勾选
     */
    bool cellCheckBoxState(int row, int column) const;

    /**
     * @brief 读取指定单元格的文本内容
     *
     * @param row: 行号
     * @param column: 列号
     *
     * @return 单元格文本
     */
    QString rowText(int row, int column) const;

    /**
     * @brief 读取指定行的发送顺序值
     *
     * @param row: 行号
     *
     * @return 顺序值，非法内容回退为 1
     */
    int rowOrder(int row) const;

    /**
     * @brief 读取指定行的发送间隔
     *
     * @param row: 行号
     *
     * @return 发送间隔，单位毫秒
     */
    int rowInterval(int row) const;

    /**
     * @brief 计算下一条建议使用的发送顺序
     *
     * @param 无
     *
     * @return 当前最大顺序值加 1
     */
    int nextSuggestedOrder() const;

    /**
     * @brief 设置复选框单元格
     *
     * @param row: 行号
     * @param column: 列号
     * @param checked: 是否勾选
     *
     * @return 无
     */
    void setCheckItem(int row, int column, bool checked);

    /**
     * @brief 设置文本单元格
     *
     * @param row: 行号
     * @param column: 列号
     * @param text: 文本内容
     *
     * @return 无
     */
    void setTextItem(int row, int column, const QString &text);

    /**
     * @brief 设置顺序列单元格
     *
     * @param row: 行号
     * @param order: 顺序值
     *
     * @return 无
     */
    void setOrderItem(int row, int order);

    /**
     * @brief 设置发送间隔单元格
     *
     * @param row: 行号
     * @param intervalMs: 间隔毫秒数
     *
     * @return 无
     */
    void setIntervalItem(int row, int intervalMs);

    /**
     * @brief 将所有行统一勾选或取消勾选
     *
     * @param enabled: 目标勾选状态
     *
     * @return 无
     */
    void setAllEnabled(bool enabled);

    /**
     * @brief 按当前表格状态刷新轮询定时器
     *
     * @param 无
     *
     * @return 无
     */
    void refreshSchedulerState();

    /**
     * @brief 发送下一条勾选命令
     *
     * @param 无
     *
     * @return 无
     */
    void sendNextCheckedRow();

    /**
     * @brief 根据上次发送位置决定下一条轮询行
     *
     * @param rows: 当前所有可发送行
     *
     * @return 本次应发送的行号
     */
    int nextRoundRobinRow(const QList<int> &rows) const;

    QLabel *hintLabel = nullptr;
    QTableWidget *presetTable = nullptr;
    QPushButton *captureButton = nullptr;
    QPushButton *replaceButton = nullptr;
    QPushButton *sendNowButton = nullptr;
    QPushButton *addEmptyButton = nullptr;
    QPushButton *removeButton = nullptr;
    QPushButton *enableAllButton = nullptr;
    QPushButton *disableAllButton = nullptr;
    QPushButton *defaultOrderButton = nullptr;
    QTimer *dispatchTimer = nullptr;

    bool chineseMode = true;
    bool loadingSettings = false;
    bool transportReady = false;
    int lastSentRow = -1;
};

#endif // MULTISTRINGDOCK_H
