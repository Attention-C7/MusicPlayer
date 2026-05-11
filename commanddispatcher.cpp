#include "commanddispatcher.h"

#include <QtGlobal>

CommandDispatcher::CommandDispatcher(PlayerController *controller, QObject *parent)
    : QObject(parent), m_controller(controller){
        //成员已通过初始化列表赋值，无需额外操作
    }


void CommandDispatcher::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap
){
    m_allSongs = allSongs;
    m_artistMap = artistMap;
    m_albumMap = albumMap;
}

void CommandDispatcher::dispatch(const Command &cmd){
    if (!cmd.valid){
        emit dispatchResult(false, QStringLiteral("无效指令"));
        return;
    }

    if (cmd.isPlaybackAction()){
        handlePlayback(cmd);
        return;
    }

    if (cmd.isMusicAction()){
        handleMusic(cmd);
        return;
    }

    if(cmd.isVolumeAction()){
        handleVolume(cmd);
        return;
    }

    if(cmd.action == CommandAction::PlaylistShuffle){
        handlePlaylist(cmd);
        return;
    }

    emit dispatchResult(false, QStringLiteral("暂不支持该指令"));
}
//用isXxxAction()判断而不是switch
//每个分支处理完直接return
//兜底emit失败结果

void CommandDispatcher::handlePlayback(const Command &cmd){
    switch (cmd.action){
        case CommandAction::PlaybackNext:
            m_controller->next();
            emit dispatchResult(true, QStringLiteral("已切换，下一首"));
            break;

        case CommandAction::PlaybackPrev:
            m_controller->prev();
            emit dispatchResult(true, QStringLiteral("已切换，上一首"));
            break;
        
        case CommandAction::PlaybackPlay:
        case CommandAction::PlaybackPause:
            m_controller->playPause();
            emit dispatchResult(true, QStringLiteral("已切换：播放/暂停"));
            break;
        
        case CommandAction::PlaybackSeek: {
            const qint64 pos = static_cast<qint64>(
                cmd.params.value(QStringLiteral("position")).toLongLong());
            m_controller->seek(pos);
            emit dispatchResult(true, QStringLiteral("已跳转进度"));
            break;
        }

        default:
            break;
    }
}
//PlaybackPlay和PlaybackPause共用同一个处理分支,因为底层都是playPause()切换
//Seek的position从params取,转qint64
//default空处理,dispatch已保证分发正确性

void CommandDispatcher::handleMusic(const Command &cmd){
    if (m_allSongs.isEmpty()){
        emit dispatchResult(false, QStringLiteral("歌曲库未加载"));
        return;
    }

    const QString &keyword = cmd.target.keyword;
    const QString &type = cmd.target.type;
    int targetIndex = -1;

    //按type选择搜索策略
    if (type == QStringLiteral("artist")){
        targetIndex = findByArtist(keyword);
    }else if(type == QStringLiteral("album")){
        targetIndex = findByAlbum(keyword);
    }else{
        //title or other,first title,then artist
        targetIndex = findByTitle(keyword);
        if (targetIndex < 0)
            targetIndex = findByArtist(keyword);
    }

    if (targetIndex < 0){
        emit dispatchResult(false, QStringLiteral("未找到相关歌曲:") + keyword);
        return ;
    }

    //找到，构建PlayContext播放
    PlayContext ctx;
    ctx.scopeList = m_allSongs;
    ctx.scopeIndex = targetIndex;
    ctx.globalIndex = targetIndex;
    ctx.source = PlayContext::Source::All;

    m_controller->setPlaylist(m_allSongs);
    m_controller->setContext(ctx);
    m_controller->playSong(targetIndex);

    emit dispatchResult(true, QStringLiteral("正在播放：") + m_allSongs[targetIndex].title);

}

void CommandDispatcher::handleVolume(const Command &cmd){
    //当前音量从controller获取
    //PlayerController需新增getVolume()方法
    //暂时用固定步进50
    switch (cmd.action){
        case CommandAction::VolumeUp:
            //后续接入真实音量控制
            emit dispatchResult(true, QStringLiteral("音量已调大"));
            break;
        
        case CommandAction::VolumeDown:
            emit dispatchResult(true, QStringLiteral("音量已调小"));
            break;
        case CommandAction::VolumeSet:{
            int vol = cmd.params
                        .value(QStringLiteral("Volume"))
                        .toInt();
            Q_UNUSED(vol);
            emit dispatchResult(true, QStringLiteral("音量已设置"));
            break;
        }
        default:
            break;
    }
}
//Volume控制需要QAudioOutput::setVolume()
//现阶段先做框架，Q_UNUSED(vol)标记为暂未使用的变量避免编译警告
//后续在PlayerController里暴露音量接口再补全

void CommandDispatcher::handlePlaylist(const Command &cmd){
    if (cmd.action == CommandAction::PlaylistShuffle){
        m_controller->setPlayMode(PlayMode::RandomPlay);
        emit dispatchResult(true, QStringLiteral("已设置：随机播放"));
    }
}

int CommandDispatcher::findByTitle(const QString &keyword) const{
    for (int i = 0; i < m_allSongs.size(); ++i){
        if (m_allSongs[i].title.contains(keyword, Qt::CaseInsensitive)){
            return i;
        }
    }
    return -1;
}

int CommandDispatcher::findByArtist(const QString &keyword) const{
    for (auto it = m_artistMap.constBegin(); it != m_artistMap.constEnd(); ++it){
        if (it.key().contains(keyword, Qt::CaseInsensitive)){
            //找到歌手，取第一首歌
            if (!it.value().isEmpty()){
                QString path = it.value().first().filePath;
                //在全量表里找对应index
                for (int i = 0; i < m_allSongs.size(); ++i){
                    if (m_allSongs[i].filePath == path){
                        return i;
                    }
                }
            }
        }
    }
    return -1;
}

int CommandDispatcher::findByAlbum(const QString &keyword) const{
    for (auto it = m_albumMap.constBegin(); it != m_albumMap.constEnd(); ++it){
        if (it.key().contains(keyword, Qt::CaseInsensitive)){
            //找到专辑，取第一首歌
            if (!it.value().isEmpty()){
                QString path = it.value().first().filePath;
                //在全量表里找对应index
                for (int i =0; i < m_allSongs.size(); ++i){
                    if (m_allSongs[i].filePath == path){
                        return i;
                    }
                }
            }
        }
    }
    return -1; 
}
//Qt::CaseInsensitive大小写不敏感匹配
//contains模糊匹配，不要求完全相等
//artistMap的key是歌手名，找到后取第一首返回全量index