#pragma once

#include <QPixmap>
#include <QShowEvent>
#include <QHideEvent>
#include <QVariantAnimation>
#include <QWidget>

class QEvent;

/**
 * 唱片机风格封面：外圈沟槽与中心圆形封面；封面下为模糊放大光晕（参考主流播放器「随封面染色 + 柔光氛围」）。
 * 转盘旋转与唱臂摆动拆开：前者紧跟解码播放状态；后者应对「用户是否在播」语义（避免切歌时短暂 Stopped 误摆臂）。
 */
class TurntableAlbumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TurntableAlbumWidget(QWidget *parent = nullptr);

    void setAlbumPixmap(const QPixmap &pixmap);
    /** 仅控制唱片盘面旋转动画（Playing 时转，否则停）。 */
    void setPlatterSpinning(bool spinning);
    /** 仅控制唱臂：true 压在沟槽侧，false 竖直暂停位。 */
    void setTonearmOnRecord(bool onRecord);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onRotationValueChanged(const QVariant &value);
    void onTonearmAngleChanged(const QVariant &value);
    void onGlowFlowValueChanged(const QVariant &value);

private:
    void animateTonearmTo(qreal targetDeg);
    void rebuildGlowBackdrop();

    QPixmap m_albumPixmap;
    QPixmap m_glowBackdrop;
    QVariantAnimation *m_rotationAnim;
    QVariantAnimation *m_tonearmAnim;
    QVariantAnimation *m_glowFlowAnim;
    qreal m_rotationDeg;
    qreal m_tonearmDeg;
    qreal m_glowFlowT;
};
