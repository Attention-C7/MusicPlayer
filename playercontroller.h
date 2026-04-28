#pragma once

#include <QObject>
#include <QAudioOutput>
#include <QList>
#include <QMediaPlayer>
#include <QPixmap>
#include <QTimer>

#include "lrcparser.h"
#include "songinfo.h"

class PlayerController : public QObject
{
    Q_OBJECT

public:
    explicit PlayerController(QObject *parent = nullptr);

    void setPlaylist(QList<SongInfo> songs);
    void setFolderPlaylist(QList<SongInfo> songs);
    void setGroupPlaylist(QList<SongInfo> songs);
    void playSong(int index);
    void playPause();
    void prev();
    void next();
    void seek(qint64 position);
    void setPlayMode(PlayMode mode);
    int currentIndex() const;
    int folderIndex() const;
    int groupIndex() const;
    int playlistCount() const;
    int activePlaylistCount() const;
    PlayMode playMode() const;
    void startSeekForward();
    void stopSeekForward();
    void startSeekBackward();
    void stopSeekBackward();

signals:
    void songChanged(SongInfo info);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void playModeChanged(PlayMode mode);
    void currentIndexChanged(int index);
    void errorOccurred(QString message);
    void albumArtChanged(QPixmap pixmap);
    void lrcLoaded(QMap<qint64, QString> lyrics);
    void playlistMetaUpdated(int index, SongInfo info);

private:
    static constexpr int SeekIntervalMs = 100;
    static constexpr qint64 SeekStepMs = 3000;

    void playByIndex(int index);
    void loadLrc(const SongInfo &song);
    void updateSongMetaData();
    int randomIndexExcludingCurrent() const;
    void handleEndOfMedia();

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QList<SongInfo> m_playlist;
    QList<SongInfo> m_folderPlaylist;
    QList<SongInfo> m_groupPlaylist;
    int m_currentIndex;
    PlayMode m_playMode;
    QTimer *m_seekTimer;
    int m_seekDirection;
};
