#include "micoverlaywidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>

namespace {

constexpr int kBarCount = 5;
constexpr int kTickMs = 50;

} // namespace

MicOverlayWidget::MicOverlayWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_listening(false)
    , m_phase(0.0)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    for (int i = 0; i < kBarCount; ++i) {
        m_levels[i] = 0.35;
    }
    m_timer->setInterval(kTickMs);
    m_timer->setObjectName(QStringLiteral("MicOverlayWaveTimer"));
    connect(m_timer, &QTimer::timeout, this, &MicOverlayWidget::onTick);
}

void MicOverlayWidget::setListening(bool listening)
{
    if (m_listening == listening) {
        return;
    }
    m_listening = listening;
    if (listening) {
        m_phase = 0.0;
        m_timer->start();
    } else {
        m_timer->stop();
    }
    update();
}

void MicOverlayWidget::onTick()
{
    m_phase += 0.22;
    for (int i = 0; i < kBarCount; ++i) {
        const qreal wobble = 0.45 + 0.55 * qSin(m_phase + static_cast<qreal>(i) * 0.9);
        const qreal noise = static_cast<qreal>(QRandomGenerator::global()->bounded(0, 40)) / 100.0;
        m_levels[i] = qBound(0.12, wobble * 0.55 + noise * 0.25, 1.0);
    }
    update();
}

void MicOverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (!m_listening) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(26, 26, 46, 175));

    const QPoint c = rect().center();
    const qreal micR = 22.0;
    painter.setPen(QPen(QColor("#FF7043"), 2.5));
    painter.setBrush(QColor(255, 112, 67, 40));
    painter.drawEllipse(QPointF(c), micR, micR);

    painter.setPen(QPen(QColor("#ffffff"), 2.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(c), micR * 0.45, micR * 0.45);

    const qreal barW = 5.0;
    const qreal gap = 7.0;
    const qreal baseY = c.y() + micR + 28.0;
    const qreal maxBarH = 42.0;
    const qreal totalW = kBarCount * barW + (kBarCount - 1) * gap;
    qreal x = c.x() - totalW / 2.0;

    for (int i = 0; i < kBarCount; ++i) {
        const qreal h = maxBarH * m_levels[i];
        const QRectF bar(x, baseY - h, barW, h);
        QLinearGradient g(bar.topLeft(), bar.bottomLeft());
        g.setColorAt(0.0, QColor("#FF8A65"));
        g.setColorAt(1.0, QColor("#FF7043"));
        painter.setPen(Qt::NoPen);
        painter.setBrush(g);
        painter.drawRoundedRect(bar, 2.0, 2.0);
        x += barW + gap;
    }
}
