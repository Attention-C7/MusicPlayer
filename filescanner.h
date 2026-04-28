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
    static QList<SongInfo> scanAllFiles(const QString &rootPath);
    static QStringList scanSubDirs(const QString &dirPath);

private:
    static bool hasAudioFiles(const QString &dirPath);
};
