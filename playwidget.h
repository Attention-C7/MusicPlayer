#pragma once

#include <QLabel>   //标签，用于显示文本；歌词行等
#include <QMap>  //映射，用于存储键值对：歌词时间戳→文本、艺人/专辑索引
#include <QVector>  //与歌词行同序的时间戳列表，供点击跳转与进度查找
#include <QPixmap>  //像素图，用于显示图像：封面与背景
#include <QTimer>  //定时器，用于实现定时功能：长按检测等
#include <QWidget>  //窗口组件，用于实现窗口管理、事件处理、绘制等功能

class QEvent;
class QResizeEvent;
class QPropertyAnimation;

class QFrame;
class QSlider;
class QPushButton;

//#include "aicontroller.h"  //AI控制器，用于处理语音识别、命令解析等，与 VoiceInputWidget 配合
#include <QMediaPlayer>  //PlaybackState，槽参数类型需完整枚举

#include "playercontroller.h"  //播放控制器，用于控制播放器：播放、暂停、切换、seek等
#include "songinfo.h"  //歌曲信息，用于存储歌曲元数据：标题、艺人、专辑、时长等。单曲元数据；setSearchContext 用 QList<SongInfo> 等。
#include "voiceinputwidget.h"  //语音输入组件，用于处理语音输入：语音识别、命令解析等。语音输入 UI 成员指针类型需要完整类声明。

QT_BEGIN_NAMESPACE  //前向声明 Ui::PlayWidget。真正的定义在生成的 ui_playwidget.h 里。
                   // 这样 playwidget.h 不必 #include "ui_playwidget.h"，
                  // 减少编译依赖；实现文件里再包含生成头并完成 setupUi。
namespace Ui {
class PlayWidget;
}
QT_END_NAMESPACE

class AiController;
class LyricLineRow;

class PlayWidget : public QWidget   //PlayWidget 继承自 QWidget，作为可布局、可绘制的整块播放界面。
{
    Q_OBJECT  //Q_OBJECT 宏声明 PlayWidget 类为 Qt 对象，自动生成信号和槽机制。本类有 signals，必须由 moc 处理。
public:
    //构造：必须传入 PlayerController *（谁播放、谁发 songChanged/positionChanged 由它负责）；parent 交给 QWidget。
    explicit PlayWidget(PlayerController *controller, AiController *aiController, QWidget *parent = nullptr);
    ~PlayWidget();  //析构：头文件未标 override，实现里应释放 ui 等（与常见 Qt 写法一致）。
    void setSearchContext(  //把当前曲库快照（全量列表、按艺人/按专辑分组）从列表侧栏同步过来，供语音/AI 在「当前扫描结果」里检索；
        QList<SongInfo> allSongs,  //签名与 ListWidget::searchContextUpdated 一侧发出的数据对齐（在 musicplayer.cpp 里连到 PlayWidget::setSearchContext）。
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );

