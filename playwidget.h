#pragma once

#include <QGraphicsBlurEffect>
#include <QLabel>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

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
    void updateLrcDisplay(qint64 position);
    void buildLrcLabels();
    void clearLrcLabels();
    void updateBackground(const QPixmap &pixmap);
    void updateIndexLabel();

    Ui::PlayWidget *ui;
    QLabel *m_bgLabel;
    QMap<qint64, QString> m_lrcMap;
    int m_currentLrcIndex;
    QList<QLabel*> m_lrcLabels;
    PlayerController *m_controller;
    bool m_isDragging;
    QTimer *m_longPressTimer;
    int m_pressDirection;
    bool m_longPressTriggered;
};
