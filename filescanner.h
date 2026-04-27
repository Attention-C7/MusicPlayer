#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include "songinfo.h"

class FileScanner : public QObject
{
public:
    static QList<SongInfo> scanFiles(const QString &dirPath);
    static QStringList scanSubDirs(const QString &dirPath);

private:
    static bool hasAudioFiles(const QString &dirPath);
};
