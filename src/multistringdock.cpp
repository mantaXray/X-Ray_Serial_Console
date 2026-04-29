/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: multistringdock.cpp
 * Version: 1.0.0
 * Description: 多字符串停靠面板实现
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#include "multistringdock.h"

#include "appsettings.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

constexpr int kDefaultIntervalMs = 500;
constexpr int kMinimumIntervalMs = 10;
constexpr int kDefaultDockMinimumWidth = 620;
constexpr int kMultiStringHeaderLayoutVersion = 2;

struct MultiStringRowData {
    int originalRow = -1;
    int order = 1;
    bool enabled = false;
    QString name;
    QString payload;
    int intervalMs = kDefaultIntervalMs;
    bool hexMode = false;
    bool appendNewLine = false;
};

QString cellCheckBoxStyleSheet()
{
    return QStringLiteral(R"(
        QCheckBox#multiStringCellCheck {
            min-width: 22px;
            min-height: 22px;
            padding: 0;
            margin: 0;
            background: transparent;
        }
        QCheckBox#multiStringCellCheck::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #5f7892;
            border-radius: 3px;
            background: #ffffff;
        }
        QCheckBox#multiStringCellCheck::indicator:hover {
            border-color: #2563eb;
            background: #f3f8ff;
        }
        QCheckBox#multiStringCellCheck::indicator:checked {
            border-color: #1e4fb9;
            background: #2563eb;
            image: url(:/icons/checkmark.svg);
        }
        QCheckBox#multiStringCellCheck::indicator:disabled {
            border-color: #cbd8e5;
            background: #f1f6fb;
        }
    )");
}

} // namespace

/**
 * @brief 构造多字符串发送面板
 *
 * @param parent: 父对象指针
 *
 * @return 无
 */
MultiStringDock::MultiStringDock(QWidget *parent)
    : QDockWidget(parent)
{
    buildUi();
    loadSettings();
    updateTexts();
    updateButtonStates();
    refreshSchedulerState();
}

/**
 * @brief 切换界面语言
 *
 * @param chineseModeEnabled: `true` 为中文，`false` 为英文
 *
 * @return 无
 */
void MultiStringDock::setChineseMode(bool chineseModeEnabled)
{
    chineseMode = chineseModeEnabled;
    updateTexts();
    emit schedulerStateChanged();
}

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
void MultiStringDock::captureEditorPayload(const QString &payloadText,
                                           bool hexMode,
                                           bool appendNewLine,
                                           bool replaceCurrentRow)
{
    int row = currentPresetRow();
    if (!replaceCurrentRow || row < 0) {
        row = presetTable->rowCount();
        presetTable->insertRow(row);
        setOrderItem(row, nextSuggestedOrder());
        setTextItem(row,
                    NameColumn,
                    uiText(QStringLiteral("字符串 %1"), QStringLiteral("Preset %1")).arg(row + 1));
        setIntervalItem(row, kDefaultIntervalMs);
        setCheckItem(row, EnabledColumn, false);
    }

    if (row < 0) {
        return;
    }

    setTextItem(row, PayloadColumn, payloadText);
    setCheckItem(row, HexColumn, hexMode);
    setCheckItem(row, NewLineColumn, appendNewLine);
    presetTable->selectRow(row);

    saveSettings();
    updateButtonStates();
    refreshSchedulerState();
}

/**
 * @brief 设置底层串口是否允许发送
 *
 * @param ready: `true` 表示串口已连接且允许发送
 *
 * @return 无
 */
void MultiStringDock::setTransportReady(bool ready)
{
    if (transportReady == ready) {
        return;
    }

    transportReady = ready;
    refreshSchedulerState();
}

/**
 * @brief 获取当前已勾选参与轮询的命令数量
 *
 * @param 无
 *
 * @return 勾选发送的命令条数
 */
int MultiStringDock::activePresetCount() const
{
    return enabledRows().size();
}

/**
 * @brief 判断轮询定时器是否正在运行
 *
 * @param 无
 *
 * @return `true` 表示正在按计划轮询发送
 */
bool MultiStringDock::isSchedulerRunning() const
{
    return dispatchTimer != nullptr && dispatchTimer->isActive();
}

