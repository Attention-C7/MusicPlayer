#pragma once

#include <QLabel>   //标签，用于显示文本；歌词行等
#include <QMap>  //映射，用于存储键值对：歌词时间戳→文本、艺人/专辑索引
#include <QPixmap>  //像素图，用于显示图像：封面与背景
#include <QPropertyAnimation>  //属性动画，用于实现动画效果：节拍脉冲
#include <QTimer>  //定时器，用于实现定时功能：节拍定时、长按检测等
#include <QWidget>  //窗口组件，用于实现窗口管理、事件处理、绘制等功能

class QResizeEvent;

class QFrame;
class QSlider;
class QPushButton;

//#include "aicontroller.h"  //AI控制器，用于处理语音识别、命令解析等，与 VoiceInputWidget 配合
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

class PlayWidget : public QWidget   //PlayWidget 继承自 QWidget，作为可布局、可绘制的整块播放界面。
{
    Q_OBJECT  //Q_OBJECT 宏声明 PlayWidget 类为 Qt 对象，自动生成信号和槽机制。本类有 signals 和 Q_PROPERTY，必须由 moc 处理。
    Q_PROPERTY(float overlayAlpha READ overlayAlpha WRITE setOverlayAlpha)  //Q_PROPERTY 宏声明 overlayAlpha 属性，用于实现节拍脉冲动画。
    //让普通变量变成 Qt 能动画的属性，没有它，自定义动画根本做不了！
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

private slots:
    void onVolumeButtonClicked();
    void onVolumeSliderValueChanged(int value);
    void onVolumeSliderReleased();
    void onVolumeMuteButtonClicked();
    void onControllerVolumePercentChanged(int percent);

private:
    void paintEvent(QPaintEvent *event) override;   //自定义绘制：模糊背景图、圆角封面、overlayAlpha 叠层等。
    QString formatTime(qint64 ms) const;   //时间格式化：ms 转 mm:ss。与项目「时长用 qint64 毫秒」一致。
    void setPlayModeIcon(PlayMode mode);   //播放模式图标切换：根据 songinfo.h 里的 PlayMode 更新循环模式按钮图标。
    QPixmap roundedAlbumArt(const QPixmap &pixmap) const;   //封面圆角处理：把封面 QPixmap 做成圆角，供标签或绘制使用。
    void updateLrcDisplay(qint64 position);   //歌词行滚动：传入当前播放时间，更新 m_lrcLabels 中对应行的颜色。
    void buildLrcLabels();
    void clearLrcLabels();
    void updateBackground(const QPixmap &pixmap);  //背景图模糊处理：传入封面 QPixmap，生成模糊背景图，供绘制使用。
    void updateIndexLabel();  //索引标签更新：根据播放状态（playing/paused）更新索引标签文本。
    void startBeatEffect();  //节拍效果启动：如果节拍效果已启用，启动节拍定时器。
    void stopBeatEffect();  //节拍效果停止：停止节拍定时器，重置 overlayAlpha 为 0。
    void onBeat();  //节拍定时：每 500ms 调用一次，更新 overlayAlpha 为 0.15f，触发动画。
    void setBeatEnabled(bool enabled);  //节拍效果开关：根据 enabled 更新按钮样式，并决定是否启动节拍定时器。
    float overlayAlpha() const;  //获取 overlayAlpha 属性值。与 Q_PROPERTY 配套的 READ/WRITE，供动画和绘制读取。
    void setOverlayAlpha(float alpha);  //设置 overlayAlpha 属性值，并触发更新。
    void setupVolumePopup();  //音量浮层：垂直滑条 + 百分比 + 静音，与 PlayerController 同步
    void repositionVolumePopup();  //锚定在音量按钮上方
    void refreshVolumeButtonIcon();  //根据音量/静音刷新工具栏喇叭图标

    void resizeEvent(QResizeEvent *event) override;

    Ui::PlayWidget *ui;  //Designer 生成控件树（按钮、滑条、scrollArea_lrc 等）。
    PlayerController *m_controller;  //播放控制器，由构造传入，与 playwidget.cpp 初始化列表一致。
    QPixmap m_bgPixmap;  //模糊背景图，用于绘制时叠加。
    QMap<qint64, QString> m_lrcMap;  //歌词时间戳→文本映射，与 m_lrcLabels 联动。
    int m_currentLrcIndex;  //当前高亮歌词行索引，避免每帧全表扫描（具体逻辑在 .cpp）。
    QList<QLabel*> m_lrcLabels;  //歌滚动歌词区里多行 QLabel*，动态增删。
    AiController *m_aiController;  //AI 相关逻辑，构造里 new AiController(this)，父对象为 PlayWidget。
    VoiceInputWidget *m_voiceWidget;  //语音输入条；构造里再 new，并塞进 ui->verticalLayout_main。
    QList<SongInfo> m_allSongs;  //与 setSearchContext 参数对应的缓存，供语音部件检索。
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
    QTimer *m_beatTimer;    //周期性触发节拍 UI（与 onBeat 连接）。
    bool m_beatEffect;  //节拍效果开关，决定是否启动节拍定时器。
    float m_overlayAlpha;  //overlayAlpha 属性值，用于动画和绘制。与 Q_PROPERTY 绑定的叠层透明度存储。
    QPropertyAnimation *m_beatAnim;  //节拍动画，用于实现 overlayAlpha 动画效果。
    bool m_isDragging;  //拖拽状态，决定是否启动长按检测。
    QTimer *m_longPressTimer;  //长按定时器，每 500ms 调用一次 onLongPress()。
    int m_pressDirection;  //长按方向，决定是否启动长按检测。
    bool m_longPressTriggered;  //长按触发，决定是否启动长按检测。
    QFrame *m_volumePopup;  //音量调节浮层父控件
    QSlider *m_sliderVolume;  //垂直音量条 0–100
    QLabel *m_lblVolumePercent;  //例如 50%
    QPushButton *m_btnVolumeMute;  //浮层底部静音切换
};
