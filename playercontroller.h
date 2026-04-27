#pragma once

#include <QObject>
#include <QAudioOutput>
#include <QList>
#include <QMediaPlayer>
#include <QTimer>

#include "songinfo.h"

class PlayerController : public QObject
{
    Q_OBJECT

public:
    explicit PlayerController(QObject *parent = nullptr);

    void setPlaylist(QList<SongInfo> songs);
    void playSong(int index);
    void playPause();
    void prev();
    void next();
    void seek(qint64 position);
    void setPlayMode(PlayMode mode);
    int currentIndex() const;
    int playlistCount() const;
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

private:
    static constexpr int SeekIntervalMs = 100;
    static constexpr qint64 SeekStepMs = 3000;

    void playByIndex(int index);
    void updateSongMetaData();
    int randomIndexExcludingCurrent() const;
    void handleEndOfMedia();

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QList<SongInfo> m_playlist;
    int m_currentIndex;
    PlayMode m_playMode;
    QTimer *m_seekTimer;
    int m_seekDirection;
};