/**
 * @brief 判断发送链路是否处于可用状态
 *
 * @param 无
 *
 * @return `true` 表示主窗口已告知串口可发送
 */
bool MultiStringDock::isTransportReady() const
{
    return transportReady;
}

/**
 * @brief 根据当前语言返回对应文本
 *
 * @param chinese: 中文文本
 * @param english: 英文文本
 *
 * @return 当前语言下应显示的文本
 */
QString MultiStringDock::uiText(const QString &chinese, const QString &english) const
{
    return chineseMode ? chinese : english;
}

/**
 * @brief 返回复选框列的提示文本
 *
 * @param column: 列号
 *
 * @return 当前语言下的提示文本
 */
QString MultiStringDock::checkColumnToolTip(int column) const
{
    switch (column) {
    case EnabledColumn:
        return uiText(QStringLiteral("勾选后参与多字符串轮询发送。"),
                      QStringLiteral("Checked rows participate in multi-string polling."));
    case HexColumn:
        return uiText(QStringLiteral("勾选后该行内容按 HEX 解析发送；未勾选时按普通文本发送。"),
                      QStringLiteral("Checked rows parse payload as HEX; unchecked rows send plain text."));
    case NewLineColumn:
        return uiText(QStringLiteral("勾选后发送时追加换行。"),
                      QStringLiteral("Append a line ending when sending."));
    default:
        return QString();
    }
}

