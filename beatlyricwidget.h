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
class QHideEvent;

class PlayWidget;
class PlayerController;

/**
 * 全屏节拍 + 歌词：背景渐变取专辑封面主题色（无封面时为淡粉）；同心波纹随歌词渐入（lyricAlpha）扩散、显隐；
 * 白幕叠层与 PlayWidget 一致；强拍 intensity ≥ 0.6 触发闪烁。
 */
class BeatLyricWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float lyricAlpha READ lyricAlpha WRITE setLyricAlpha)
    Q_PROPERTY(float overlayAlpha READ overlayAlpha WRITE setOverlayAlpha)

public:
    explicit BeatLyricWidget(QWidget *parent = nullptr);
    ~BeatLyricWidget() override;

    /** 用封面缩略平均色更新背景渐变与波纹主题色；无封面时为淡粉默认。 */
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

    QMetaObject::Connection m_beatConn;
    QMetaObject::Connection m_lyricConn;

    QPushButton *m_closeButton = nullptr;
};
