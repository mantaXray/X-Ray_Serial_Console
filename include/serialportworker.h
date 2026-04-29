/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: serialportworker.h
 * Version: 1.00.00
 * Description: 串口工作线程对象声明
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#ifndef SERIALPORTWORKER_H
#define SERIALPORTWORKER_H

#include <QObject>
#include <QString>

class QTimer;

// 串口工作对象运行在独立线程里，负责和 Windows 串口 API 直接交互。
// 这样主线程只关心界面，不需要承担阻塞式串口读写。
class SerialPortWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortWorker(QObject *parent = nullptr);
    ~SerialPortWorker() override;

signals:
    // 把串口状态和收发结果回传给主线程，供界面展示。
    void portOpened(const QString &summary);
    void portClosed(const QString &summary);
    void errorOccurred(const QString &message);
    void dataReceived(const QByteArray &data);
    void bytesWritten(qint64 count);
    void modemStatusChanged(bool cts, bool dsr, bool rlsd, bool ring);

public slots:
    // 直接接收界面传来的文本参数，内部再统一转换为 Windows API 需要的配置。
    void openPort(const QString &portName,
                  int baudRate,
                  int dataBits,
                  const QString &parityText,
                  const QString &stopBitsText);
    void closePort();
    void writeData(const QByteArray &data);
    void setRtsEnabled(bool enabled);
    void setDtrEnabled(bool enabled);

private slots:
    // 通过定时轮询读取串口输入队列，逻辑简单且足够满足这个工具的实时性需求。
    void pollPort();

private:
    // buildSummary 用于生成统一的状态栏摘要，
    // closeHandle 只负责释放句柄，避免资源回收逻辑散落在多个函数里。
    QString buildSummary(const QString &stateText) const;
    void updateModemStatus();
    void closeHandle();

    // 使用 void* 保存原生句柄，避免在头文件里引入 windows.h，减少编译耦合。
    void *portHandle;
    QTimer *pollTimer;
    QString currentPortName;
    int currentBaudRate;
    int currentDataBits;
    QString currentParity;
    QString currentStopBits;
    bool rtsEnabled;
    bool dtrEnabled;
    unsigned long lastModemStatus;
};

#endif // SERIALPORTWORKER_H