/**
 * @brief 创建停靠面板界面
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::buildUi()
{
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(kDefaultDockMinimumWidth);

    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    hintLabel = new QLabel(container);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    presetTable = new QTableWidget(container);
    presetTable->setColumnCount(ColumnCount);
    presetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    presetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    presetTable->setAlternatingRowColors(true);
    presetTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed
                                 | QAbstractItemView::SelectedClicked);
    presetTable->verticalHeader()->setVisible(false);
    presetTable->horizontalHeader()->setStretchLastSection(true);
    presetTable->horizontalHeader()->setMinimumSectionSize(48);
    presetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // 默认列宽控制在面板首次展开宽度内，避免用户没注意到底部横向滚动条。
    // 表头状态仍会在用户拖拽后保存，下一次打开优先恢复用户自己的比例。
    presetTable->setColumnWidth(EnabledColumn, 54);
    presetTable->setColumnWidth(OrderColumn, 54);
    presetTable->setColumnWidth(NameColumn, 84);
    presetTable->setColumnWidth(PayloadColumn, 190);
    presetTable->setColumnWidth(IntervalColumn, 72);
    presetTable->setColumnWidth(HexColumn, 78);
    presetTable->setColumnWidth(NewLineColumn, 56);
    layout->addWidget(presetTable, 1);

    auto *row1 = new QHBoxLayout;
    row1->setSpacing(6);
    captureButton = new QPushButton(container);
    replaceButton = new QPushButton(container);
    addEmptyButton = new QPushButton(container);
    removeButton = new QPushButton(container);
    row1->addWidget(captureButton);
    row1->addWidget(replaceButton);
    row1->addWidget(addEmptyButton);
    row1->addWidget(removeButton);
    layout->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->setSpacing(6);
    sendNowButton = new QPushButton(container);
    enableAllButton = new QPushButton(container);
    disableAllButton = new QPushButton(container);
    defaultOrderButton = new QPushButton(container);
    row2->addWidget(sendNowButton);
    row2->addWidget(enableAllButton);
    row2->addWidget(disableAllButton);
    row2->addWidget(defaultOrderButton);
    row2->addStretch();
    layout->addLayout(row2);

    setWidget(container);

    dispatchTimer = new QTimer(this);
    dispatchTimer->setSingleShot(true);

    connect(captureButton, &QPushButton::clicked, this, [this] {
        emit requestCaptureFromEditor(false);
    });
    connect(replaceButton, &QPushButton::clicked, this, [this] {
        emit requestCaptureFromEditor(true);
    });
    connect(addEmptyButton, &QPushButton::clicked, this, [this] {
        const int row = presetTable->rowCount();
        presetTable->insertRow(row);
        setCheckItem(row, EnabledColumn, false);
        setOrderItem(row, nextSuggestedOrder());
        setTextItem(row,
                    NameColumn,
                    uiText(QStringLiteral("字符串 %1"), QStringLiteral("Preset %1")).arg(row + 1));
        setTextItem(row, PayloadColumn, QString());
        setIntervalItem(row, kDefaultIntervalMs);
        setCheckItem(row, HexColumn, false);
        setCheckItem(row, NewLineColumn, false);
        presetTable->selectRow(row);

        saveSettings();
        updateButtonStates();
        refreshSchedulerState();
    });
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const int row = currentPresetRow();
        if (row < 0) {
            return;
        }

        presetTable->removeRow(row);
        if (lastSentRow == row) {
            lastSentRow = -1;
        } else if (lastSentRow > row) {
            --lastSentRow;
        }

        saveSettings();
        updateButtonStates();
        refreshSchedulerState();
    });
    connect(sendNowButton, &QPushButton::clicked, this, [this] {
        const int row = currentPresetRow();
        if (row < 0) {
            return;
        }

        emit requestSendPayload(rowText(row, PayloadColumn),
                                rowCheckState(row, HexColumn),
                                rowCheckState(row, NewLineColumn));
    });
    connect(enableAllButton, &QPushButton::clicked, this, [this] { setAllEnabled(true); });
    connect(disableAllButton, &QPushButton::clicked, this, [this] { setAllEnabled(false); });
    connect(defaultOrderButton, &QPushButton::clicked, this, &MultiStringDock::applyDefaultOrder);
    connect(dispatchTimer, &QTimer::timeout, this, &MultiStringDock::sendNextCheckedRow);

    connect(presetTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &MultiStringDock::updateButtonStates);
    connect(presetTable, &QTableWidget::itemChanged, this, [this] {
        if (!loadingSettings) {
            saveSettings();
        }
        updateButtonStates();
        refreshSchedulerState();
    });
    connect(presetTable->horizontalHeader(), &QHeaderView::sectionResized, this, [this] {
        if (!loadingSettings) {
            saveSettings();
        }
    });
}

/**
 * @brief 刷新界面文案
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::updateTexts()
{
    setWindowTitle(uiText(QStringLiteral("多字符串"), QStringLiteral("Multi String")));
    hintLabel->setText(
        uiText(QStringLiteral("勾选“发送”后，该行会按“顺序”列独立轮询发送；“HEX发送”勾选时按 HEX 解析，未勾选时按普通文本发送。"),
               QStringLiteral("Checked rows send independently by the Order column. Enable \"HEX Send\" to parse payload as HEX; leave it unchecked for plain text.")));

    captureButton->setText(uiText(QStringLiteral("保存当前"), QStringLiteral("Capture Current")));
    replaceButton->setText(uiText(QStringLiteral("覆盖选中"), QStringLiteral("Replace Selected")));
    addEmptyButton->setText(uiText(QStringLiteral("新增空白"), QStringLiteral("Add Blank")));
    removeButton->setText(uiText(QStringLiteral("删除选中"), QStringLiteral("Delete")));
    sendNowButton->setText(uiText(QStringLiteral("立即单发"), QStringLiteral("Send Once")));
    enableAllButton->setText(uiText(QStringLiteral("全部启用"), QStringLiteral("Enable All")));
    disableAllButton->setText(uiText(QStringLiteral("全部停发"), QStringLiteral("Stop All")));
    defaultOrderButton->setText(uiText(QStringLiteral("默认排序"), QStringLiteral("Default Order")));

    presetTable->setHorizontalHeaderLabels({
        uiText(QStringLiteral("发送"), QStringLiteral("Send")),
        uiText(QStringLiteral("顺序"), QStringLiteral("Order")),
        uiText(QStringLiteral("名称"), QStringLiteral("Name")),
        uiText(QStringLiteral("内容"), QStringLiteral("Payload")),
        uiText(QStringLiteral("间隔(ms)"), QStringLiteral("Interval")),
        uiText(QStringLiteral("HEX发送"), QStringLiteral("HEX Send")),
        uiText(QStringLiteral("换行"), QStringLiteral("CRLF"))
    });

    presetTable->horizontalHeaderItem(EnabledColumn)->setToolTip(checkColumnToolTip(EnabledColumn));
    presetTable->horizontalHeaderItem(HexColumn)->setToolTip(checkColumnToolTip(HexColumn));
    presetTable->horizontalHeaderItem(NewLineColumn)->setToolTip(checkColumnToolTip(NewLineColumn));
    refreshCheckItemToolTips();
}

/**
 * @brief 按当前语言刷新所有复选框单元格提示
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::refreshCheckItemToolTips()
{
    const int checkColumns[] = {EnabledColumn, HexColumn, NewLineColumn};
    for (int row = 0; row < presetTable->rowCount(); ++row) {
        for (int column : checkColumns) {
            const QString toolTip = checkColumnToolTip(column);
            QTableWidgetItem *item = presetTable->item(row, column);
            if (item != nullptr) {
                item->setToolTip(toolTip);
            }
            if (QWidget *cellWidget = presetTable->cellWidget(row, column)) {
                cellWidget->setToolTip(toolTip);
            }
            if (QCheckBox *checkBox = cellCheckBox(row, column)) {
                checkBox->setToolTip(toolTip);
            }
        }
    }
}

/**
 * @brief 根据当前选择和轮询状态刷新按钮可用性
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::updateButtonStates()
{
    const bool hasSelection = currentPresetRow() >= 0;
    replaceButton->setEnabled(hasSelection);
    removeButton->setEnabled(hasSelection);
    sendNowButton->setEnabled(hasSelection);
    disableAllButton->setEnabled(activePresetCount() > 0);
    enableAllButton->setEnabled(presetTable->rowCount() > 0);
    defaultOrderButton->setEnabled(presetTable->rowCount() > 0);
}

/**
 * @brief 从配置中恢复多字符串列表
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::loadSettings()
{
    QSettings settings = createAppSettings();

    loadingSettings = true;
    presetTable->setRowCount(0);

    const int size = settings.beginReadArray(QStringLiteral("multi_string_presets"));
    for (int row = 0; row < size; ++row) {
        settings.setArrayIndex(row);
        presetTable->insertRow(row);
        setCheckItem(row, EnabledColumn, settings.value(QStringLiteral("enabled"), false).toBool());
        setOrderItem(row, settings.value(QStringLiteral("order"), row + 1).toInt());
        setTextItem(row,
                    NameColumn,
                    settings.value(QStringLiteral("name"),
                                   uiText(QStringLiteral("字符串 %1"), QStringLiteral("Preset %1")).arg(row + 1))
                        .toString());
        setTextItem(row, PayloadColumn, settings.value(QStringLiteral("payload")).toString());
        setIntervalItem(row, settings.value(QStringLiteral("interval_ms"), kDefaultIntervalMs).toInt());
        setCheckItem(row, HexColumn, settings.value(QStringLiteral("hex_mode"), false).toBool());
        setCheckItem(row, NewLineColumn, settings.value(QStringLiteral("append_new_line"), false).toBool());
    }
    settings.endArray();

    const QByteArray headerState = settings.value(QStringLiteral("layout/multi_string_table_header")).toByteArray();
    const int headerLayoutVersion =
        settings.value(QStringLiteral("layout/multi_string_table_header_version"), 0).toInt();
    // 旧版本可能已经把过宽的默认列宽写入配置；无版本号的表头状态不再恢复。
    if (!headerState.isEmpty() && headerLayoutVersion >= kMultiStringHeaderLayoutVersion) {
        presetTable->horizontalHeader()->restoreState(headerState);
    }

    loadingSettings = false;
}

/**
 * @brief 把当前多字符串列表保存到配置
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::saveSettings() const
{
    QSettings settings = createAppSettings();

    settings.beginWriteArray(QStringLiteral("multi_string_presets"));
    for (int row = 0; row < presetTable->rowCount(); ++row) {
        settings.setArrayIndex(row);
        settings.setValue(QStringLiteral("enabled"), rowCheckState(row, EnabledColumn));
        settings.setValue(QStringLiteral("order"), rowOrder(row));
        settings.setValue(QStringLiteral("name"), rowText(row, NameColumn));
        settings.setValue(QStringLiteral("payload"), rowText(row, PayloadColumn));
        settings.setValue(QStringLiteral("interval_ms"), rowInterval(row));
        settings.setValue(QStringLiteral("hex_mode"), rowCheckState(row, HexColumn));
        settings.setValue(QStringLiteral("append_new_line"), rowCheckState(row, NewLineColumn));
    }
    settings.endArray();
    settings.setValue(QStringLiteral("layout/multi_string_table_header"),
                      presetTable->horizontalHeader()->saveState());
    settings.setValue(QStringLiteral("layout/multi_string_table_header_version"), kMultiStringHeaderLayoutVersion);
    settings.sync();
}

/**
 * @brief 按当前顺序列重新整理发送顺序
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::applyDefaultOrder()
{
    QList<MultiStringRowData> rows;
    rows.reserve(presetTable->rowCount());

    const int selectedRow = currentPresetRow();
    const int previousLastSentRow = lastSentRow;

    for (int row = 0; row < presetTable->rowCount(); ++row) {
        MultiStringRowData data;
        data.originalRow = row;
        data.order = rowOrder(row);
        data.enabled = rowCheckState(row, EnabledColumn);
        data.name = rowText(row, NameColumn);
        data.payload = rowText(row, PayloadColumn);
        data.intervalMs = rowInterval(row);
        data.hexMode = rowCheckState(row, HexColumn);
        data.appendNewLine = rowCheckState(row, NewLineColumn);
        rows.push_back(data);
    }

    std::sort(rows.begin(), rows.end(), [](const MultiStringRowData &left, const MultiStringRowData &right) {
        if (left.order != right.order) {
            return left.order < right.order;
        }
        return left.originalRow < right.originalRow;
    });

    loadingSettings = true;
    presetTable->setRowCount(0);

    int remappedSelection = -1;
    int remappedLastSentRow = -1;
    for (int row = 0; row < rows.size(); ++row) {
        const MultiStringRowData &data = rows.at(row);
        presetTable->insertRow(row);
        setCheckItem(row, EnabledColumn, data.enabled);
        setOrderItem(row, row + 1);
        setTextItem(row, NameColumn, data.name);
        setTextItem(row, PayloadColumn, data.payload);
        setIntervalItem(row, data.intervalMs);
        setCheckItem(row, HexColumn, data.hexMode);
        setCheckItem(row, NewLineColumn, data.appendNewLine);

        if (data.originalRow == selectedRow) {
            remappedSelection = row;
        }
        if (data.originalRow == previousLastSentRow) {
            remappedLastSentRow = row;
        }
    }

    loadingSettings = false;
    lastSentRow = remappedLastSentRow;

    if (remappedSelection >= 0) {
        presetTable->selectRow(remappedSelection);
    }

    saveSettings();
    updateButtonStates();
    refreshSchedulerState();
}

/**
 * @brief 获取当前选中行
 *
 * @param 无
 *
 * @return 当前选中行索引，若无选中则返回 `-1`
 */
