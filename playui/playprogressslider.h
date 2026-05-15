#pragma once

#include <QSlider>
#include <QVariantAnimation>

/**
 * 播放进度条：4px 圆角轨道、已播段橙色渐变、12px 实心圆点滑块，悬停平滑放大至 16px。
 */
class PlayProgressSlider : public QSlider
{
    Q_OBJECT

public:
    explicit PlayProgressSlider(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    bool event(QEvent *event) override;

private slots:
    void onHandleRadiusAnimValueChanged(const QVariant &value);

private:
    void startHandleRadiusAnim(qreal endRadius);

    QVariantAnimation *m_handleRadiusAnim;
    qreal m_handleRadius;
    bool m_hovering;
};
