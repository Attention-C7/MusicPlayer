#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include "songinfo.h"   //扫描结果统一为 SongInfo，与 PlayerController、ListWidget 使用同一套模型。

class FileScanner : public QObject  //把「扫盘 + 读标签」能力收在一个类型里；当前接口全是 static，不依赖实例状态。
{
public:
    static QList<SongInfo> scanFiles(const QString &dirPath);   //只扫单层目录：在该路径下列出符合扩展名过滤的音频文件，填好 SongInfo（路径、同基名 .lrc、TagLib 元数据等），不递归子目录。
    static QList<SongInfo> scanAllFiles(const QString &rootPath);   //从根目录递归整棵树：先扫根目录自身，再用 QDirIterator 遍历所有子目录，对每个目录调用 scanFiles，合并成一个大列表（实现见 filescanner.cpp）。
    static QStringList scanSubDirs(const QString &dirPath);     //列出「值得进入」的子目录路径：直接子文件夹中，若 hasAudioFiles（当前目录或子孙中有音频）为真，则加入结果，供目录浏览器只显示「可能有歌」的文件夹。

private:
    static bool hasAudioFiles(const QString &dirPath);  //判断某目录下（含递归子目录）是否存在音频文件；供 scanSubDirs 过滤空壳文件夹。
};
