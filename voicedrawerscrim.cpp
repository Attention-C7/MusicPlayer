#include "voicedrawerscrim.h"

#include <QPainter>
#include <QPaintEvent>

namespace {

/** 与节拍页类似的缩小再放大近似高斯，控制开销。 */
QPixmap makeBlurredPixmap(const QPixmap &source)
{
    if (source.isNull() || source.width() < 8 || source.height() < 8) {
        return QPixmap();
    }

    const QSize target = source.size();
    const int maxSide = qMax(target.width(), target.height());
    const int divisor = qBound(8, maxSide / 20, 36);
    const int wSmall = qMax(24, target.width() / divisor);
    const int hSmall = qMax(24, target.height() / divisor);
    const QPixmap tiny = source.scaled(wSmall, hSmall, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return tiny.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

VoiceDrawerScrimWidget::VoiceDrawerScrimWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setVisible(false);
    setFocusPolicy(Qt::NoFocus);
}

void VoiceDrawerScrimWidget::clearBackdrop()
{
    m_blur = QPixmap();
    update();
}

void VoiceDrawerScrimWidget::setBackdropBlurred(const QPixmap &pixmap)
{
    m_blur = makeBlurredPixmap(pixmap);
    update();
}

void VoiceDrawerScrimWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_blur.isNull()) {
        painter.drawPixmap(rect(), m_blur, m_blur.rect());
    }

    QLinearGradient veil(0, 0, 0, height());
    veil.setColorAt(0.0, QColor(10, 10, 22, 175));
    veil.setColorAt(0.55, QColor(18, 18, 34, 200));
    veil.setColorAt(1.0, QColor(26, 26, 46, 220));
    painter.fillRect(rect(), veil);
}
