#pragma once

#include <QtGlobal>

#include <QObject>  //控制器是非界面QObject，挂对象树、发信号、内部连QMediaPlayer
#include <QAudioOutput> //Qt6 播放链路里 QMediaPlayer + QAudioOutput，头文件里成员指针需要完整类型（或前向声明 + 实现文件包含）
#include <QList> //歌曲列表，QList<SongInfo> 与 ListWidget 共享数据模型。
#include <QMediaPlayer> //Qt多媒体框架里的核心类，负责媒体文件的播放、暂停、停止、seek等操作。
#include <QPixmap> //像素图，用于显示图像：封面与背景
#include <QTimer> //定时器，用于实现定时功能：seek定时、长按检测等

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QAudioBufferOutput> //Qt 6.8+：从 QMediaPlayer 取解码 PCM（FFmpeg 后端）
#endif

#include "beatdetector.h" //BeatDetector：QAudioBufferOutput（Qt6.8+）喂 PCM，aubio_tempo 节拍跟踪
#include "lrcparser.h" //歌词解析器，用于解析歌词文件，生成歌词时间戳→文本映射。
#include "songinfo.h" //歌曲信息，用于存储歌曲元数据：标题、艺人、专辑、时长等。单曲元数据；setSearchContext 用 QList<SongInfo> 等。

class PlayerController : public QObject //不做绘制，专门封装「播放状态机 + 多媒体后端」
{
    Q_OBJECT //Q_OBJECT 宏声明 PlayerController 类为 Qt 对象，自动生成信号和槽机制。本类有 signals 和 Q_PROPERTY，必须由 moc 处理。

public:
    explicit PlayerController(QObject *parent = nullptr); //构造：必须传入 QObject *parent = nullptr；通常由 MusicPlayer 以 this 为 parent 创建,父窗口销毁时子对象一并释放。

    void setPlaylist(QList<SongInfo> songs); //替换全量表 m_playlist；并同步 m_ctx 为「全库范围」（scopeList = m_playlist），保证仅调用 setPlaylist 时 prev/next 仍有合法范围
    void setContext(PlayContext ctx);        //原子替换播放上下文（范围 + scope/global 索引）；不替换 m_playlist，依赖调用方已 setPlaylist
    PlayContext currentContext() const;      //当前 m_ctx 快照（含 scopeList 副本）

    void playSong(int index); //播放指定索引的歌曲
    void playPause(); //界面按钮：按当前 QMediaPlayer 状态切换播放/暂停
    /** 语音/指令明确「播放」：只 resume，不与暂停共用切换语义 */
    void requestPlay();
    /** 语音/指令明确「暂停」 */
    void requestPause();

    /** 会话层是否在「播放意图」中（切歌时不因短暂 Stopped 抖动）；界面唱臂/按钮图标宜跟此状态 */
    bool sessionPlaybackActive() const;
    void prev(); //上一曲
    void next(); //下一曲
    void seek(qint64 position); //seek 到指定位置（毫秒）,毫秒 定位；实现里会按 duration 夹紧
    qint64 playbackPositionMs() const; //当前播放进度（毫秒），供语音 seek 相对跳转
    int volumePercent() const; //输出音量 0–100，对应 QAudioOutput 线性音量
    void setVolumePercent(int percent); //设置音量 0–100（非零会解除静音）
    void adjustVolumePercent(int delta); //在 Current 上增减，自动夹紧到 0–100
    bool isMuted() const; //界面静音状态（与音量为 0 不同：语音设 0 不一定视为静音）
    void setMuted(bool muted); //静音：输出 0 并记忆上次音量；取消则恢复
    int volumePercentBeforeMute() const; //静音前记忆的还原音量（用于取消静音前听力提示）
    /** 节拍检测器指针（构造末尾创建）；Qt 6.8+ 时另有 PCM 缓冲输出接入 feedBuffer。节拍 UI 请用 beatDetected(float) 信号。 */
    BeatDetector *beatDetector() const;
    /** 当前曲目最近一次 loadLrc 解析结果（无文件或解析失败为空映射）。 */
    const QMap<qint64, QString> &currentLyrics() const { return m_currentLyrics; }
    void setPlayMode(PlayMode mode); //设置播放模式,设置 SingleLoop / FolderLoop / AllLoop / RandomPlay，变化时 emit playModeChanged
    int currentIndex() const; //当前曲目在全量表中的索引（与 m_ctx.globalIndex 一致，由 playByIndex / setContext 维护）
    int currentScopeIndex() const; //当前曲目在 m_ctx.scopeList 中的索引（界面「第几首」）

