#include "playercontroller.h"

#include <QMediaMetaData>
#include <QPixmap>
#include <QRandomGenerator>
#include <QUrl>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_currentIndex(-1)
    , m_playMode(PlayMode::FolderLoop)
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

void PlayerController::setFolderPlaylist(QList<SongInfo> songs)
{
    m_folderPlaylist = songs;
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

    const QList<SongInfo> &activeList =
        (m_playMode == PlayMode::AllLoop || m_folderPlaylist.isEmpty()) ? m_playlist : m_folderPlaylist;
    if (activeList.isEmpty()) {
        return;
    }

    QString currentFilePath;
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        currentFilePath = m_playlist[m_currentIndex].filePath;
    } else {
        currentFilePath = activeList.first().filePath;
    }

    int currentActiveIndex = -1;
    for (int i = 0; i < activeList.size(); ++i) {
        if (activeList[i].filePath == currentFilePath) {
            currentActiveIndex = i;
            break;
        }
    }
    if (currentActiveIndex < 0) {
        currentActiveIndex = 0;
    }

    if (m_playMode == PlayMode::RandomPlay && activeList.size() > 1) {
        int randomActiveIndex = currentActiveIndex;
        while (randomActiveIndex == currentActiveIndex) {
            randomActiveIndex = QRandomGenerator::global()->bounded(activeList.size());
        }
        const QString targetPath = activeList[randomActiveIndex].filePath;
        for (int i = 0; i < m_playlist.size(); ++i) {
            if (m_playlist[i].filePath == targetPath) {
                playByIndex(i);
                return;
            }
        }
        return;
    }

    const int prevActiveIndex = (currentActiveIndex <= 0) ? (activeList.size() - 1) : (currentActiveIndex - 1);
    const QString targetPath = activeList[prevActiveIndex].filePath;
    for (int i = 0; i < m_playlist.size(); ++i) {
        if (m_playlist[i].filePath == targetPath) {
            playByIndex(i);
            return;
        }
    }
}

void PlayerController::next()
{
    if (m_playlist.isEmpty()) {
        return;
    }

    const QList<SongInfo> &activeList =
        (m_playMode == PlayMode::AllLoop || m_folderPlaylist.isEmpty()) ? m_playlist : m_folderPlaylist;
    if (activeList.isEmpty()) {
        return;
    }

    QString currentFilePath;
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        currentFilePath = m_playlist[m_currentIndex].filePath;
    } else {
        currentFilePath = activeList.first().filePath;
    }

    int currentActiveIndex = -1;
    for (int i = 0; i < activeList.size(); ++i) {
        if (activeList[i].filePath == currentFilePath) {
            currentActiveIndex = i;
            break;
        }
    }

    if (m_playMode == PlayMode::RandomPlay && activeList.size() > 1) {
        int randomActiveIndex = currentActiveIndex;
        while (randomActiveIndex == currentActiveIndex) {
            randomActiveIndex = QRandomGenerator::global()->bounded(activeList.size());
        }
        const QString targetPath = activeList[randomActiveIndex].filePath;
        for (int i = 0; i < m_playlist.size(); ++i) {
            if (m_playlist[i].filePath == targetPath) {
                playByIndex(i);
                return;
            }
        }
        return;
    }

    const int nextActiveIndex =
        (currentActiveIndex < 0 || currentActiveIndex >= activeList.size() - 1) ? 0 : (currentActiveIndex + 1);
    const QString targetPath = activeList[nextActiveIndex].filePath;
    for (int i = 0; i < m_playlist.size(); ++i) {
        if (m_playlist[i].filePath == targetPath) {
            playByIndex(i);
            return;
        }
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

int PlayerController::activePlaylistCount() const
{
    if (m_playMode == PlayMode::AllLoop) {
        return m_playlist.size();
    }
    if (!m_folderPlaylist.isEmpty()) {
        return m_folderPlaylist.size();
    }
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
    loadLrc(m_playlist[m_currentIndex]);
}

void PlayerController::loadLrc(const SongInfo &song)
{
    if (song.lrcPath.isEmpty()) {
        emit lrcLoaded(QMap<qint64, QString>());
        return;
    }

    const QMap<qint64, QString> result = LrcParser::parse(song.lrcPath);
    emit lrcLoaded(result);
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

    QPixmap albumArt;
    const QVariant thumbnailValue = metaData.value(QMediaMetaData::ThumbnailImage);
    if (thumbnailValue.canConvert<QPixmap>()) {
        albumArt = qvariant_cast<QPixmap>(thumbnailValue);
    } else if (thumbnailValue.canConvert<QImage>()) {
        albumArt = QPixmap::fromImage(qvariant_cast<QImage>(thumbnailValue));
    }

    if (albumArt.isNull()) {
        const QVariant coverValue = metaData.value(QMediaMetaData::CoverArtImage);
        if (coverValue.canConvert<QPixmap>()) {
            albumArt = qvariant_cast<QPixmap>(coverValue);
        } else if (coverValue.canConvert<QImage>()) {
            albumArt = QPixmap::fromImage(qvariant_cast<QImage>(coverValue));
        }
    }

    emit albumArtChanged(albumArt);
    emit songChanged(song);
    emit playlistMetaUpdated(m_currentIndex, song);
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
        next();
        break;
    }
}
