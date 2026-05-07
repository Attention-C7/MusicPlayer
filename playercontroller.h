#pragma once

#include <QObject>  //控制器是非界面QObject，挂对象树、发信号、内部连QMediaPlayer
#include <QAudioOutput> //Qt6 播放链路里 QMediaPlayer + QAudioOutput，头文件里成员指针需要完整类型（或前向声明 + 实现文件包含）
#include <QList> //歌曲列表，QList<SongInfo> 与 ListWidget 共享数据模型。
#include <QMediaPlayer> //Qt多媒体框架里的核心类，负责媒体文件的播放、暂停、停止、seek等操作。
#include <QPixmap> //像素图，用于显示图像：封面与背景
#include <QTimer> //定时器，用于实现定时功能：seek定时、长按检测等

#include "lrcparser.h" //歌词解析器，用于解析歌词文件，生成歌词时间戳→文本映射。
#include "songinfo.h" //歌曲信息，用于存储歌曲元数据：标题、艺人、专辑、时长等。单曲元数据；setSearchContext 用 QList<SongInfo> 等。

class PlayerController : public QObject //不做绘制，专门封装「播放状态机 + 多媒体后端」
{
    Q_OBJECT //Q_OBJECT 宏声明 PlayerController 类为 Qt 对象，自动生成信号和槽机制。本类有 signals 和 Q_PROPERTY，必须由 moc 处理。

public:
    explicit PlayerController(QObject *parent = nullptr); //构造：必须传入 QObject *parent = nullptr；通常由 MusicPlayer 以 this 为 parent 创建,父窗口销毁时子对象一并释放。

    void setPlaylist(QList<SongInfo> songs); //主播放列表（全库扫描结果等）；当前曲目索引 m_currentIndex 指向这张表
    void setFolderPlaylist(QList<SongInfo> songs); //当前目录（单选目录）播放列表；当前曲目索引 folderIndex 指向这张表
    void setGroupPlaylist(QList<SongInfo> songs); //当前艺人（单选艺人）播放列表；当前曲目索引 groupIndex 指向这张表
    void playSong(int index); //播放指定索引的歌曲
    void playPause(); //播放/暂停
    void prev(); //上一曲
    void next(); //下一曲
    void seek(qint64 position); //seek 到指定位置（毫秒）,毫秒 定位；实现里会按 duration 夹紧
    void setPlayMode(PlayMode mode); //设置播放模式,设置 SingleLoop / FolderLoop / AllLoop / RandomPlay，变化时 emit playModeChanged
    int currentIndex() const; //当前曲目索引
    int folderIndex() const; //当前目录索引
    int groupIndex() const; //当前艺人索引
    int playlistCount() const; //主播放列表歌曲数，m_playlist.size()
    int activePlaylistCount() const; //当前激活的播放列表歌曲数
    PlayMode playMode() const; //当前播放模式
    QMediaPlayer::PlaybackState playbackState() const; //当前播放状态
    void startSeekForward(); //开始向前seek,长按快进/快退：内部 QTimer 周期性 seek（步长在 private 常量里）
    void stopSeekForward(); //停止向前seek
    void startSeekBackward(); //开始向后seek
    void stopSeekBackward(); //停止向后seek

signals:
    void songChanged(SongInfo info);    //换歌；PlayWidget / ListWidget 更新标题、高亮等
    void positionChanged(qint64 position); //进度变化；PlayWidget 更新进度条。毫秒；驱动进度条与时间标签
    void durationChanged(qint64 duration); //时长变化；PlayWidget 更新总时长
    void playbackStateChanged(QMediaPlayer::PlaybackState state); //播放状态变化；播放/暂停/停止 → 更新播放按钮图标等
    void playModeChanged(PlayMode mode); //播放模式变化；PlayWidget 更新循环模式按钮图标，ListWidget 里切换 Tab 刷新等
    void currentIndexChanged(int index); //当前曲目索引变化；PlayWidget 更新当前曲目标签，ListWidget 里高亮等
    void errorOccurred(QString message); //播放错误；弹窗提示用户
    void albumArtChanged(QPixmap pixmap); //专辑封面变化；PlayWidget 更新封面
    void lrcLoaded(QMap<qint64, QString> lyrics); //歌词加载完成；PlayWidget 更新歌词。歌词时间轴（毫秒 → 文本），PlayWidget 滚动歌词
    void playlistMetaUpdated(int index, SongInfo info); //播放列表元数据更新；ListWidget 里更新专辑、艺人等信息

private:
    static constexpr int SeekIntervalMs = 100;  //长按 seek 定时器间隔
    static constexpr qint64 SeekStepMs = 3000;  //长按 seek 步长,每次步进毫秒数

    void playByIndex(int index); //统一入口：设置媒体源、更新索引、发 songChanged、加载歌词等
    void loadLrc(const SongInfo &song); //加载歌词文件，生成时间轴映射
    void updateSongMetaData(); //从 QMediaPlayer 元数据刷新 SongInfo（与扫描阶段 TagLib 互补）
    int randomIndexExcludingCurrent() const; //随机索引，排除当前曲目
    void handleEndOfMedia(); //播放结束：单曲循环/列表循环/随机播放

    QMediaPlayer *m_player; //Qt多媒体框架里的核心类，负责媒体文件的播放、暂停、停止、seek等操作。
    QAudioOutput *m_audioOutput; //Qt6 播放链路里 QMediaPlayer + QAudioOutput，头文件里成员指针需要完整类型（或前向声明 + 实现文件包含）
    QList<SongInfo> m_playlist; //主播放列表（全库扫描结果等）；当前曲目索引 m_currentIndex 指向这张表
    QList<SongInfo> m_folderPlaylist; //当前目录（单选目录）播放列表；当前曲目索引 folderIndex 指向这张表
    QList<SongInfo> m_groupPlaylist; //当前艺人（单选艺人）播放列表；当前曲目索引 groupIndex 指向这张表
    int m_currentIndex; //当前曲目索引，指向 m_playlist / m_folderPlaylist / m_groupPlaylist 之一
    PlayMode m_playMode; //当前播放模式
    QTimer *m_seekTimer; //长按 seek 定时器
    int m_seekDirection; //长按 seek 方向，1 向前，-1 向后
};
