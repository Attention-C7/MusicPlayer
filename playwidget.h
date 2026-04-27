#pragma once

#include <QPixmap>
#include <QWidget>
#include <QTimer>

#include "playercontroller.h"
#include "songinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PlayWidget;
}
QT_END_NAMESPACE

class PlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlayWidget(PlayerController *controller, QWidget *parent = nullptr);
    ~PlayWidget();

signals:
    void showListRequested();

private:
    QString formatTime(qint64 ms) const;
    QString playModeText(PlayMode mode) const;
    QPixmap roundedAlbumArt(const QPixmap &pixmap) const;
    void updateIndexLabel();

    Ui::PlayWidget *ui;
    PlayerController *m_controller;
    bool m_isDragging;
    QTimer *m_longPressTimer;
    int m_pressDirection;
    bool m_longPressTriggered;
};
