#pragma once

#include <QPixmap>
#include <QVariantAnimation>
#include <QWidget>

class QEvent;

/**
 * 唱片机风格封面：外圈沟槽与中心圆形封面；播放时封面匀速旋转，暂停/停止时静止。
 * 唱臂：播放时压在盘面上方，暂停/停止时经动画旋至盘面外。
 */
class TurntableAlbumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TurntableAlbumWidget(QWidget *parent = nullptr);

    void setAlbumPixmap(const QPixmap &pixmap);
    void setPlaying(bool playing);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onRotationValueChanged(const QVariant &value);
    void onTonearmAngleChanged(const QVariant &value);

private:
    void animateTonearmTo(qreal targetDeg);

    QPixmap m_albumPixmap;
    QVariantAnimation *m_rotationAnim;
    QVariantAnimation *m_tonearmAnim;
    qreal m_rotationDeg;
    qreal m_tonearmDeg;
};
