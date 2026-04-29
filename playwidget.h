#pragma once

#include <QLabel>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "aicontroller.h"
#include "playercontroller.h"
#include "songinfo.h"
#include "voiceinputwidget.h"

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
    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );

signals:
    void showListRequested();

private:
    void paintEvent(QPaintEvent *event) override;
    QString formatTime(qint64 ms) const;
    void setPlayModeIcon(PlayMode mode);
    QPixmap roundedAlbumArt(const QPixmap &pixmap) const;
    void updateLrcDisplay(qint64 position);
    void buildLrcLabels();
    void clearLrcLabels();
    void updateBackground(const QPixmap &pixmap);
    void updateIndexLabel();

    Ui::PlayWidget *ui;
    QPixmap m_bgPixmap;
    QMap<qint64, QString> m_lrcMap;
    int m_currentLrcIndex;
    QList<QLabel*> m_lrcLabels;
    PlayerController *m_controller;
    AiController *m_aiController;
    VoiceInputWidget *m_voiceWidget;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
    bool m_isDragging;
    QTimer *m_longPressTimer;
    int m_pressDirection;
    bool m_longPressTriggered;
};
