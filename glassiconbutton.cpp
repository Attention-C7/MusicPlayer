#include "glassiconbutton.h"

#include <QEasingCurve>
#include <QIODevice>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QFile>
#include <QMouseEvent>
#include <QSvgRenderer>
#include <QVariantAnimation>
#include <QtMath>

namespace {

constexpr int kSecondaryPx = 40;
constexpr int kMainPlayPx = 56;
constexpr int kGlyphSecondary = 24;
constexpr int kGlyphMain = 28;
constexpr int kAccentDotW = 4;
constexpr int kAccentDotH = 2;
constexpr int kClickAnimMs = 100;

} // namespace

GlassIconButton::GlassIconButton(QWidget *parent)
    : QPushButton(parent)
    , m_glyphTint(QColor(224, 224, 232))
    , m_accentActive(false)
    , m_beatChrome(0)
    , m_beatVisual(false)
    , m_role(ChipRole::Secondary40)
    , m_clickScale(1.0)
    , m_pressed(false)
    , m_clickAnim(new QVariantAnimation(this))
{
    setFlat(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setChipRole(ChipRole::Secondary40);

    m_clickAnim->setDuration(kClickAnimMs);
    m_clickAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_clickAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_clickScale = v.toReal();
        update();
    });
    connect(m_clickAnim, &QVariantAnimation::finished, this, &GlassIconButton::onClickAnimFinished);
}

void GlassIconButton::setChipRole(ChipRole role)
{
    m_role = role;
    if (role == ChipRole::MainPlay56) {
        setFixedSize(kMainPlayPx, kMainPlayPx);
    } else {
        setFixedSize(kSecondaryPx, kSecondaryPx);
    }
    rebuildGlyphPixmap();
}

void GlassIconButton::setSvgResourcePath(const QString &qrcPath)
{
    m_svgPath = qrcPath;
    rebuildGlyphPixmap();
}

void GlassIconButton::setGlyphTint(const QColor &color)
{
    m_glyphTint = color;
    rebuildGlyphPixmap();
}

void GlassIconButton::setAccentActive(bool active)
{
    if (m_accentActive == active) {
        return;
    }
    m_accentActive = active;
    rebuildGlyphPixmap();
    update();
}

void GlassIconButton::setBeatChrome(int level)
{
    if (m_beatChrome == level) {
        return;
    }
    m_beatChrome = level;
    update();
}

void GlassIconButton::setBeatVisualEnabled(bool enabled)
{
    if (m_beatVisual == enabled) {
        return;
    }
    m_beatVisual = enabled;
    update();
}

void GlassIconButton::setClickScale(qreal scale)
{
    m_clickScale = scale;
    update();
}

void GlassIconButton::onClickAnimFinished()
{
    m_clickScale = 1.0;
    update();
}

void GlassIconButton::rebuildGlyphPixmap()
{
    m_glyphPixmap = QPixmap();
    if (m_svgPath.isEmpty()) {
        return;
    }
    QFile file(m_svgPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray data = file.readAll();
    const QColor drawColor = m_accentActive ? QColor(QStringLiteral("#FF7043")) : m_glyphTint;
    data.replace("CURRENT_COLOR", drawColor.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return;
    }
    const int glyph = (m_role == ChipRole::MainPlay56) ? kGlyphMain : kGlyphSecondary;
    const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
    const int side = static_cast<int>(qCeil(static_cast<qreal>(glyph) * dpr));
    QPixmap pm(side, side);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(0, 0, glyph, glyph));
    m_glyphPixmap = pm;
}

QRect GlassIconButton::iconDrawRect() const
{
    const int glyph = (m_role == ChipRole::MainPlay56) ? kGlyphMain : kGlyphSecondary;
    const QRect r = rect();
    const int x = r.center().x() - glyph / 2;
    const int y = r.center().y() - glyph / 2;
    return QRect(x, y, glyph, glyph);
}

void GlassIconButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const bool hover = underMouse();

    if (m_role == ChipRole::MainPlay56) {
        QColor base(255, 255, 255, 38);
        if (hover || m_pressed) {
            base.setAlpha(m_pressed ? 50 : 48);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(base);
        painter.drawEllipse(r.adjusted(1, 1, -1, -1));
    } else {
        if (hover || m_pressed) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, m_pressed ? 22 : 26));
            painter.drawEllipse(r.adjusted(1, 1, -1, -1));
        }
    }

    if (m_beatVisual) {
        if (m_beatChrome == 2) {
            painter.setPen(QPen(QColor(255, 255, 255, 140), 1.0));
            painter.setBrush(QColor(255, 255, 255, 40));
            painter.drawEllipse(r.adjusted(2, 2, -2, -2));
        } else if (m_beatChrome == 1) {
            painter.setPen(QPen(QColor(255, 190, 100, 200), 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(r.adjusted(2, 2, -2, -2));
        } else {
            painter.setPen(QPen(QColor(255, 255, 255, 55), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(r.adjusted(2, 2, -2, -2));
        }
    }

    if (!m_glyphPixmap.isNull()) {
        const QRect ir = iconDrawRect();
        painter.save();
        painter.translate(ir.center());
        painter.scale(m_clickScale, m_clickScale);
        painter.translate(-ir.center());
        const QRect drawTarget(ir.left(), ir.top(),
                               static_cast<int>(m_glyphPixmap.width() / m_glyphPixmap.devicePixelRatio()),
                               static_cast<int>(m_glyphPixmap.height() / m_glyphPixmap.devicePixelRatio()));
        painter.drawPixmap(drawTarget.topLeft(), m_glyphPixmap);
        painter.restore();
    }

    if (m_accentActive) {
        const int dotY = r.bottom() - 5;
        const int dotX = r.center().x() - kAccentDotW / 2;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#FF7043")));
        painter.drawRoundedRect(QRect(dotX, dotY, kAccentDotW, kAccentDotH), 1.0, 1.0);
    }

    if (hasFocus()) {
        painter.setPen(QPen(QColor(255, 255, 255, 80), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(r.adjusted(3, 3, -3, -3));
    }
}

void GlassIconButton::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        rebuildGlyphPixmap();
    }
}

void GlassIconButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_clickAnim->stop();
        m_clickScale = 0.9;
        update();
    }
    QPushButton::mousePressEvent(event);
}

void GlassIconButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        m_clickAnim->stop();
        m_clickAnim->setStartValue(0.9);
        m_clickAnim->setEndValue(1.0);
        m_clickAnim->start();
    }
    QPushButton::mouseReleaseEvent(event);
}

void GlassIconButton::leaveEvent(QEvent *event)
{
    if (m_pressed) {
        m_pressed = false;
        m_clickAnim->stop();
        m_clickAnim->setStartValue(m_clickScale);
        m_clickAnim->setEndValue(1.0);
        m_clickAnim->start();
    }
    QPushButton::leaveEvent(event);
}
