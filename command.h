#pragma once
#include <QString>
#include <QVariantMap> //本质是QMap<QString, QVariant>,用于存储键值对：命令名称→命令参数。

enum class CommandAction {
    //Playback:播放控制
    PlaybackNext,
    PlaybackPrev,
    PlaybackPlay,
    PlaybackPause,
    PlaybackSeek,
    //Music:音乐搜索
    MusicPlay,
    MusicSearch,
    //Volume:音量控制
    VolumeUp,
    VolumeDown,
    VolumeSet,
    //Playlist:列表操作
    PlaylistShuffle,
    //兜底
    Unknown,
};

struct CommandTarget {  //target描述作用于谁
    QString type;   //type = "artist"/"album"/"title"/"file"
    QString keyword;    //搜索关键词
    bool isEmpty() const {
        return keyword.trimmed().isEmpty();
    }
};
//NLU层统一输出格式
struct Command {
    QString version = "1.0";
    CommandAction action = CommandAction::Unknown;
    CommandTarget target;
    QVariantMap params;
    QString source;  //source = "local"/"remote"
    float confidence = 1.0f;    //本地规则固定1.0，LLM返回实际值
    bool valid = false;

    //判断是否为播放控制类指令
    bool isPlaybackAction() const {
        return action == CommandAction::PlaybackNext 
            || action == CommandAction::PlaybackPrev
            || action == CommandAction::PlaybackPlay 
            || action == CommandAction::PlaybackPause 
            || action == CommandAction::PlaybackSeek;
    }

    //判断是否为音乐搜索类指令
    bool isMusicAction() const {
        return action == CommandAction::MusicPlay 
            || action == CommandAction::MusicSearch;
    }

    //判断是否为音量类指令
    bool isVolumeAction() const {
        return action == CommandAction::VolumeUp 
            || action == CommandAction::VolumeDown
            || action == CommandAction::VolumeSet;
    }
};
