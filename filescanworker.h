#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "songinfo.h"

class FileScanWorker : public QObject
{
    Q_OBJECT  //Q_OBJECT 宏声明 FileScanWorker 类为 Qt 对象，自动生成信号和槽机制。本类有 signals 和 Q_PROPERTY，必须由 moc 处理。

public:
    explicit FileScanWorker(QObject *parent = nullptr); //构造：必须传入 QObject *parent。

    void cancel();// 取消扫描

public slots:
    void startScan(QString rootPath); //启动扫描

signals:
    void scanProgress(int current, int total); //扫描进度
    void scanFinished(QList<SongInfo> songs); //扫描完成
    void scanError(QString message); //扫描错误

private:
    bool m_cancelled; //扫描是否被取消：true 表示取消，false 表示未取消。
};