    VoiceInputWidget *voiceInputWidget() const { return m_voiceWidget; }  //供 MusicPlayer 连接语音「按路径播放」到 ListWidget::playFromPath

signals:
    void showListRequested();   //用户点「列表」等操作时由 PlayWidget emit，MusicPlayer 收到后执行 showList()，从而 播放页不直接操作 ListWidget 几何，只发「请求」，降低与主窗口布局的耦合。
    /** 当前高亮歌词行变化（毫秒时间轴索引 + 文本）；供 BeatLyricWidget 等订阅。 */
    void lyricCurrentLineChanged(int lineIndex, const QString &text);

private slots:
    void onVolumeButtonClicked();
    void onVolumeSliderValueChanged(int value);
    void onVolumeSliderReleased();
    void onVolumeMuteButtonClicked();
    void onControllerVolumePercentChanged(int percent);
    void hideVolumePopupIfOpen();
    void onControllerPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onSessionPlaybackActiveChanged(bool active);
    void onBeatButtonClicked();

private:
    void paintEvent(QPaintEvent *event) override;   //自定义绘制：模糊背景图、圆角封面等（节拍闪光仅在 BeatLyricWidget 全屏）。
    QString formatTime(qint64 ms) const;   //时间格式化：ms 转 mm:ss。与项目「时长用 qint64 毫秒」一致。
    void setPlayModeIcon(PlayMode mode);   //播放模式图标切换：根据 songinfo.h 里的 PlayMode 更新循环模式按钮图标。
    void updateLrcDisplay(qint64 position);   //歌词行滚动：传入当前播放时间，更新 m_lrcRows 中对应行的高亮。
    void buildLrcLabels();
    void clearLrcLabels();
    void updateBackground(const QPixmap &pixmap);  //背景图模糊处理：传入封面 QPixmap，生成模糊背景图，供绘制使用。
    void updateIndexLabel();  //索引标签更新：根据播放状态（playing/paused）更新索引标签文本。
    void setBeatEnabled(bool enabled);  //节奏按钮高亮风格开关（主界面不叠节拍闪光；闪光仅在全屏节奏界面）。
    void setupVolumePopup();  //音量浮层：垂直滑条 + 百分比 + 静音，与 PlayerController 同步
    void repositionVolumePopup();  //锚定在音量按钮上方
    void refreshVolumeButtonIcon();  //根据音量/静音刷新工具栏喇叭图标
    void showToast(const QString &message, int displayMs = 2500);  //短时提示（无歌词等）
    /** 节拍按钮：根据是否有歌词更新提示文案、高亮（可进入节拍歌词全屏时暖色描边）。 */
    void updateBeatLyricButtonState();

    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    Ui::PlayWidget *ui;  //Designer 生成控件树（按钮、滑条、scrollArea_lrc 等）。
    PlayerController *m_controller;  //播放控制器，由构造传入，与 playwidget.cpp 初始化列表一致。
    QPixmap m_bgPixmap;  //模糊背景图，用于绘制时叠加。
    QMap<qint64, QString> m_lrcMap;  //歌词时间戳→文本映射，与 m_lrcRows 联动。
    QVector<qint64> m_lrcTimesMs;   //buildLrcLabels 填充，与 m_lrcRows 同序；updateLrcDisplay 用它二分，单调播放时先判断区间再 upper_bound。
    int m_currentLrcIndex;  //当前高亮歌词行索引，避免每帧全表扫描（具体逻辑在 .cpp）。
    QList<LyricLineRow *> m_lrcRows;  //歌词行控件：悬停左侧显示播放与时间，点击跳转。
    AiController *m_aiController;  //AI 相关逻辑，构造里 new AiController(this)，父对象为 PlayWidget。
    VoiceInputWidget *m_voiceWidget;  //语音输入条；构造里再 new，并塞进 ui->verticalLayout_main。
    QList<SongInfo> m_allSongs;  //与 setSearchContext 参数对应的缓存，供语音部件检索。
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
    bool m_beatEffect;  //节奏按钮样式；与全屏 BeatLyricWidget 节拍闪光无关。
    QPixmap m_lastAlbumCover;
    bool m_isDragging;  //拖拽状态，决定是否启动长按检测。
    QTimer *m_longPressTimer;  //长按定时器，每 500ms 调用一次 onLongPress()。
    int m_pressDirection;  //长按方向，决定是否启动长按检测。
    bool m_longPressTriggered;  //长按触发，决定是否启动长按检测。
    QFrame *m_volumePopup;  //音量调节浮层父控件
    QSlider *m_sliderVolume;  //垂直音量条 0–100
    QLabel *m_lblVolumePercent;  //例如 50%
    QPushButton *m_btnVolumeMute;  //浮层底部静音切换
    QPropertyAnimation *m_lrcScrollAnim;  //歌词区垂直滚动平滑动画（隐藏滚动条时仍操作 QScrollBar）
};
