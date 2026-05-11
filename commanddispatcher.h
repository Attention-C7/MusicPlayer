#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "command.h"
#include "playercontroller.h"
#include "songinfo.h"

class QWidget;

class CommandDispatcher : public QObject{
    Q_OBJECT

public:
    explicit CommandDispatcher(PlayerController *controller, QObject *parent = nullptr);

    //听力提示弹窗父控件（通常为 MusicPlayer）；语音调音量前确认用
    void setVolumeWarningParent(QWidget *parent);

    //主调度入口
    void dispatch(const Command &cmd);

public slots:
    //接收ListWidget扫描完成后的全量歌曲
    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );

signals:
    //调度结果通知UI，success=true时是操作描述，false时是错误信息
    void dispatchResult(bool success,QString message);
    //与 PlayWidget::showListRequested、主窗 showList 同源
    void showListRequested();
    //与 ListWidget::backToPlayerRequested、主窗 hideList 同源
    void backToPlayerRequested();

private:
    void handleUiNavigation(const Command &cmd);
    //按域分发
    void handlePlayback(const Command &cmd);
    void handleMusic(const Command &cmd);
    void handleVolume(const Command &cmd);
    void handlePlaylist(const Command &cmd);

    //搜索辅助方法
    //返回在m_allsongs中的index，找不到返回-1
    int findByTitle(const QString &title) const;
    int findByArtist(const QString &artist) const;
    int findByAlbum(const QString &album) const;

    PlayerController *m_controller;
    QWidget *m_volumeWarningParent;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
};