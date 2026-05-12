#include "playercontroller.h"

#include <QMediaMetaData>
#include <QPixmap>
#include <QRandomGenerator>
#include <QUrl>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))  //构造里 new QMediaPlayer(this)，父对象为 PlayerController。
    , m_audioOutput(new QAudioOutput(this)) //QMediaPlayer(this)、QAudioOutput(this) 挂在控制器下，随 MusicPlayer 析构一起释放
    , m_currentIndex(-1) //初始无曲目，-1 表示无效索引。尚无合法曲目
    , m_playMode(PlayMode::FolderLoop) //默认目录循环
    , m_seekTimer(new QTimer(this)) //长按 seek 定时器，用于实现长按快进/快退功能
    , m_seekDirection(0) //长按 seek 方向，0 表示停止，1 表示向前，-1 表示向后
    , m_muted(false)
    , m_volumePercentBeforeMute(50)
{
    m_player->setAudioOutput(m_audioOutput); //Qt6 必需的一步，否则无声

    m_seekTimer->setInterval(SeekIntervalMs); //长按 seek 定时器间隔，100ms

    connect(m_seekTimer, &QTimer::timeout, this, [this]() {
        if (m_seekDirection == 0) {
            return;
        }
        seek(m_player->position() + static_cast<qint64>(m_seekDirection) * SeekStepMs); //长按 seek 步进，100ms 一次，每次步进 3000ms
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

    setVolumePercent(50);

    // 节拍检测链：放在构造末尾，避免与上方播放器初始化顺序纠缠；父对象均为 this，随控制器析构。
    m_beatDetector = new BeatDetector(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    m_audioBufferOutput = new QAudioBufferOutput(this);
    m_player->setAudioBufferOutput(m_audioBufferOutput);
    connect(
        m_audioBufferOutput,
        &QAudioBufferOutput::audioBufferReceived,
        m_beatDetector,
        &BeatDetector::feedBuffer,
        Qt::QueuedConnection);
#endif
}

/** UI 可 connect beatDetector 的 beatDetected(float)、bpmUpdated(float)；跨线程建议 QueuedConnection。 */
BeatDetector *PlayerController::beatDetector() const
{
    return m_beatDetector;
}

void PlayerController::setPlaylist(QList<SongInfo> songs)
{
    m_playlist = songs;
    if (m_playlist.isEmpty()) {
        m_currentIndex = -1;
        m_ctx = PlayContext{};
        emit currentIndexChanged(m_currentIndex);
        emitSessionPlaybackActiveIfChanged(false);
        return;
    }
    if (m_currentIndex >= m_playlist.size()) {
        m_currentIndex = 0;
        emit currentIndexChanged(m_currentIndex);
    }
    // 仅替换全量表时：默认上下文为「全库」，避免 prev/next 见到空的 scopeList
    m_ctx.scopeList = m_playlist;
    m_ctx.source = PlayContext::All;
    m_ctx.globalIndex = m_currentIndex;
    m_ctx.scopeIndex =
        (m_currentIndex >= 0 && m_currentIndex < m_ctx.scopeList.size()) ? m_currentIndex : -1;

    /*
    // ——— 旧实现（三张表）：仅替换主表，folder/group 由其它入口单独设置 ———
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
    */
}

void PlayerController::setContext(PlayContext ctx)
{
    const int oldIndex = m_currentIndex;
    m_ctx = std::move(ctx);

    if (!m_playlist.isEmpty()) {
        if (m_ctx.globalIndex >= 0 && m_ctx.globalIndex < m_playlist.size()) {
            m_currentIndex = m_ctx.globalIndex;
        } else if (m_ctx.scopeIndex >= 0 && m_ctx.scopeIndex < m_ctx.scopeList.size()) {
            const QString path = m_ctx.scopeList[m_ctx.scopeIndex].filePath;
            int gi = -1;
            for (int i = 0; i < m_playlist.size(); ++i) {
                if (m_playlist[i].filePath == path) {
                    gi = i;
                    break;
                }
            }
            m_ctx.globalIndex = gi;
            m_currentIndex = gi;
        }
    }

    if (m_currentIndex != oldIndex) {
        emit currentIndexChanged(m_currentIndex);
    }
}

PlayContext PlayerController::currentContext() const
{
    return m_ctx;
}

void PlayerController::playSong(int index)
{
    playByIndex(index);
}

void PlayerController::requestPlay()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        playByIndex(0);
        return;
    }
    m_player->play();
    emitSessionPlaybackActiveIfChanged(true);
}

void PlayerController::requestPause()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    m_player->pause();
    emitSessionPlaybackActiveIfChanged(false);
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
        requestPause();
    } else {
        requestPlay();
    }
}

bool PlayerController::sessionPlaybackActive() const
{
    return m_sessionPlaybackActive;
}

void PlayerController::emitSessionPlaybackActiveIfChanged(bool active)
{
    if (m_sessionPlaybackActive == active) {
        return;
    }
    m_sessionPlaybackActive = active;
    emit sessionPlaybackActiveChanged(active);
}

void PlayerController::prev()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    navigate(-1);

    /*
    // ——— 旧 prev：按 PlayMode 选 activeList（All / group / folder），再路径映射回 m_playlist ———
    void PlayerController::prev()
    {
        if (m_playlist.isEmpty()) {
            return;
        }
        const QList<SongInfo> &activeList = (m_playMode == PlayMode::AllLoop)
                                                ? m_playlist
                                                : (!m_groupPlaylist.isEmpty() ? m_groupPlaylist
                                                                              : (!m_folderPlaylist.isEmpty()
                                                                                     ? m_folderPlaylist
                                                                                     : m_playlist));
        ...
    }
    */
}

