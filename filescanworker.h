#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "songinfo.h"

class FileScanWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileScanWorker(QObject *parent = nullptr);

    void cancel();

public slots:
    void startScan(QString rootPath);

signals:
    void scanProgress(int current, int total);
    void scanFinished(QList<SongInfo> songs);
    void scanError(QString message);

private:
    bool m_cancelled;
};
