#pragma once

#include <QLabel>
#include <QMap>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include "aicontroller.h"
#include "beatanalyzer.h"
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
    void updateBrightnessByPosition(qint64 position);
    float smoothBrightness(float target);
    int overlayAlphaFromBrightness(float brightness) const;

    Ui::PlayWidget *ui;
    QPixmap m_bgPixmap;
    QMap<qint64, QString> m_lrcMap;
    int m_currentLrcIndex;
    QList<QLabel*> m_lrcLabels;
    PlayerController *m_controller;
    AiController *m_aiController;
    BeatAnalyzer *m_beatAnalyzer;
    VoiceInputWidget *m_voiceWidget;
    QPushButton *m_btnBeatLight;
    float m_brightnessNow;
    float m_brightnessBase;
    float m_brightnessGain;
    bool m_beatEffectEnabled;
    bool m_isDragging;
    QTimer *m_longPressTimer;
    int m_pressDirection;
    bool m_longPressTriggered;
};