int MultiStringDock::currentPresetRow() const
{
    return presetTable->currentRow();
}

/**
 * @brief 获取当前所有勾选发送的行
 *
 * @param 无
 *
 * @return 所有启用发送的行号列表
 */
QList<int> MultiStringDock::enabledRows() const
{
    QList<int> rows;
    for (int row = 0; row < presetTable->rowCount(); ++row) {
        if (rowCheckState(row, EnabledColumn)) {
            rows << row;
        }
    }

    std::sort(rows.begin(), rows.end(), [this](int left, int right) {
        if (rowOrder(left) != rowOrder(right)) {
            return rowOrder(left) < rowOrder(right);
        }
        return left < right;
    });

    return rows;
}

/**
 * @brief 读取指定单元格的勾选状态
 *
 * @param row: 行号
 * @param column: 列号
 *
 * @return `true` 表示该单元格处于勾选状态
 */
bool MultiStringDock::rowCheckState(int row, int column) const
{
    if (row < 0 || row >= presetTable->rowCount()) {
        return false;
    }

    if (QCheckBox *checkBox = cellCheckBox(row, column)) {
        return cellCheckBoxState(row, column);
    }

    if (QTableWidgetItem *item = presetTable->item(row, column)) {
        return item->checkState() == Qt::Checked;
    }

    return false;
}

