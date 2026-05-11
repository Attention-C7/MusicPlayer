#include "commanddispatcher.h"

#include "volumesafety.h"

#include <QWidget>
#include <QtGlobal>

namespace {
constexpr int kVolumeStepPercent = 10;
}

CommandDispatcher::CommandDispatcher(PlayerController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_volumeWarningParent(nullptr)
{
}

void CommandDispatcher::setVolumeWarningParent(QWidget *parent)
{
    m_volumeWarningParent = parent;
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

    if (cmd.isUiNavigationAction()) {
        handleUiNavigation(cmd);
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

    if (cmd.isPlaylistAction()){
        handlePlaylist(cmd);
        return;
    }

    emit dispatchResult(false, QStringLiteral("暂不支持该指令"));
}
//用isXxxAction()判断而不是switch
//每个分支处理完直接return
//兜底emit失败结果

void CommandDispatcher::handleUiNavigation(const Command &cmd)
{
    switch (cmd.action) {
    case CommandAction::UiShowList:
        emit showListRequested();
        emit dispatchResult(true, QStringLiteral("已打开列表"));
        break;
    case CommandAction::UiHideList:
        emit backToPlayerRequested();
        emit dispatchResult(true, QStringLiteral("已返回播放"));
        break;
    default:
        break;
    }
}

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
            m_controller->requestPlay();
            emit dispatchResult(true, QStringLiteral("已播放"));
            break;

        case CommandAction::PlaybackPause:
            m_controller->requestPause();
            emit dispatchResult(true, QStringLiteral("已暂停"));
            break;
        
        case CommandAction::PlaybackSeek: {
            qint64 posMs = 0;
            if (cmd.params.contains(QStringLiteral("position"))) {
                posMs = static_cast<qint64>(
                    cmd.params.value(QStringLiteral("position")).toLongLong());
            } else {
                posMs = m_controller->playbackPositionMs()
                    + static_cast<qint64>(
                          cmd.params.value(QStringLiteral("offsetMs")).toLongLong());
            }
            m_controller->seek(posMs);
            emit dispatchResult(true, QStringLiteral("已跳转进度"));
            break;
        }

        default:
            break;
    }
}
//PlaybackPlay → requestPlay，PlaybackPause → requestPause（会话层与 QMediaPlayer 状态分离）
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
    switch (cmd.action){
        case CommandAction::VolumeUp: {
            const int cur = m_controller->volumePercent();
            const int next = qMin(100, cur + kVolumeStepPercent);
            if (!VolumeSafety::confirmHighVolumeIfNeeded(next, cur, m_volumeWarningParent)) {
                emit dispatchResult(false, QStringLiteral("已取消增大音量"));
                return;
            }
            m_controller->adjustVolumePercent(kVolumeStepPercent);
            emit dispatchResult(
                true,
                QStringLiteral("音量：%1%").arg(m_controller->volumePercent()));
            break;
        }

        case CommandAction::VolumeDown:
            m_controller->adjustVolumePercent(-kVolumeStepPercent);
            emit dispatchResult(
                true,
                QStringLiteral("音量：%1%").arg(m_controller->volumePercent()));
            break;
        case CommandAction::VolumeSet:{
            const int vol = cmd.params
                              .value(QStringLiteral("volume"))
                              .toInt();
            const int cur = m_controller->volumePercent();
            if (!VolumeSafety::confirmHighVolumeIfNeeded(vol, cur, m_volumeWarningParent)) {
                emit dispatchResult(false, QStringLiteral("已取消设置音量"));
                return;
            }
            m_controller->setVolumePercent(vol);
            emit dispatchResult(
                true,
                QStringLiteral("音量已设为 %1%").arg(m_controller->volumePercent()));
            break;
        }
        case CommandAction::VolumeMute:
            if (m_controller->isMuted()) {
                emit dispatchResult(true, QStringLiteral("已是静音"));
                break;
            }
            m_controller->setMuted(true);
            emit dispatchResult(true, QStringLiteral("已静音"));
            break;
        case CommandAction::VolumeUnmute:
            if (!m_controller->isMuted()) {
                emit dispatchResult(true, QStringLiteral("当前未静音"));
                break;
            }
            {
                const int restore = m_controller->volumePercentBeforeMute();
                const int effectiveRestore = restore > 0 ? restore : 50;
                const int curOut = m_controller->volumePercent();
                if (!VolumeSafety::confirmHighVolumeIfNeeded(
                        effectiveRestore, curOut, m_volumeWarningParent)) {
                    emit dispatchResult(false, QStringLiteral("已取消取消静音"));
                    break;
                }
                m_controller->setMuted(false);
                emit dispatchResult(true, QStringLiteral("已取消静音"));
            }
            break;
        default:
            break;
    }
}

void CommandDispatcher::handlePlaylist(const Command &cmd){
    switch (cmd.action) {
    case CommandAction::PlaylistShuffle:
        m_controller->setPlayMode(PlayMode::RandomPlay);
        emit dispatchResult(true, QStringLiteral("已设置：随机播放"));
        break;
    case CommandAction::PlaylistLoopSingle:
        m_controller->setPlayMode(PlayMode::SingleLoop);
        emit dispatchResult(true, QStringLiteral("已设置：单曲循环"));
        break;
    case CommandAction::PlaylistLoopAll:
        m_controller->setPlayMode(PlayMode::AllLoop);
        emit dispatchResult(true, QStringLiteral("已设置：列表循环（全部）"));
        break;
    case CommandAction::PlaylistLoopFolder:
        m_controller->setPlayMode(PlayMode::FolderLoop);
        emit dispatchResult(true, QStringLiteral("已设置：文件夹循环"));
        break;
    default:
        emit dispatchResult(false, QStringLiteral("暂不支持该列表指令"));
        break;
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