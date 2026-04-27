#include "playercontroller.h"

#include <QMediaMetaData>
#include <QRandomGenerator>
#include <QUrl>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_currentIndex(-1)
    , m_playMode(PlayMode::AllLoop)
    , m_seekTimer(new QTimer(this))
    , m_seekDirection(0)
{
    m_player->setAudioOutput(m_audioOutput);

    m_seekTimer->setInterval(SeekIntervalMs);

    connect(m_seekTimer, &QTimer::timeout, this, [this]() {
        if (m_seekDirection == 0) {
            return;
        }
        seek(m_player->position() + static_cast<qint64>(m_seekDirection) * SeekStepMs);
    });

    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerController::positionChanged);

    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            m_playlist[m_currentIndex].duration = duration;
            emit songChanged(m_playlist[m_currentIndex]);
        }
        emit durationChanged(duration);
    });

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &PlayerController::playbackStateChanged);

    connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
        updateSongMetaData();
    });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            handleEndOfMedia();
        }
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString) {
        if (!errorString.isEmpty()) {
            emit errorOccurred(errorString);
        }
    });
}

void PlayerController::setPlaylist(QList<SongInfo> songs)
{
    m_playlist = songs;
    if (m_playlist.isEmpty()) {
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    } else if (m_currentIndex >= m_playlist.size()) {
        m_currentIndex = 0;
        emit currentIndexChanged(m_currentIndex);
    }
}

void PlayerController::playSong(int index)
{
    playByIndex(index);
}

void PlayerController::playPause()
{
    if (m_playlist.isEmpty()) {
        return;
    }

    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        playByIndex(0);
        return;
    }

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void PlayerController::prev()
{
    if (m_playlist.isEmpty()) {
        return;
    }

    if (m_playMode == PlayMode::RandomPlay) {
        playByIndex(randomIndexExcludingCurrent());
        return;
    }

    if (m_currentIndex <= 0) {
        playByIndex(m_playlist.size() - 1);
    } else {
        playByIndex(m_currentIndex - 1);
    }
}

void PlayerController::next()
{
    if (m_playlist.isEmpty()) {
        return;
    }

    if (m_playMode == PlayMode::RandomPlay) {
        playByIndex(randomIndexExcludingCurrent());
        return;
    }

    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size() - 1) {
        playByIndex(0);
    } else {
        playByIndex(m_currentIndex + 1);
    }
}

void PlayerController::seek(qint64 position)
{
    qint64 maxDuration = m_player->duration();
    if (maxDuration < 0) {
        maxDuration = 0;
    }
    const qint64 clampedPosition = qBound<qint64>(0, position, maxDuration);
    m_player->setPosition(clampedPosition);
}

void PlayerController::setPlayMode(PlayMode mode)
{
    if (m_playMode == mode) {
        return;
    }
    m_playMode = mode;
    emit playModeChanged(m_playMode);
}

int PlayerController::currentIndex() const
{
    return m_currentIndex;
}

int PlayerController::playlistCount() const
{
    return m_playlist.size();
}

PlayMode PlayerController::playMode() const
{
    return m_playMode;
}

void PlayerController::startSeekForward()
{
    m_seekDirection = 1;
    if (!m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void PlayerController::stopSeekForward()
{
    if (m_seekDirection == 1) {
        m_seekDirection = 0;
        m_seekTimer->stop();
    }
}

void PlayerController::startSeekBackward()
{
    m_seekDirection = -1;
    if (!m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void PlayerController::stopSeekBackward()
{
    if (m_seekDirection == -1) {
        m_seekDirection = 0;
        m_seekTimer->stop();
    }
}

void PlayerController::playByIndex(int index)
{
    if (index < 0 || index >= m_playlist.size()) {
        return;
    }

    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);
    emit songChanged(m_playlist[m_currentIndex]);

    const QUrl source = QUrl::fromLocalFile(m_playlist[m_currentIndex].filePath);
    m_player->setSource(source);
    m_player->play();
}

void PlayerController::updateSongMetaData()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return;
    }

    SongInfo &song = m_playlist[m_currentIndex];
    const QMediaMetaData metaData = m_player->metaData();

    const QString title = metaData.value(QMediaMetaData::Title).toString().trimmed();
    const QVariant artistValue = metaData.value(QMediaMetaData::ContributingArtist);
    QString artist;
    if (artistValue.canConvert<QStringList>()) {
        artist = artistValue.toStringList().join(", ").trimmed();
    } else {
        artist = artistValue.toString().trimmed();
    }
    const QString album = metaData.value(QMediaMetaData::AlbumTitle).toString().trimmed();

    if (!title.isEmpty()) {
        song.title = title;
    }
    if (!artist.isEmpty()) {
        song.artist = artist;
    }
    if (!album.isEmpty()) {
        song.album = album;
    }

    emit songChanged(song);
}

int PlayerController::randomIndexExcludingCurrent() const
{
    if (m_playlist.isEmpty()) {
        return -1;
    }
    if (m_playlist.size() == 1) {
        return 0;
    }

    int index = m_currentIndex;
    while (index == m_currentIndex) {
        index = QRandomGenerator::global()->bounded(m_playlist.size());
    }
    return index;
}

void PlayerController::handleEndOfMedia()
{
    if (m_playlist.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return;
    }

    switch (m_playMode) {
    case PlayMode::SingleLoop:
        m_player->setPosition(0);
        m_player->play();
        break;
    case PlayMode::FolderLoop:
    case PlayMode::AllLoop:
        next();
        break;
    case PlayMode::RandomPlay:
        playByIndex(randomIndexExcludingCurrent());
        break;
    }
}