/**
 * @brief 获取指定复选框单元格中的控件
 *
 * @param row: 行号
 * @param column: 列号
 *
 * @return 单元格中的复选框；不存在时返回 `nullptr`
 */
QCheckBox *MultiStringDock::cellCheckBox(int row, int column) const
{
    QWidget *cellWidget = presetTable->cellWidget(row, column);
    return cellWidget != nullptr
        ? cellWidget->findChild<QCheckBox *>(QStringLiteral("multiStringCellCheck"), Qt::FindDirectChildrenOnly)
        : nullptr;
}

/**
 * @brief 读取指定复选框单元格控件状态
 *
 * @param row: 行号
 * @param column: 列号
 *
 * @return `true` 表示控件已勾选
 */
bool MultiStringDock::cellCheckBoxState(int row, int column) const
{
    QCheckBox *checkBox = cellCheckBox(row, column);
    return checkBox != nullptr && checkBox->isChecked();
}

/**
 * @brief 读取指定单元格的文本内容
 *
 * @param row: 行号
 * @param column: 列号
 *
 * @return 单元格文本
 */
QString MultiStringDock::rowText(int row, int column) const
{
    if (row < 0 || row >= presetTable->rowCount()) {
        return {};
    }

    if (QTableWidgetItem *item = presetTable->item(row, column)) {
        return item->text().trimmed();
    }

    return {};
}

