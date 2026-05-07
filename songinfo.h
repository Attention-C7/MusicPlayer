#pragma once

#include <QList>        //PlayContext::scopeList 使用 QList<SongInfo>。
#include <QtGlobal>     //提供 qint64 等 Qt 整数别名，保证跨平台宽度一致。
#include <QString>      //路径与元数据一律用 QString。

struct SongInfo  //用POD式聚合描述「一首歌」在应用里传递时需要携带的信息；不设封装接口，谁都可以读写成员。
{
    QString filePath; // Full file path音频文件完整路径；PlayerController 用来 setSource / 比较当前播放文件；ListWidget 用来高亮；语音匹配也依赖路径或展示名。
    QString lrcPath;  // Full .lrc lyric file path.对应 .lrc 歌词文件完整路径；控制器或歌词加载逻辑据此读文件（与 QMediaPlayer 时间轴无关，只是磁盘上的歌词源）。
    QString title;    // Song title (ID3).来自 ID3/元数据 的展示字段；为空时 UI 里常回退到 QFileInfo(filePath).completeBaseName() 等（在 playwidget.cpp 等处可见）。
    QString artist;   // Artist (ID3)
    QString album;    // Album (ID3)
    qint64 duration = 0; // Duration in milliseconds.时长，毫秒；与 QMediaPlayer::duration() / position() 单位一致，也符合项目「时长统一 qint64 毫秒」的规则。= 0 表示默认「未知/零」，构造时不必手写初始化。
};

struct PlayContext {   //播放上下文：逻辑范围 + 范围内下标 + 全量表播放下标（重构 PlayerController 内部状态用）。
    QList<SongInfo> scopeList;  // 当前范围列表
    int scopeIndex = -1;        // 在 scopeList 中的位置
    int globalIndex = -1;       // 在全量表中的位置（唯一播放索引）
    enum Source { Folder, Group, All } source = All;
};

enum class PlayMode  //播放模式枚举：强类型枚举，避免与整数隐式混用。
{
    SingleLoop, // Single song loop
    FolderLoop, // Loop current folder
    AllLoop,    // Loop all songs
    RandomPlay  // Random play
};
