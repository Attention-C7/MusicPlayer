#pragma once

#include <QPropertyAnimation>
#include <QWidget>

#include "listwidget.h"
#include "playercontroller.h"
#include "playwidget.h"

QT_BEGIN_NAMESPACE  //前向声明 Ui::MusicPlayer。真正的定义在生成的 ui_musicplayer.h 里。
                   // 这样 musicplayer.h 不必 #include "ui_musicplayer.h"，
                  // 避免每个包含 musicplayer.h 的编译单元都依赖 .ui 生成物、缩短编译链
namespace Ui {
class MusicPlayer;
}
QT_END_NAMESPACE

class MusicPlayer : public QWidget  //MusicPlayer 继承自 QWidget，提供窗口管理、事件处理、绘制等功能
{
    Q_OBJECT  //Q_OBJECT 宏声明 MusicPlayer 类为 Qt 对象，自动生成信号和槽机制

public:
    explicit MusicPlayer(QWidget *parent = nullptr);    //explicit 防止隐式转换，QWidget *parent = nullptr 默认父窗口为空
    ~MusicPlayer() override;  //override 确保重写父类虚函数，防止编译器警告，override 是 C++11 引入的关键字，用于显式声明重写父类虚函数

private:
    void showList();  //显示列表界面
    void hideList();  //隐藏列表界面

    Ui::MusicPlayer *ui;    //Designer 生成界面的持有者，setupUi 在 .cpp 里调用。
    PlayerController *m_controller;  //播放与业务中枢
    PlayWidget *m_playWidget;  //播放界面
    ListWidget *m_listWidget;  //列表界面
    QPropertyAnimation *m_listAnimation;  //列表动画
    AiController *m_aiController;  //AI控制器
    VoiceInputWidget *m_voiceWidget;  //语音输入界面
};
