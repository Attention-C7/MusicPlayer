#pragma once

#include <QPropertyAnimation>
#include <QWidget>

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
    void showList();
    void hideList();

    Ui::MusicPlayer *ui;
    PlayerController *m_controller;
    PlayWidget *m_playWidget;
    ListWidget *m_listWidget;
    QPropertyAnimation *m_listAnimation;
};
