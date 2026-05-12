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
class QShowEvent;

class BeatDetector;
class PlayWidget;

/**
 * 全屏节拍 + 歌词：参考「动效歌词」思路用节拍驱动短时视觉反馈（径向闪白 + 轻暖色脉冲），
 * 非 QQ 音乐 ASS 序列帧方案；背景波纹、大字水印、歌词阴影贴近常见手电筒/氛围歌词样式。
 */
class BeatLyricWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float lyricAlpha READ lyricAlpha WRITE setLyricAlpha)
    Q_PROPERTY(float overlayAlpha READ overlayAlpha WRITE setOverlayAlpha)
    /** 节拍「手电筒」闪白层：径向高光，峰值约 0.55，短衰减。 */
    Q_PROPERTY(float flashAlpha READ flashAlpha WRITE setFlashAlpha)

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
    void onBeat();
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

private:
    float lyricAlpha() const;
    void setLyricAlpha(float alpha);
    float overlayAlpha() const;
    void setOverlayAlpha(float alpha);
    float flashAlpha() const;
    void setFlashAlpha(float alpha);

    void updateWarmGradientFromCover(const QPixmap &cover);

    QString m_line1;
    QString m_line2;
    float m_lyricAlpha = 0.0f;
    float m_overlayAlpha = 0.0f;
    float m_flashAlpha = 0.0f;
    QPropertyAnimation *m_lyricAnim = nullptr;
    QPropertyAnimation *m_beatAnim = nullptr;
    QPropertyAnimation *m_flashAnim = nullptr;
    int m_currentIndex = 0;

    QColor m_gradTop;
    QColor m_gradBottom;
    QString m_watermarkText;

    QMetaObject::Connection m_beatConn;
    QMetaObject::Connection m_lyricConn;

    QPushButton *m_closeButton = nullptr;
};
