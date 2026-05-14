#include "playprogressslider.h"

#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

namespace {

constexpr qreal kHandleRadiusNormal = 6.0;
constexpr qreal kHandleRadiusHover = 8.0;
constexpr int kHandleAnimMs = 180;

constexpr int kTrackHeight = 4;
constexpr int kPreferredHeight = 28;

} // namespace

PlayProgressSlider::PlayProgressSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
    , m_handleRadiusAnim(new QVariantAnimation(this))
    , m_handleRadius(kHandleRadiusNormal)
    , m_hovering(false)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kPreferredHeight);
    setMaximumHeight(kPreferredHeight);

    m_handleRadiusAnim->setDuration(kHandleAnimMs);
    m_handleRadiusAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_handleRadiusAnim, &QVariantAnimation::valueChanged,
            this, &PlayProgressSlider::onHandleRadiusAnimValueChanged);
}

void PlayProgressSlider::onHandleRadiusAnimValueChanged(const QVariant &value)
{
    m_handleRadius = static_cast<qreal>(value.toDouble());
    update();
}

void PlayProgressSlider::startHandleRadiusAnim(qreal endRadius)
{
    if (qAbs(m_handleRadius - endRadius) < 0.01) {
        return;
    }
    m_handleRadiusAnim->stop();
    m_handleRadiusAnim->setStartValue(m_handleRadius);
    m_handleRadiusAnim->setEndValue(endRadius);
    m_handleRadiusAnim->start();
}

bool PlayProgressSlider::event(QEvent *event)
{
    const QEvent::Type t = event->type();
    if (t == QEvent::HoverEnter) {
        if (!m_hovering) {
            m_hovering = true;
            startHandleRadiusAnim(kHandleRadiusHover);
        }
    } else if (t == QEvent::HoverLeave) {
        if (m_hovering) {
            m_hovering = false;
            startHandleRadiusAnim(kHandleRadiusNormal);
        }
    }
    return QSlider::event(event);
}

QSize PlayProgressSlider::sizeHint() const
{
    QSize s = QSlider::sizeHint();
    s.setHeight(kPreferredHeight);
    s.setWidth(qMax(s.width(), 160));
    return s;
}

void PlayProgressSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect wr = rect();
    const qreal cy = wr.center().y();
    const qreal trackHalf = static_cast<qreal>(kTrackHeight) / 2.0;
    const qreal y0 = cy - trackHalf;
    const qreal margin = m_handleRadius + 3.0;
    const qreal grooveLeft = margin;
    const qreal grooveRight = static_cast<qreal>(wr.width()) - margin;
    const qreal grooveW = qMax(1.0, grooveRight - grooveLeft);

    const int minV = minimum();
    const int maxV = maximum();
    const qreal span = static_cast<qreal>(qMax(1, maxV - minV));
    const qreal t = static_cast<qreal>(value() - minV) / span;
    const qreal playedW = grooveW * qBound(0.0, t, 1.0);
    const qreal splitX = grooveLeft + playedW;

    const QRectF trackRect(grooveLeft, y0, grooveW, static_cast<qreal>(kTrackHeight));
    const qreal radius = static_cast<qreal>(kTrackHeight) / 2.0;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#333355"));
    painter.drawRoundedRect(trackRect, radius, radius);

    if (playedW > 0.5) {
        const QRectF playedRect(grooveLeft, y0, playedW, static_cast<qreal>(kTrackHeight));
        QLinearGradient orangeGrad(playedRect.left(), playedRect.center().y(),
                                   playedRect.right(), playedRect.center().y());
        orangeGrad.setColorAt(0.0, QColor("#FF7043"));
        orangeGrad.setColorAt(1.0, QColor("#FF8A65"));
        painter.setBrush(orangeGrad);
        painter.drawRoundedRect(playedRect, radius, radius);
    }

    const qreal hx = qBound(grooveLeft, splitX, grooveRight);
    const QPointF handleCenter(hx, cy);

    painter.setPen(QPen(QColor(255, 255, 255, 140), 1.2));
    painter.setBrush(QColor("#FF7043"));
    painter.drawEllipse(handleCenter, m_handleRadius, m_handleRadius);

    if (hasFocus()) {
        painter.setPen(QPen(QColor("#FF7043"), 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(handleCenter, m_handleRadius + 3.0, m_handleRadius + 3.0);
    }
}
