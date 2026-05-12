#pragma once

#include <QColor>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>

#include <QtGlobal>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPropertyAnimation;
class QPushButton;
class QResizeEvent;
class QSequentialAnimationGroup;
class QShowEvent;
class QHideEvent;
class QTimer;

class PlayWidget;
class PlayerController;

/**
 * 全屏节拍 + 歌词：底层大号模糊封面+压暗，再叠主题色半透明渐变、暗角与呼吸波纹；无封面时仅用渐变主题。
 */
class BeatLyricWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float lyricAlpha READ lyricAlpha WRITE setLyricAlpha)
    Q_PROPERTY(float overlayAlpha READ overlayAlpha WRITE setOverlayAlpha)

public:
    explicit BeatLyricWidget(QWidget *parent = nullptr);
    ~BeatLyricWidget() override;

    /** 用封面更新底层模糊垫图与主题渐变/波纹色；无封面时清除垫图。 */
    void setBackgroundCover(const QPixmap &cover);
    /** 接入 PlayerController::beatDetected(float) → onBeat（QueuedConnection）。 */
    void setBeatSource(PlayerController *controller);
    /** 接入 PlayWidget::lyricCurrentLineChanged → onLyricLineChanged。 */
    void setLyricController(PlayWidget *lyricSource);

public slots:
    void onBeat(float intensity);
    void onLyricLineChanged(int lineIndex, const QString &text);
    void closeWidget();

private slots:
    void onCloseButtonClicked();

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
    void rebuildBackdrop();

    void applyBeatFlash(float intensity);

    QString m_line1;
    QString m_line2;
    float m_lyricAlpha = 0.0f;
    float m_overlayAlpha = 0.0f;
    QPropertyAnimation *m_lyricAnim = nullptr;
    QSequentialAnimationGroup *m_beatFlashGroup = nullptr;
    QPropertyAnimation *m_beatFlashRise = nullptr;
    QPropertyAnimation *m_beatFlashFall = nullptr;
    int m_currentIndex = 0;

    QColor m_gradTop;
    QColor m_gradBottom;
    /** 封面平均色，用于波纹描边 tint；无封面时为淡粉 accent。 */
    QColor m_themeColor;

    QPixmap m_sourceCover;
    QPixmap m_coverBackdrop;
    QSize m_backdropSize;

    QMetaObject::Connection m_beatConn;
    QMetaObject::Connection m_lyricConn;

    QPushButton *m_closeButton = nullptr;
    /** 驱动波纹相位，形成水波式缓慢起伏（非完美静态同心圆）。 */
    QTimer *m_waveTimer = nullptr;
    float m_ripplePhase = 0.0f;
};
