#include "filescanworker.h"

#include "filescanner.h"

FileScanWorker::FileScanWorker(QObject *parent)
    : QObject(parent)   //构造：必须传入 QObject *parent。
    , m_cancelled(false) //扫描是否被取消：true 表示取消，false 表示未取消。
{
}

void FileScanWorker::cancel() //取消扫描
{
    m_cancelled = true; //设置扫描是否被取消：true 表示取消，false 表示未取消。
}

void FileScanWorker::startScan(QString rootPath) //启动扫描
{
    m_cancelled = false; //设置扫描是否被取消：true 表示取消，false 表示未取消。

    const QList<SongInfo> allSongs = FileScanner::scanAllFiles(rootPath); //扫描所有歌曲
    QList<SongInfo> result; //扫描结果
    result.reserve(allSongs.size()); //预分配空间

    const int total = allSongs.size(); //总歌曲数，用于进度分母
    for (int i = 0; i < total; ++i) {
        if (m_cancelled) { //如果扫描被取消，则返回
            return;
        }
        result.append(allSongs[i]); //添加歌曲到结果列表
        emit scanProgress(i + 1, total); //发出扫描进度信号
    }

    emit scanFinished(result); //发出扫描完成信号，传入扫描结果
}