/**
 * @brief 读取指定行的发送顺序值
 *
 * @param row: 行号
 *
 * @return 顺序值，非法内容回退为 1
 */
int MultiStringDock::rowOrder(int row) const
{
    bool ok = false;
    const int value = rowText(row, OrderColumn).toInt(&ok);
    return ok ? qMax(1, value) : 1;
}

/**
 * @brief 读取指定行的发送间隔
 *
 * @param row: 行号
 *
 * @return 发送间隔，单位毫秒
 */
int MultiStringDock::rowInterval(int row) const
{
    bool ok = false;
    const int value = rowText(row, IntervalColumn).toInt(&ok);
    return ok ? qMax(kMinimumIntervalMs, value) : kDefaultIntervalMs;
}

/**
 * @brief 计算下一条建议使用的发送顺序
 *
 * @param 无
 *
 * @return 当前最大顺序值加 1
 */
int MultiStringDock::nextSuggestedOrder() const
{
    int maxOrder = 0;
    for (int row = 0; row < presetTable->rowCount(); ++row) {
        maxOrder = qMax(maxOrder, rowOrder(row));
    }
    return maxOrder + 1;
}

/**
 * @brief 设置复选框单元格
 *
 * @param row: 行号
 * @param column: 列号
 * @param checked: 是否勾选
 *
 * @return 无
 */
