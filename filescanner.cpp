#include "filescanner.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

namespace
{
QStringList audioNameFilters()
{
    return {"*.mp3", "*.wav", "*.wma", "*.MP3", "*.WAV", "*.WMA"};
}
}

QList<SongInfo> FileScanner::scanFiles(const QString &dirPath)
{
    QList<SongInfo> songs;

    const QDir dir(dirPath);
    if (!dir.exists()) {
        return songs;
    }

    const QFileInfoList fileList = dir.entryInfoList(
        audioNameFilters(),
        QDir::Files | QDir::NoSymLinks | QDir::Readable
    );

    songs.reserve(fileList.size());
    for (const QFileInfo &fileInfo : fileList) {
        SongInfo song;
        song.filePath = fileInfo.absoluteFilePath();

        // ID3 metadata will be parsed in PlayerController.
        const QString baseName = fileInfo.completeBaseName();
        song.title = baseName;
        song.artist = QString();
        song.album = QString();
        song.duration = 0;

        songs.append(song);
    }

    return songs;
}

QStringList FileScanner::scanSubDirs(const QString &dirPath)
{
    QStringList validSubDirs;

    const QDir rootDir(dirPath);
    if (!rootDir.exists()) {
        return validSubDirs;
    }

    const QFileInfoList subDirList = rootDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable
    );

    for (const QFileInfo &subDirInfo : subDirList) {
        const QString subDirPath = subDirInfo.absoluteFilePath();
        if (hasAudioFiles(subDirPath)) {
            validSubDirs.append(subDirPath);
        }
    }

    return validSubDirs;
}

bool FileScanner::hasAudioFiles(const QString &dirPath)
{
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return false;
    }

    const QFileInfoList audioFiles = dir.entryInfoList(
        audioNameFilters(),
        QDir::Files | QDir::NoSymLinks | QDir::Readable
    );
    if (!audioFiles.isEmpty()) {
        return true;
    }

    const QFileInfoList subDirList = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable
    );

    for (const QFileInfo &subDirInfo : subDirList) {
        if (hasAudioFiles(subDirInfo.absoluteFilePath())) {
            return true;
        }
    }

    return false;
}
