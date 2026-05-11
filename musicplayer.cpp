#include "musicplayer.h"
#include "./ui_musicplayer.h"
#include "voiceinputwidget.h"
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QDir>

MusicPlayer::MusicPlayer(QWidget *parent)   //构造函数，初始化UI对象、播放控制器、播放界面、列表界面、列表动画
    : QWidget(parent)  //父对象是parent，QWidget是所有窗口组件的基类，提供窗口管理、事件处理、绘制等功能
    , ui(new Ui::MusicPlayer)  //创建.ui文件对应的UI对象，并初始化其子控件
    , m_controller(nullptr)  //播放控制器
    , m_playWidget(nullptr)  //播放界面
    , m_listWidget(nullptr)  //列表界面
    , m_listAnimation(nullptr)  //列表动画
    , m_aiController(nullptr)
{   
    ui->setupUi(this);  //在this上创建.ui文件对应的UI对象，并初始化其子控件

    setFixedSize(800, 500);  //固定主窗尺寸

    m_controller = new PlayerController(this);  //创建播放控制器，控制器父对象是主窗口，窗口销毁时子对象一并释放
    m_aiController = new AiController(m_controller, this);  //须在 PlayWidget 之前创建：VoiceInputWidget 依赖同一 AiController 实例
    m_playWidget = new PlayWidget(m_controller, m_aiController, this);  //创建播放界面，接收m_controller的控制信号，并显示播放信息
    m_listWidget = new ListWidget(m_controller, this);  //创建列表界面，接收m_controller的控制信号，并显示列表信息
    m_listAnimation = new QPropertyAnimation(m_listWidget, "pos", this);  //创建列表动画，对列表控件pos属性进行动画，父为this便于管理生命周期
    
    m_playWidget->setGeometry(0, 0, width(), height());  //设置播放界面位置和大小，占满主窗
    m_listWidget->setGeometry(width(), 0, 360, height());   //设置列表界面位置和大小，从主窗右侧开始，宽度360px，高度与主窗相同
    m_listWidget->hide();   //初始隐藏列表界面

    //m_listWidget->setRootPath(QStringLiteral("/Music"));
    m_listWidget->setRootPath(QDir::homePath() + QStringLiteral("/Music"));  //设置列表界面根路径为当前用户音乐目录

    //连接播放界面和列表界面的信号与槽，当播放界面请求显示列表时，调用showList槽函数，当列表界面请求返回播放界面时，调用hideList槽函数
    connect(m_playWidget, &PlayWidget::showListRequested, this, &MusicPlayer::showList);
    connect(m_listWidget, &ListWidget::backToPlayerRequested, this, &MusicPlayer::hideList);
    //connect(m_listWidget, &ListWidget::searchContextUpdated, m_playWidget, &PlayWidget::setSearchContext);
    /*connect(
        m_playWidget->voiceInputWidget(),
        &VoiceInputWidget::playRequested,
        m_listWidget,
        &ListWidget::playFromPath);*/
    connect(m_listWidget, &ListWidget::searchContextUpdated, m_aiController,&AiController::setSearchContext);
    connect(m_aiController->dispatcher(), &CommandDispatcher::showListRequested, this, &MusicPlayer::showList);
    connect(m_aiController->dispatcher(), &CommandDispatcher::backToPlayerRequested, this, &MusicPlayer::hideList);
    VoiceInputWidget *voiceWidget = m_playWidget->voiceInputWidget();
    connect(m_aiController->dispatcher(), &CommandDispatcher::dispatchResult, voiceWidget, &VoiceInputWidget::onDispatchResult);
    connect(m_aiController, &AiController::recognizing, voiceWidget, &VoiceInputWidget::onRecognizing);
    connect(m_aiController, &AiController::recognizeFailed, voiceWidget, &VoiceInputWidget::onRecognizeFailed);
}

MusicPlayer::~MusicPlayer()
{
    delete ui;  //释放UI对象,.ui 生成的界面堆在 ui 里，需手动释放
}

void MusicPlayer::showList()  //显示列表界面
{
    m_listAnimation->stop();    //停止当前动画，避免连续点“列表”时状态打架
    m_listAnimation->setDuration(250);  //设置动画持续时间250ms
    m_listAnimation->setEasingCurve(QEasingCurve::OutCubic);  //设置动画缓动曲线为OutCubic  
    m_listAnimation->setStartValue(QPoint(800, 0));  //设置动画起始位置，从主窗右侧开始
    m_listAnimation->setEndValue(QPoint(440, 0));  //设置动画结束位置，从主窗左侧开始
    m_listWidget->show();  //显示列表界面
    m_listWidget->raise();  //将列表界面置于顶层
    m_listAnimation->start();  //开始动画
}

void MusicPlayer::hideList()  //隐藏列表界面
{
    m_listAnimation->stop();
    m_listAnimation->setDuration(200);  //设置动画持续时间200ms
    m_listAnimation->setEasingCurve(QEasingCurve::OutCubic);  //设置动画缓动曲线为OutCubic
    m_listAnimation->setStartValue(QPoint(440, 0));  //设置动画起始位置，从主窗左侧开始
    m_listAnimation->setEndValue(QPoint(800, 0));  //设置动画结束位置，从主窗右侧开始

    disconnect(m_listAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_listAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_listWidget->pos().x() >= 800) {
            m_listWidget->hide();
        }
    });
    m_listAnimation->start();
}