void PlayerController::next()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    navigate(1);

    /*
    // ——— 旧 next：同上 ———
    */
}

void PlayerController::navigate(int delta)
{
    if (m_playlist.isEmpty() || m_ctx.scopeList.isEmpty()) {
        return;
    }

    const int n = m_ctx.scopeList.size();
    int scopePos = m_ctx.scopeIndex;
    if (scopePos < 0 || scopePos >= n) {
        QString path;
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            path = m_playlist[m_currentIndex].filePath;
        }
        scopePos = 0;
        for (int i = 0; i < n; ++i) {
            if (m_ctx.scopeList[i].filePath == path) {
                scopePos = i;
                break;
            }
        }
    }

    int newScopeIndex = scopePos;
    if (m_playMode == PlayMode::RandomPlay && n > 1) {
        while (newScopeIndex == scopePos) {
            newScopeIndex = QRandomGenerator::global()->bounded(n);
        }
    } else {
        if (delta == 0) {
            return;
        }
        newScopeIndex = scopePos + delta;
        newScopeIndex %= n;
        if (newScopeIndex < 0) {
            newScopeIndex += n;
        }
    }

    const QString targetPath = m_ctx.scopeList[newScopeIndex].filePath;
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

qint64 PlayerController::playbackPositionMs() const
{
    return m_player->position();
}

int PlayerController::volumePercent() const
{
    const float v = m_audioOutput->volume();
    return qBound(0, static_cast<int>(qRound(static_cast<qreal>(v) * 100.0)), 100);
}

void PlayerController::setVolumePercent(int percent)
{
    const int p = qBound(0, percent, 100);
    m_muted = false;
    const int before = volumePercent();
    m_audioOutput->setVolume(static_cast<float>(p) / 100.0f);
    const int after = volumePercent();
    if (after != before) {
        emit volumePercentChanged(after);
    }
}

void PlayerController::adjustVolumePercent(int delta)
{
    setVolumePercent(volumePercent() + delta);
}

bool PlayerController::isMuted() const
{
    return m_muted;
}

int PlayerController::volumePercentBeforeMute() const
{
    return m_volumePercentBeforeMute;
}

void PlayerController::setMuted(bool muted)
{
    if (m_muted == muted) {
        return;
    }
    if (muted) {
        const int v = volumePercent();
        if (v > 0) {
            m_volumePercentBeforeMute = v;
        }
        m_muted = true;
        m_audioOutput->setVolume(0.0f);
        emit volumePercentChanged(0);
        return;
    }
    m_muted = false;
    const int restore = m_volumePercentBeforeMute > 0 ? m_volumePercentBeforeMute : 50;
    m_audioOutput->setVolume(static_cast<float>(restore) / 100.0f);
    emit volumePercentChanged(volumePercent());
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

int PlayerController::currentScopeIndex() const
{
    return m_ctx.scopeIndex;
}

int PlayerController::playlistCount() const
{
    return m_playlist.size();
}

int PlayerController::activePlaylistCount() const
{
    return m_ctx.scopeList.size();

    /*
    // ——— 旧 activePlaylistCount：按 PlayMode 与 folder/group 是否非空分叉 ———
    int PlayerController::activePlaylistCount() const
    {
        if (m_playMode == PlayMode::AllLoop) {
            return m_playlist.size();
        }
        if (!m_groupPlaylist.isEmpty()) {
            return m_groupPlaylist.size();
        }
        if (!m_folderPlaylist.isEmpty()) {
            return m_folderPlaylist.size();
        }
        return m_playlist.size();
    }
    */
}

PlayMode PlayerController::playMode() const
{
    return m_playMode;
}

QMediaPlayer::PlaybackState PlayerController::playbackState() const
{
    return m_player->playbackState();
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

    emitSessionPlaybackActiveIfChanged(true);

    m_currentIndex = index;
    m_ctx.globalIndex = index;
    const QString path = m_playlist[m_currentIndex].filePath;
    m_ctx.scopeIndex = -1;
    for (int i = 0; i < m_ctx.scopeList.size(); ++i) {
        if (m_ctx.scopeList[i].filePath == path) {
            m_ctx.scopeIndex = i;
            break;
        }
    }

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
        m_currentLyrics.clear();
        emit lrcLoaded(QMap<qint64, QString>());
        return;
    }
    const QMap<qint64, QString> result = LrcParser::parse(song.lrcPath);
    m_currentLyrics = result;
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
    case PlayMode::RandomPlay:
        navigate(1);
        break;
    }
}

/*
// ——— 旧 handleEndOfMedia：SingleLoop 外 FolderLoop/AllLoop/RandomPlay 均走 next() ———
*/

/*
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
*/

/*
// ——— 旧 folderIndex / groupIndex ———
int PlayerController::folderIndex() const { ... }
int PlayerController::groupIndex() const { ... }
*/

/*
void PlayerController::setFolderPlaylist(QList<SongInfo> songs)
{
    m_folderPlaylist = songs;
}
void PlayerController::setGroupPlaylist(QList<SongInfo> songs)
{
    m_groupPlaylist = songs;
}
*/