    int playlistCount() const; //主播放列表歌曲数，m_playlist.size()
    int activePlaylistCount() const; //当前范围歌曲数：m_ctx.scopeList.size()（不再按 PlayMode 分叉）

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
    void playbackStateChanged(QMediaPlayer::PlaybackState state); //QMediaPlayer 原始状态；转盘旋转等跟解码是否正在跑
    void sessionPlaybackActiveChanged(bool active); //会话播放意图变化；跟 UI 唱臂、暂停按钮图标
    void playModeChanged(PlayMode mode); //播放模式变化；PlayWidget 更新循环模式按钮图标，ListWidget 里切换 Tab 刷新等
    void currentIndexChanged(int index); //当前曲目索引变化；PlayWidget 更新当前曲目标签，ListWidget 里高亮等
    void errorOccurred(QString message); //播放错误；弹窗提示用户
    void albumArtChanged(QPixmap pixmap); //专辑封面变化；PlayWidget 更新封面
    void lrcLoaded(QMap<qint64, QString> lyrics); //歌词加载完成；PlayWidget 更新歌词。歌词时间轴（毫秒 → 文本），PlayWidget 滚动歌词
    void playlistMetaUpdated(int index, SongInfo info); //播放列表元数据更新；ListWidget 里更新专辑、艺人等信息
    void volumePercentChanged(int percent); //音量或静音变化；触控滑条与语音指令共用一路径时同步 UI
    /** 与 BeatDetector::beatDetected 一致；构造时已从内部检测器转发，UI 请连接本信号而非直连 BeatDetector。 */
    void beatDetected(float intensity);

private:
    static constexpr int SeekIntervalMs = 100;  //长按 seek 定时器间隔
    static constexpr qint64 SeekStepMs = 3000;  //长按 seek 步长,每次步进毫秒数

    void playByIndex(int index); //统一入口：设置媒体源、更新索引、发 songChanged、加载歌词等
    void navigate(int delta);    //在 m_ctx.scopeList 内顺序或随机步进（RandomPlay），再映射到全量索引并 playByIndex
    void loadLrc(const SongInfo &song); //加载歌词文件，生成时间轴映射
    void updateSongMetaData(); //从 QMediaPlayer 元数据刷新 SongInfo（与扫描阶段 TagLib 互补）
    void handleEndOfMedia(); //播放结束：单曲循环/列表循环/随机播放
    void emitSessionPlaybackActiveIfChanged(bool active);

    QMediaPlayer *m_player; //Qt多媒体框架里的核心类，负责媒体文件的播放、暂停、停止、seek等操作。
    QAudioOutput *m_audioOutput; //Qt6 播放链路里 QMediaPlayer + QAudioOutput，头文件里成员指针需要完整类型（或前向声明 + 实现文件包含）
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    /** Qt 6.8+：挂到 QMediaPlayer::setAudioBufferOutput，收解码 PCM（audioBufferReceived）。 */
    QAudioBufferOutput *m_audioBufferOutput = nullptr;
#endif
    /** RMS 节拍检测；在构造函数末尾 new，父对象为 this。 */
    BeatDetector *m_beatDetector = nullptr;
    QMap<qint64, QString> m_currentLyrics;
    QList<SongInfo> m_playlist; //全量播放列表；唯一媒体索引 m_currentIndex / m_ctx.globalIndex 指向此表
    PlayContext m_ctx;           //当前播放上下文（范围列表 + 范围内下标 + 全量下标）
    int m_currentIndex;          //当前曲目在全量表中的索引（冗余自 m_ctx.globalIndex，与 durationChanged 等lambda捕获一致）
    PlayMode m_playMode; //当前播放模式
    QTimer *m_seekTimer; //长按 seek 定时器
    int m_seekDirection; //长按 seek 方向，1 向前，-1 向后
    bool m_muted; //触控静音键打开时为 true；setVolumePercent(>0) 会清除
    int m_volumePercentBeforeMute; //静音前音量，用于取消静音；默认 50
    bool m_sessionPlaybackActive = false; //用户/指令意图：处于播放会话（切歌不改变 false）
};
