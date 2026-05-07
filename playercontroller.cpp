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
{
    m_player->setAudioOutput(m_audioOutput); //Qt6 必需的一步，否则无声

    m_seekTimer->setInterval(SeekIntervalMs); //长按 seek 定时器间隔，100ms

    connect(m_seekTimer, &QTimer::timeout, this, [this]() {
        if (m_seekDirection == 0) {
            return;
        }
        seek(m_player->position() + static_cast<qint64>(m_seekDirection) * SeekStepMs); //长按 seek 步进，100ms 一次，每次步进 3000ms
    });
    //直接把 QMediaPlayer::positionChanged 接到 PlayerController::positionChanged，UI 拿到的仍是 毫秒
    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerController::positionChanged); //进度变化；PlayWidget 更新进度条。毫秒；驱动进度条与时间标签
    //时长：播放器 durationChanged 时，若当前索引合法，把 m_playlist[m_currentIndex].duration 写成播放器给的时长，再 emit songChanged（让界面刷新「总时长」一类绑定在 SongInfo 上的展示），最后 emit durationChanged(duration)
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            m_playlist[m_currentIndex].duration = duration;
            emit songChanged(m_playlist[m_currentIndex]);
        }
        emit durationChanged(duration);
    });
    //播放状态：播放器 playbackStateChanged 时，更新播放按钮图标等
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &PlayerController::playbackStateChanged);
    //元数据：播放器 metaDataChanged 时，更新 SongInfo（与扫描阶段 TagLib 互补）
    connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
        updateSongMetaData();
    });
    //播放结束：播放器 mediaStatusChanged 时，若播放结束，调用 handleEndOfMedia()
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            handleEndOfMedia();
        }
    });
    //播放错误：播放器 errorOccurred 时，弹窗提示用户
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString) {
        if (!errorString.isEmpty()) {
            emit errorOccurred(errorString);
        }
    });
}
//替换 主表 m_playlist；若为空则 m_currentIndex = -1 并 currentIndexChanged；若索引越界则收到 0。注意：不会自动播放，只修正索引边界。
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
}   //只做赋值，不校验索引；供 ListWidget 在「当前文件夹 / 当前分组」上下文更新 prev/next/计数 时使用。
//替换 目录表 m_folderPlaylist；不会自动播放，只替换表
void PlayerController::setFolderPlaylist(QList<SongInfo> songs)
{
    m_folderPlaylist = songs;
}
//替换 艺人表 m_groupPlaylist；不会自动播放，只替换表
void PlayerController::setGroupPlaylist(QList<SongInfo> songs)
{
    m_groupPlaylist = songs;
}
//统一入口：设置媒体源、更新索引、发 songChanged、加载歌词等
void PlayerController::playSong(int index)
{
    playByIndex(index);
}
//播放/暂停：若无曲目则返回；若索引越界则收到 0；若播放中则暂停，否则播放
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
    if (m_playlist.isEmpty()) { //歌单为空 → 直接返回
        return;
    }
    //根据播放模式，决定当前使用哪个歌单:优先级：AllLoop → 分组歌单 → 文件夹歌单 → 总歌单
    const QList<SongInfo> &activeList = (m_playMode == PlayMode::AllLoop) //当前播放模式决定激活列表
                                            ? m_playlist
                                            : (!m_groupPlaylist.isEmpty() ? m_groupPlaylist
                                                                          : (!m_folderPlaylist.isEmpty() ? m_folderPlaylist
                                                                                                         : m_playlist));
    if (activeList.isEmpty()) { // 有效列表为空 → 返回
        return;
    }
    
    QString currentFilePath; //当前播放文件路径
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        currentFilePath = m_playlist[m_currentIndex].filePath;
    } else {
        currentFilePath = activeList.first().filePath;
    }
    //在 activeList 里找到当前歌曲的位置 currentActiveIndex
    int currentActiveIndex = -1;
    for (int i = 0; i < activeList.size(); ++i) {
        if (activeList[i].filePath == currentFilePath) {
            currentActiveIndex = i;
            break;
        }
    }
    if (currentActiveIndex < 0) { // 找不到 → 从头开始
        currentActiveIndex = 0;
    }
    //随机播放：若当前模式为随机播放，且歌单长度大于1，则随机选择一首歌播放
    if (m_playMode == PlayMode::RandomPlay && activeList.size() > 1) {
        int randomActiveIndex = currentActiveIndex;
        while (randomActiveIndex == currentActiveIndex) {
            randomActiveIndex = QRandomGenerator::global()->bounded(activeList.size());
        }
        const QString targetPath = activeList[randomActiveIndex].filePath;
        for (int i = 0; i < m_playlist.size(); ++i) {// 在总歌单里找到对应歌曲并播放
            if (m_playlist[i].filePath == targetPath) {
                playByIndex(i);
                return;
            }
        }
        return;
    }
    // 正常播放：若当前模式为非随机播放，则按顺序播放上一首。若当前歌曲为第一首，则播放最后一首
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

    const QList<SongInfo> &activeList = (m_playMode == PlayMode::AllLoop)
                                            ? m_playlist
                                            : (!m_groupPlaylist.isEmpty() ? m_groupPlaylist
                                                                          : (!m_folderPlaylist.isEmpty() ? m_folderPlaylist
                                                                                                         : m_playlist));
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
// 跳转到指定播放位置（进度条拖动时调用）：夹紧到 duration 范围内
void PlayerController::seek(qint64 position)
{
    qint64 maxDuration = m_player->duration(); //获取当前歌曲时长
    if (maxDuration < 0) { //如果时长无效（未加载完成），强制设为 0
        maxDuration = 0; //时长为负 → 0
    }
    const qint64 clampedPosition = qBound<qint64>(0, position, maxDuration); //夹紧到 duration 范围内
    m_player->setPosition(clampedPosition);
}
// 设置播放模式（顺序、单曲循环、列表循环、随机等）：若与当前模式相同则返回；否则更新 m_playMode 并 emit playModeChanged
void PlayerController::setPlayMode(PlayMode mode)
{
    if (m_playMode == mode) {
        return;
    }
    m_playMode = mode; //更新播放模式
    emit playModeChanged(m_playMode); //发出播放模式变化信号，让界面更新循环模式按钮图标
}
// 当前曲目索引
int PlayerController::currentIndex() const
{
    return m_currentIndex;
}
// 当前目录索引：若目录表为空或当前索引无效则 -1；否则在目录表里找到当前歌曲的位置
int PlayerController::folderIndex() const
{
    if (m_folderPlaylist.isEmpty()) {
        return -1;
    }
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return -1;
    }

    const QString currentPath = m_playlist[m_currentIndex].filePath;
    for (int i = 0; i < m_folderPlaylist.size(); ++i) {
        if (m_folderPlaylist[i].filePath == currentPath) {
            return i;
        }
    }
    return -1;
}
// 当前艺人索引：若艺人表为空或当前索引无效则 -1；否则在艺人表里找到当前歌曲的位置
int PlayerController::groupIndex() const
{
    if (m_groupPlaylist.isEmpty()) {
        return -1;
    }
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return -1;
    }

    const QString currentPath = m_playlist[m_currentIndex].filePath;    //获取当前正在播放的歌曲路径
    for (int i = 0; i < m_groupPlaylist.size(); ++i) {    //在【分组歌单 m_groupPlaylist】里遍历查找
        if (m_groupPlaylist[i].filePath == currentPath) {
            return i;
        }
    }
    return -1;
}
// 总歌单歌曲数：直接返回 m_playlist.size()
int PlayerController::playlistCount() const
{
    return m_playlist.size();
}
// 当前激活的播放列表歌曲数：根据播放模式决定使用哪个歌单
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
//返回当前播放模式
PlayMode PlayerController::playMode() const
{
    return m_playMode;
}
//返回播放器正在播放 / 暂停 / 停止的状态
QMediaPlayer::PlaybackState PlayerController::playbackState() const
{
    return m_player->playbackState();
}
//开始向前seek：设置 m_seekDirection = 1，启动定时器
void PlayerController::startSeekForward()
{
    m_seekDirection = 1;
    if (!m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}
//停止向前seek：若 m_seekDirection = 1，则设置 m_seekDirection = 0，停止定时器
void PlayerController::stopSeekForward()
{
    if (m_seekDirection == 1) {
        m_seekDirection = 0;
        m_seekTimer->stop();
    }
}
//开始向后seek：设置 m_seekDirection = -1，启动定时器
void PlayerController::startSeekBackward()
{
    m_seekDirection = -1;
    if (!m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}
//停止向后seek：若 m_seekDirection = -1，则设置 m_seekDirection = 0，停止定时器
void PlayerController::stopSeekBackward()
{
    if (m_seekDirection == -1) {
        m_seekDirection = 0;
        m_seekTimer->stop();
    }
}
// 根据【总歌单索引】播放指定歌曲
void PlayerController::playByIndex(int index)
{
    if (index < 0 || index >= m_playlist.size()) {
        return;
    }

    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);// 通知UI：当前歌曲索引变了
    emit songChanged(m_playlist[m_currentIndex]);// 通知UI：当前歌曲变了
    //设置媒体源：把当前歌曲路径转换为 QUrl，并设置给播放器
    const QUrl source = QUrl::fromLocalFile(m_playlist[m_currentIndex].filePath);
    m_player->setSource(source);
    m_player->play();   //播放
    loadLrc(m_playlist[m_currentIndex]);//加载歌词
}
// 加载歌词：若歌词文件路径为空，则清空歌词并返回；否则解析歌词文件，生成时间轴映射，并 emit lrcLoaded
void PlayerController::loadLrc(const SongInfo &song)
{   //若歌词文件路径为空，则清空歌词并返回
    if (song.lrcPath.isEmpty()) {
        emit lrcLoaded(QMap<qint64, QString>());
        return;
    }
    //解析歌词文件，生成时间轴映射。调用解析器，解析 .lrc 文件
    const QMap<qint64, QString> result = LrcParser::parse(song.lrcPath);
    emit lrcLoaded(result);    //发送解析好的歌词给 UI 显示
}
// 从播放器读取歌曲元数据（标题、歌手、专辑、封面），更新到歌曲信息并通知界面
void PlayerController::updateSongMetaData()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return;
    }
    //获取当前正在播放的歌曲（引用，直接修改原数据）
    SongInfo &song = m_playlist[m_currentIndex];
    const QMediaMetaData metaData = m_player->metaData();//从播放器获取媒体元数据
    
    const QString title = metaData.value(QMediaMetaData::Title).toString().trimmed();//获取标题
    const QVariant artistValue = metaData.value(QMediaMetaData::ContributingArtist);
    QString artist;// 读取 歌手（兼容多种格式）
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
    // 读取 专辑封面（优先缩略图，再封面大图）
    QPixmap albumArt;
    const QVariant thumbnailValue = metaData.value(QMediaMetaData::ThumbnailImage);
    if (thumbnailValue.canConvert<QPixmap>()) {
        albumArt = qvariant_cast<QPixmap>(thumbnailValue);
    } else if (thumbnailValue.canConvert<QImage>()) {
        albumArt = QPixmap::fromImage(qvariant_cast<QImage>(thumbnailValue));
    }
    // 缩略图为空，再读封面图
    if (albumArt.isNull()) {
        const QVariant coverValue = metaData.value(QMediaMetaData::CoverArtImage);
        if (coverValue.canConvert<QPixmap>()) {
            albumArt = qvariant_cast<QPixmap>(coverValue);
        } else if (coverValue.canConvert<QImage>()) {
            albumArt = QPixmap::fromImage(qvariant_cast<QImage>(coverValue));
        }
    }
    // 发送信号，通知 UI 全部更新
    emit albumArtChanged(albumArt);
    emit songChanged(song);
    emit playlistMetaUpdated(m_currentIndex, song);
}

int PlayerController::randomIndexExcludingCurrent() const
{
    if (m_playlist.isEmpty()) {
        return -1;
    }
    if (m_playlist.size() == 1) {//如果歌单里只有1首歌 → 只能返回 0
        return 0;
    }
    // 随机选择一首歌，排除当前歌曲
    int index = m_currentIndex; //初始化 index 为当前索引（目的：强制进入循环）
    while (index == m_currentIndex) {
        index = QRandomGenerator::global()->bounded(m_playlist.size()); //随机生成一个索引，排除当前歌曲
    }
    return index;
}
// 当一首歌【完全播放完毕】时自动调用
void PlayerController::handleEndOfMedia()
{
    if (m_playlist.isEmpty() || m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return;
    }
    // 根据播放模式，决定下一首歌的处理方式
    switch (m_playMode) {
    case PlayMode::SingleLoop:
        m_player->setPosition(0); //单曲循环：从头开始播放，进度回到 0 毫秒
        m_player->play();
        break;
    case PlayMode::FolderLoop:
    case PlayMode::AllLoop:
        next();
        break;
    case PlayMode::RandomPlay://随机播放：播放完 → 自动下一曲（next内部会随机）
        next();
        break;
    }
}
