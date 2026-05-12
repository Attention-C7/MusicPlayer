#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <QtGlobal>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPixmap;
class QPropertyAnimation;
class QPushButton;
class QResizeEvent;
class QSequentialAnimationGroup;
class QShowEvent;
class QTimer;
class QHideEvent;

class BeatDetector;
class PlayWidget;

/**
 * 全屏节拍 + 歌词：白幕叠层与 PlayWidget 同公式（0→峰值→0，峰值/时长随强度）；
 * 另含背景波纹、水印字、歌词阴影等氛围绘制。
 */
class BeatLyricWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float lyricAlpha READ lyricAlpha WRITE setLyricAlpha)
    Q_PROPERTY(float overlayAlpha READ overlayAlpha WRITE setOverlayAlpha)

public:
    explicit BeatLyricWidget(QWidget *parent = nullptr);
    ~BeatLyricWidget() override;

    /** 用封面主色更新暖色渐变背景；可多次调用。 */
    void setBackgroundCover(const QPixmap &cover);
    /** 接入节拍信号 beatDetected → onBeat（QueuedConnection）。 */
    void setAudioController(BeatDetector *detector);
    /** 接入 PlayWidget::lyricCurrentLineChanged → onLyricLineChanged。 */
    void setLyricController(PlayWidget *lyricSource);

public slots:
    void onBeat(float intensity);
    void onLyricLineChanged(int lineIndex, const QString &text);
    void closeWidget();

private slots:
    void onCloseButtonClicked();
    void onBpmUpdated(float bpm);
    void onBreathTimeout();

signals:
    /** 用户按 Esc、点右上角关闭键后发出；可与 deleteLater 组合释放本窗口。 */
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    float lyricAlpha() const;
    void setLyricAlpha(float alpha);
    float overlayAlpha() const;
    void setOverlayAlpha(float alpha);

    void updateWarmGradientFromCover(const QPixmap &cover);

    void applyBeatFlash(float intensity);
    void scheduleBreathAfterSilence();

    QString m_line1;
    QString m_line2;
    float m_lyricAlpha = 0.0f;
    float m_overlayAlpha = 0.0f;
    QPropertyAnimation *m_lyricAnim = nullptr;
    QSequentialAnimationGroup *m_beatFlashGroup = nullptr;
    QPropertyAnimation *m_beatFlashRise = nullptr;
    QPropertyAnimation *m_beatFlashFall = nullptr;
    QTimer *m_breathTimer = nullptr;
    float m_currentBpm = 120.0f;
    bool m_inBreathTimeout = false;
    int m_currentIndex = 0;

    QColor m_gradTop;
    QColor m_gradBottom;
    QString m_watermarkText;

    QMetaObject::Connection m_beatConn;
    QMetaObject::Connection m_bpmConn;
    QMetaObject::Connection m_lyricConn;

    QPushButton *m_closeButton = nullptr;
};
