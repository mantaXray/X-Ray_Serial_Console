/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: mainwindow.h
 * Version: 1.0.0
 * Description: X-Ray Serial Console main window declaration
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPoint>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QAction;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDockWidget;
class QFile;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QSettings;
class QSplitter;
class QSpinBox;
class QTableWidget;
class QThread;
class QTimer;
class QFileInfo;

class SerialPortWorker;
class MultiStringDock;
class ToolboxDialog;

// 主窗口负责三类工作：
// 1. 构建串口调试界面与菜单。
// 2. 在主线程和串口工作线程之间转发命令。
// 3. 统一管理中英文界面文本和状态栏展示。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    // 主线程不直接操作底层串口句柄，而是通过这些信号把请求投递到工作线程。
    void requestOpenPort(const QString &portName,
                         int baudRate,
                         int dataBits,
                         const QString &parityText,
                         const QString &stopBitsText);
    void requestClosePort();
    void requestWriteData(const QByteArray &data);
    void requestSetRtsEnabled(bool enabled);
    void requestSetDtrEnabled(bool enabled);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class UiLanguage {
        Chinese,
        English
    };

    void buildUi();
    void buildMenus();
    void buildStatusBar();
    void connectUi();

    void applyLanguage(UiLanguage language);
    void updateLanguageActions();
    void populateParityOptions();
    void populateBaudRateOptions();
    void refreshPortList(const QString &preferredPort = QString());
    void updateUiState();
    void updateWrapMode();
    void updatePortSummary();
    void updateCounters();
    void updateAutoSendTimer();
    void scrollReceiveToBottom();
    void appendTrafficLog(const QByteArray &data,
                          bool outgoing,
                          bool hexMode,
                          bool flushPendingReceiveBeforeOutgoing = true);
    void flushPendingReceivePacket();
    void appendReceiveChunk(const QString &chunk);
    void refreshReceiveView();
    void applyReceiveHighlights();
    void updateConsoleFont();
    void updateDockTexts();
    void updatePresetSchedulerStatus();
    void buildModbusMonitorDock();
    void updateModbusMonitorTexts();
    void updateModbusMonitorState();
    void updateModbusMonitorFunctionState();
    void updateModbusMonitorRows();
    void refreshModbusMonitorTableValues();
    void sendModbusMonitorRequest();
    void handleModbusMonitorBytes(const QByteArray &data);
    void appendModbusMonitorLog(const QByteArray &data, bool outgoing);
    /**
     * @brief 刷新 Modbus 原始日志中等待超时聚合的接收数据
     *
     * @param 无
     *
     * @return 无
     */
    void flushPendingModbusMonitorLogPacket();

    /**
     * @brief 按当前同步显示设置追加一条 Modbus 原始收发日志
     *
     * @param data: 需要显示的原始字节流
     * @param outgoing: `true` 表示发送方向，`false` 表示接收方向
     *
     * @return 无
     */
    void appendModbusMonitorLogLine(const QByteArray &data, bool outgoing);

    /**
     * @brief 取消当前 Modbus 请求响应接收会话
     *
     * @param discardPendingLog: `true` 表示丢弃尚未刷新的 Modbus 原始日志缓存
     *
     * @return 无
     */
    void cancelModbusMonitorReceiveSession(bool discardPendingLog);

    /**
     * @brief 刷新 Modbus 原始日志选项控件状态
     *
     * @param 无
     *
     * @return 无
     */
    void updateModbusMonitorLogOptionState();

    QByteArray buildModbusMonitorRequest(const QVector<quint16> &writeValues) const;
    QVector<quint16> parseModbusMonitorWriteValues(QString *errorMessage) const;
    QString formatModbusMonitorValue(int row) const;

    /**
     * @brief 返回指定 Modbus 表格行实际使用的数据解析格式
     *
     * @param row: 表格行号
     *
     * @return 数据格式编号
     */
    int modbusMonitorFormatForRow(int row) const;

    /**
     * @brief 返回 Modbus 解析格式的界面显示名称
     *
     * @param format: 数据格式编号
     *
     * @return 当前语言下的格式名称
     */
    QString modbusMonitorFormatName(int format) const;

    /**
     * @brief 处理 Modbus 寄存器表格右键格式菜单
     *
     * @param position: 表格视口内的右键位置
     *
     * @return 无
     */
    void showModbusMonitorFormatMenu(const QPoint &position);

    /**
     * @brief 收集需要批量修改解析格式的 Modbus 表格行
     *
     * @param clickedRow: 右键命中的行号
     * @param clickedColumn: 右键命中的列号
     *
     * @return 去重后的表格行号
     */
    QList<int> selectedModbusMonitorRows(int clickedRow, int clickedColumn) const;

    /**
     * @brief 为选中的 Modbus 地址写入或清除解析格式覆盖
     *
     * @param rows: 目标表格行
     * @param format: 数据格式编号
     * @param useDefaultFormat: `true` 表示清除覆盖并跟随默认格式
     *
     * @return 无
     */
    void applyModbusMonitorFormatOverride(const QList<int> &rows, int format, bool useDefaultFormat);

    /**
     * @brief 保存 Modbus 单地址解析格式覆盖
     *
     * @param settings: 目标配置对象
     *
     * @return 无
     */
    void saveModbusMonitorFormatOverrides(QSettings &settings) const;

    /**
     * @brief 读取 Modbus 单地址解析格式覆盖
     *
     * @param settings: 来源配置对象
     *
     * @return 无
     */
    void loadModbusMonitorFormatOverrides(QSettings &settings);

    bool parseModbusMonitorResponse(const QByteArray &frame);
    void resetModbusMonitorState();

    /**
     * @brief 保存窗口几何、分隔条和表格列宽布局
     *
     * @param settings: 目标配置对象
     *
     * @return 无
     */
    void saveWindowLayoutSettings(QSettings &settings) const;

    /**
     * @brief 从配置恢复窗口几何、分隔条和表格列宽布局
     *
     * @param settings: 来源配置对象
     *
     * @return 无
     */
    void loadWindowLayoutSettings(QSettings &settings);

    /**
     * @brief 按多字符串面板可见状态恢复接收区分隔条比例
     *
     * @param visible: 多字符串面板是否可见
     *
     * @return 无
     */
    void applyPresetDockVisibilityLayout(bool visible);

    /**
     * @brief 单独保存发送编辑框草稿
     *
     * @param 无
     *
     * @return 无
     */
    void saveSendEditorText() const;

    void saveDisplaySettings() const;
    void loadDisplaySettings();
    void openToolboxPage(int pageIndex);
    void sendPresetPayload(const QString &payloadText, bool hexMode, bool appendNewLine);
    void showImportedFileNotice(const QString &filePath,
                                qint64 fileSize,
                                bool previewOnly,
                                bool binaryLike,
                                bool loadedAsHex,
                                qint64 previewBytes);
    void updateImportedFileNoticeTexts();
    void clearImportedFileNotice();
    bool queueFileForSending(const QString &fileName);
    void sendFileData();
    void sendToolboxPayload(const QString &payloadText, bool hexMode, bool appendNewLine);
    void startLiveCapture();
    void stopLiveCapture();
    void writeLiveCaptureChunk(const QString &chunk);
    void promptCustomBaudRate();
    void setCurrentBaudRateText(const QString &baudText);
    void chooseConsoleFont();
    bool promptFileSendOptions(const QFileInfo &fileInfo, int *chunkBytes, int *delayMs);
    bool sendFileInChunks(const QString &fileName, int chunkBytes, int delayMs);

    void saveReceiveContent();
    void loadSendFile();
    void showInfoDialog(const QString &title, const QString &text);
    void sendCurrentPayload(bool autoTriggered);
    QString windowTitleText() const;
    QString quickGuideText() const;
    QString aboutDialogText() const;

    int currentBaudRate() const;
    int currentDataBits() const;
    QString currentParityCode() const;
    int currentSendChecksumMode() const;
    QByteArray currentSendPayload(QString *errorMessage = nullptr) const;
    QByteArray buildPayload(const QString &content,
                            bool hexMode,
                            bool appendNewLine,
                            QString *errorMessage) const;
    QString trafficPrefix(bool outgoing) const;
    QString trafficPayloadText(const QByteArray &data, bool hexMode) const;
    QString uiText(const QString &chinese, const QString &english) const;

    static QStringList availablePorts();
    int comPortIndex(const QString &portName) const;
    QByteArray parseHexPayload(const QString &input, QString *errorMessage) const;

    Ui::MainWindow *ui;
    SerialPortWorker *serialWorker;
    QThread *serialThread;
    QTimer *autoSendTimer;
    QTimer *receivePacketTimer = nullptr;
    QTimer *modbusMonitorTimer = nullptr;
    QTimer *modbusMonitorPacketTimer = nullptr;
    QTimer *modbusMonitorResponseTimer = nullptr;

    // 主控件区。
    QPlainTextEdit *receiveEdit;
    QPlainTextEdit *sendEdit;
    QComboBox *portComboBox;
    QComboBox *baudRateComboBox;
    QComboBox *dataBitsComboBox;
    QComboBox *parityComboBox;
    QComboBox *stopBitsComboBox;
    QComboBox *checksumComboBox = nullptr;
    QCheckBox *hexDisplayCheckBox;
    QCheckBox *timestampCheckBox;
    QCheckBox *wrapCheckBox;
    QCheckBox *packetSplitCheckBox = nullptr;
    QCheckBox *pauseDisplayCheckBox;
    QCheckBox *hexSendCheckBox;
    QCheckBox *appendNewLineCheckBox;
    QCheckBox *autoSendCheckBox;
    QCheckBox *rtsCheckBox;
    QCheckBox *dtrCheckBox;
    QSpinBox *receiveTimeoutSpinBox = nullptr;
    QSpinBox *autoSendIntervalSpinBox;
    QPushButton *refreshPortsButton;
    QPushButton *openPortButton;
    QPushButton *clearReceiveButton;
    QPushButton *saveReceiveButton;
    QPushButton *loadSendFileButton;
    QPushButton *clearSendButton;
    QPushButton *sendButton;
    QFrame *importNoticeFrame = nullptr;
    QLabel *importNoticeTitleLabel = nullptr;
    QLabel *importNoticeDetailLabel = nullptr;
    QLabel *importNoticeHintLabel = nullptr;
    QPushButton *importNoticeActionButton = nullptr;
    QPushButton *importNoticeCloseButton = nullptr;

    // 需要跟随语言变化的标签。
    QLabel *receiveSectionLabel;
    QLabel *sendSectionLabel;
    QLabel *portLabel;
    QLabel *baudRateLabel;
    QLabel *dataBitsLabel;
    QLabel *parityLabel;
    QLabel *stopBitsLabel;
    QLabel *intervalLabel;
    QLabel *receiveTimeoutLabel = nullptr;
    QLabel *receiveTimeoutUnitLabel = nullptr;
    QLabel *autoSendIntervalUnitLabel = nullptr;
    QLabel *checksumLabel = nullptr;

    // 状态栏控件。
    QLabel *brandStatusLabel;
    QLabel *sendCountLabel;
    QLabel *receiveCountLabel;
    QLabel *portStateLabel;
    QLabel *lineStateLabel;
    QLabel *presetSchedulerStatusLabel = nullptr;
    QPushButton *presetDockToggleButton = nullptr;
    QSplitter *topDisplaySplitter = nullptr;
    QSplitter *modbusMonitorSplitter = nullptr;

    QDockWidget *modbusMonitorDock = nullptr;
    QTableWidget *modbusRegisterTable = nullptr;
    QSpinBox *modbusMonitorSlaveSpinBox = nullptr;
    QComboBox *modbusMonitorFunctionComboBox = nullptr;
    QComboBox *modbusMonitorFormatComboBox = nullptr;
    QSpinBox *modbusMonitorStartAddressSpinBox = nullptr;
    QSpinBox *modbusMonitorQuantitySpinBox = nullptr;
    QSpinBox *modbusMonitorIntervalSpinBox = nullptr;
    QLabel *modbusMonitorIntervalUnitLabel = nullptr;
    QLineEdit *modbusMonitorWriteValuesEdit = nullptr;
    QPushButton *modbusMonitorStartButton = nullptr;
    QPushButton *modbusMonitorPollButton = nullptr;
    QPushButton *modbusMonitorClearButton = nullptr;
    QLabel *modbusMonitorStatusLabel = nullptr;
    QCheckBox *modbusMonitorTimestampCheckBox = nullptr;
    QCheckBox *modbusMonitorPacketSplitCheckBox = nullptr;
    QLabel *modbusMonitorTimeoutLabel = nullptr;
    QSpinBox *modbusMonitorTimeoutSpinBox = nullptr;
    QLabel *modbusMonitorTimeoutUnitLabel = nullptr;
    QPlainTextEdit *modbusMonitorLogEdit = nullptr;

    // 菜单与动作，切换语言时需要整体重设文本。
    QMenu *connectionMenu;
    QMenu *configMenu;
    QMenu *viewMenu;
    QMenu *sendMenu;
    QMenu *presetMenu;
    QMenu *toolsMenu;
    QMenu *protocolToolboxMenu = nullptr;
    QMenu *languageMenu;
    QMenu *helpMenu;

    QAction *refreshAction;
    QAction *togglePortAction;
    QAction *quitAction;
    QAction *resetCounterAction;
    QAction *copySummaryAction;
    QAction *clearReceiveAction;
    QAction *saveReceiveAction;
    QAction *hexDisplayAction;
    QAction *timestampAction;
    QAction *wrapAction;
    QAction *sendNowAction;
    QAction *loadFileAction;
    QAction *sendFileAction = nullptr;
    QAction *hexSendAction;
    QAction *autoSendAction;
    QAction *heartbeatAction;
    QAction *modbusAction;
    QAction *clearSendAction;
    QAction *pauseAction;
    QAction *languageChineseAction;
    QAction *languageEnglishAction;
    QAction *guideAction;
    QAction *aboutAction;
    QAction *showPresetDockAction = nullptr;
    QAction *increaseFontAction = nullptr;
    QAction *decreaseFontAction = nullptr;
    QAction *resetFontAction = nullptr;
    QAction *fontSizeAction = nullptr;
    QAction *fontFamilyAction = nullptr;
    QAction *filterAction = nullptr;
    QAction *clearFilterAction = nullptr;
    QAction *highlightAction = nullptr;
    QAction *clearHighlightAction = nullptr;
    QAction *calculatorAction = nullptr;
    QAction *hexConverterAction = nullptr;
    QAction *modbusToolAction = nullptr;
    QAction *modbusMonitorAction = nullptr;
    QAction *customProtocolToolAction = nullptr;
    QAction *startCaptureAction = nullptr;
    QAction *stopCaptureAction = nullptr;

    MultiStringDock *presetDock = nullptr;
    ToolboxDialog *toolboxDialog = nullptr;
    QFile *liveCaptureFile = nullptr;
    QString liveCapturePath;
    QString importedFilePath;
    qint64 importedFileSize = 0;
    qint64 importedPreviewBytes = 0;
    bool importedPreviewOnly = false;
    bool importedBinaryLike = false;
    bool importedLoadedAsHex = false;

    QString receiveLogBuffer;
    QByteArray pendingReceivePacket;
    QByteArray pendingModbusMonitorLogPacket;
    QByteArray modbusMonitorBuffer;
    QVector<quint16> modbusMonitorRegisterValues;
    QVector<bool> modbusMonitorRegisterValid;
    QVector<quint16> modbusMonitorLastWriteValues;
    QHash<int, int> modbusMonitorAddressFormats;
    QString receiveFilterKeyword;
    QString receiveHighlightKeyword;
    QString consoleFontFamily;
    int consoleFontPointSize = 13;
    int fileSendChunkBytes = 1024;
    int fileSendDelayMs = 2;
    int modbusMonitorRequestSlave = 1;
    int modbusMonitorRequestFunction = 3;
    int modbusMonitorRequestStartAddress = 0;
    int modbusMonitorRequestQuantity = 0;
    int modbusMonitorResponseCount = 0;
    int modbusMonitorErrorCount = 0;
    bool modbusMonitorAwaitingResponse = false;

    UiLanguage currentLanguage;
    bool portOpen;
    qint64 totalSendBytes;
    qint64 totalReceiveBytes;
};

#endif // MAINWINDOW_H