void MultiStringDock::setCheckItem(int row, int column, bool checked)
{
    QTableWidgetItem *item = presetTable->item(row, column);
    if (item == nullptr) {
        item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        presetTable->setItem(row, column, item);
    }

    item->setText(QString());
    item->setToolTip(checkColumnToolTip(column));

    QCheckBox *checkBox = cellCheckBox(row, column);
    QWidget *cellWidget = presetTable->cellWidget(row, column);
    if (checkBox == nullptr || cellWidget == nullptr) {
        cellWidget = new QWidget(presetTable);
        cellWidget->setObjectName(QStringLiteral("multiStringCheckCell"));
        cellWidget->setToolTip(checkColumnToolTip(column));

        auto *layout = new QHBoxLayout(cellWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->setAlignment(Qt::AlignCenter);

        checkBox = new QCheckBox(cellWidget);
        checkBox->setObjectName(QStringLiteral("multiStringCellCheck"));
        checkBox->setText(QString());
        checkBox->setToolTip(checkColumnToolTip(column));
        checkBox->setCursor(Qt::PointingHandCursor);
        checkBox->setStyleSheet(cellCheckBoxStyleSheet());
        layout->addWidget(checkBox);
        presetTable->setCellWidget(row, column, cellWidget);

        connect(checkBox, &QCheckBox::toggled, this, [this] {
            if (!loadingSettings) {
                saveSettings();
            }
            updateButtonStates();
            refreshSchedulerState();
        });
    }

    cellWidget->setToolTip(checkColumnToolTip(column));
    checkBox->setToolTip(checkColumnToolTip(column));
    const QSignalBlocker blocker(checkBox);
    checkBox->setChecked(checked);
}

/**
 * @brief 设置文本单元格
 *
 * @param row: 行号
 * @param column: 列号
 * @param text: 文本内容
 *
 * @return 无
 */
void MultiStringDock::setTextItem(int row, int column, const QString &text)
{
    QTableWidgetItem *item = presetTable->item(row, column);
    if (item == nullptr) {
        item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        presetTable->setItem(row, column, item);
    }

    item->setText(text);
    if (column == OrderColumn || column == IntervalColumn) {
        item->setTextAlignment(Qt::AlignCenter);
    } else {
        item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    }
}

/**
 * @brief 设置顺序列单元格
 *
 * @param row: 行号
 * @param order: 顺序值
 *
 * @return 无
 */
void MultiStringDock::setOrderItem(int row, int order)
{
    setTextItem(row, OrderColumn, QString::number(qMax(1, order)));
}

/**
 * @brief 设置发送间隔单元格
 *
 * @param row: 行号
 * @param intervalMs: 间隔毫秒数
 *
 * @return 无
 */
void MultiStringDock::setIntervalItem(int row, int intervalMs)
{
    setTextItem(row, IntervalColumn, QString::number(qMax(kMinimumIntervalMs, intervalMs)));
}

/**
 * @brief 将所有行统一勾选或取消勾选
 *
 * @param enabled: 目标勾选状态
 *
 * @return 无
 */
void MultiStringDock::setAllEnabled(bool enabled)
{
    loadingSettings = true;
    for (int row = 0; row < presetTable->rowCount(); ++row) {
        setCheckItem(row, EnabledColumn, enabled);
    }
    loadingSettings = false;

    saveSettings();
    updateButtonStates();
    refreshSchedulerState();
}

/**
 * @brief 按当前表格状态刷新轮询定时器
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::refreshSchedulerState()
{
    const QList<int> rows = enabledRows();
    const bool shouldRun = transportReady && !rows.isEmpty();

    if (!shouldRun) {
        dispatchTimer->stop();
        if (rows.isEmpty()) {
            lastSentRow = -1;
        }
        emit schedulerStateChanged();
        return;
    }

    if (!dispatchTimer->isActive()) {
        dispatchTimer->start(kMinimumIntervalMs);
    }

    emit schedulerStateChanged();
}

/**
 * @brief 发送下一条勾选命令
 *
 * @param 无
 *
 * @return 无
 */
void MultiStringDock::sendNextCheckedRow()
{
    const QList<int> rows = enabledRows();
    if (!transportReady || rows.isEmpty()) {
        refreshSchedulerState();
        return;
    }

    const int row = nextRoundRobinRow(rows);
    if (row < 0) {
        refreshSchedulerState();
        return;
    }

    emit requestSendPayload(rowText(row, PayloadColumn),
                            rowCheckState(row, HexColumn),
                            rowCheckState(row, NewLineColumn));

    lastSentRow = row;
    dispatchTimer->start(rowInterval(row));
    emit schedulerStateChanged();
}

/**
 * @brief 根据上次发送位置决定下一条轮询行
 *
 * @param rows: 当前所有可发送行
 *
 * @return 本次应发送的行号
 */
int MultiStringDock::nextRoundRobinRow(const QList<int> &rows) const
{
    if (rows.isEmpty()) {
        return -1;
    }

    const int currentIndex = rows.indexOf(lastSentRow);
    if (currentIndex < 0) {
        return rows.first();
    }

    return rows.at((currentIndex + 1) % rows.size());
}
