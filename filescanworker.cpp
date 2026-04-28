#include "filescanworker.h"

#include "filescanner.h"

FileScanWorker::FileScanWorker(QObject *parent)
    : QObject(parent)
    , m_cancelled(false)
{
}

void FileScanWorker::cancel()
{
    m_cancelled = true;
}

void FileScanWorker::startScan(QString rootPath)
{
    m_cancelled = false;

    const QList<SongInfo> allSongs = FileScanner::scanAllFiles(rootPath);
    QList<SongInfo> result;
    result.reserve(allSongs.size());

    const int total = allSongs.size();
    for (int i = 0; i < total; ++i) {
        if (m_cancelled) {
            return;
        }
        result.append(allSongs[i]);
        emit scanProgress(i + 1, total);
    }

    emit scanFinished(result);
}
