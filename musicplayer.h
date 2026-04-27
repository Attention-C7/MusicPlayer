#pragma once

#include <QWidget>
#include <QStackedWidget>

#include "listwidget.h"
#include "playercontroller.h"
#include "playwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MusicPlayer;
}
QT_END_NAMESPACE

class MusicPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer() override;

private:
    Ui::MusicPlayer *ui;
    QStackedWidget *m_stack;
    PlayerController *m_controller;
    PlayWidget *m_playWidget;
    ListWidget *m_listWidget;
};
