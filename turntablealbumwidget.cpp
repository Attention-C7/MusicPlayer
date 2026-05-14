#include "turntablealbumwidget.h"

#include <QEasingCurve>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QHideEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QtMath>

namespace {

constexpr qreal kTwoPi = 6.28318530717958647692;

constexpr qreal kMsPerRevolution = 14000.0;

/** 暂停：0° 臂身为竖直向下；播放：顺时针略转使针尖落在外圈沟槽。 */
constexpr qreal kTonearmParkedDeg = 0.0;
constexpr qreal kTonearmOnRecordDeg = 14.0;
constexpr int kTonearmAnimMs = 280;

constexpr qreal kArmLengthFactor = 0.62;

/** 支点水平：相对「圆心 + platterR」向左缩进（相对布局短边）。 */
constexpr qreal kPivotLeftOfCircleRightByLayout = 0.028;

/** 支点垂直：相对布局短边，负值为圆心上侧。 */
constexpr qreal kPivotUpFromCenterByLayout = 0.34;

/** 光晕底层模糊处理边长（较小以减轻嵌入式 CPU / 内存）。 */
constexpr int kGlowBlurProcessSide = 132;

/** 光晕相对封面直径的放大倍率（略大于转盘，形成氛围光）。 */
constexpr qreal kGlowDiameterFactor = 1.78;

/** 缓慢相位循环，形成「流动」阴影/光晕位移（非 UI 瞬时过渡）。 */
constexpr int kGlowFlowCycleMs = 2800;

/**
 * 与全屏节拍垫图类似的轻量模糊：缩小再放大近似高斯，输出正方形光晕素材。
 */
QPixmap makeBlurredSquareGlow(const QPixmap &source, int sideOut)
{
    if (source.isNull() || sideOut < 32) {
        return QPixmap();
    }

    const QSize target(sideOut, sideOut);
    QPixmap scaled = source.scaled(
        target.width(),
        target.height(),
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation);
    if (scaled.isNull()) {
        return QPixmap();
    }

    const int cx = qMax(0, (scaled.width() - target.width()) / 2);
    const int cy = qMax(0, (scaled.height() - target.height()) / 2);
    const QPixmap cropped = scaled.copy(cx, cy, target.width(), target.height());

    const int maxSide = qMax(target.width(), target.height());
    const int divisor = qBound(8, maxSide / 18, 36);
    const int wSmall = qMax(24, target.width() / divisor);
    const int hSmall = qMax(24, target.height() / divisor);
    const QPixmap tiny = cropped.scaled(wSmall, hSmall, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return tiny.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

TurntableAlbumWidget::TurntableAlbumWidget(QWidget *parent)
    : QWidget(parent)
    , m_rotationAnim(new QVariantAnimation(this))
    , m_tonearmAnim(new QVariantAnimation(this))
    , m_glowFlowAnim(new QVariantAnimation(this))
    , m_rotationDeg(0.0)
    , m_tonearmDeg(kTonearmParkedDeg)
    , m_glowFlowT(0.0)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);

    m_rotationAnim->setDuration(static_cast<int>(kMsPerRevolution));
    m_rotationAnim->setStartValue(0.0);
    m_rotationAnim->setEndValue(360.0);
    m_rotationAnim->setLoopCount(-1);
    connect(m_rotationAnim, &QVariantAnimation::valueChanged,
            this, &TurntableAlbumWidget::onRotationValueChanged);

    m_tonearmAnim->setDuration(kTonearmAnimMs);
    m_tonearmAnim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_tonearmAnim, &QVariantAnimation::valueChanged,
            this, &TurntableAlbumWidget::onTonearmAngleChanged);

    m_glowFlowAnim->setDuration(kGlowFlowCycleMs);
    m_glowFlowAnim->setStartValue(0.0);
    m_glowFlowAnim->setEndValue(1.0);
    m_glowFlowAnim->setLoopCount(-1);
    m_glowFlowAnim->setEasingCurve(QEasingCurve::Linear);
    connect(m_glowFlowAnim, &QVariantAnimation::valueChanged,
            this, &TurntableAlbumWidget::onGlowFlowValueChanged);
}

void TurntableAlbumWidget::rebuildGlowBackdrop()
{
    if (m_albumPixmap.isNull()) {
        m_glowBackdrop = QPixmap();
        return;
    }
    m_glowBackdrop = makeBlurredSquareGlow(m_albumPixmap, kGlowBlurProcessSide);
}

void TurntableAlbumWidget::setAlbumPixmap(const QPixmap &pixmap)
{
    m_albumPixmap = pixmap;
    rebuildGlowBackdrop();
    if (m_glowBackdrop.isNull()) {
        m_glowFlowAnim->stop();
    } else if (isVisible()) {
        if (m_glowFlowAnim->state() != QAbstractAnimation::Running) {
            m_glowFlowAnim->start();
        }
    }
    update();
}

void TurntableAlbumWidget::setPlatterSpinning(bool spinning)
{
    if (spinning) {
        if (m_rotationAnim->state() == QAbstractAnimation::Paused) {
            m_rotationAnim->resume();
        } else if (m_rotationAnim->state() != QAbstractAnimation::Running) {
            m_rotationAnim->start();
        }
    } else {
        if (m_rotationAnim->state() == QAbstractAnimation::Running) {
            m_rotationAnim->pause();
        }
    }
}

void TurntableAlbumWidget::setTonearmOnRecord(bool onRecord)
{
    animateTonearmTo(onRecord ? kTonearmOnRecordDeg : kTonearmParkedDeg);
}

void TurntableAlbumWidget::onRotationValueChanged(const QVariant &value)
{
    m_rotationDeg = static_cast<qreal>(value.toDouble());
    update();
}

void TurntableAlbumWidget::onTonearmAngleChanged(const QVariant &value)
{
    m_tonearmDeg = static_cast<qreal>(value.toDouble());
    update();
}

void TurntableAlbumWidget::onGlowFlowValueChanged(const QVariant &value)
{
    m_glowFlowT = static_cast<qreal>(value.toDouble());
    update();
}

void TurntableAlbumWidget::animateTonearmTo(qreal targetDeg)
{
    if (qAbs(m_tonearmDeg - targetDeg) < 0.05) {
        return;
    }
    m_tonearmAnim->stop();
    m_tonearmAnim->setStartValue(m_tonearmDeg);
    m_tonearmAnim->setEndValue(targetDeg);
    m_tonearmAnim->start();
}

void TurntableAlbumWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange) {
        update();
    }
}

void TurntableAlbumWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_glowBackdrop.isNull() && m_glowFlowAnim->state() != QAbstractAnimation::Running) {
        m_glowFlowAnim->start();
    }
}

void TurntableAlbumWidget::hideEvent(QHideEvent *event)
{
    m_glowFlowAnim->stop();
    QWidget::hideEvent(event);
}

void TurntableAlbumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect wr = rect();
    if (wr.width() <= 0 || wr.height() <= 0) {
        return;
    }

    painter.fillRect(wr, Qt::transparent);

    const int side = qMin(wr.width(), wr.height());
    const QPoint center(wr.center());
    const QPoint platterCenter(center);

    const qreal platterR = static_cast<qreal>(side) * 0.42;
    const qreal labelR = platterR * 0.68;
    const qreal artR = labelR - 4.0;
    const qreal glowD = (artR * 2.0) * kGlowDiameterFactor;

    // --- 封面染色光晕（模糊放大副本 + 轻微位移/呼吸，参考 Spotify / Apple Music 类氛围底） ---
    if (!m_glowBackdrop.isNull()) {
        const qreal theta = m_glowFlowT * kTwoPi;
        const qreal ofsX = 5.0 * qSin(theta);
        const qreal ofsY = 4.0 * qCos(theta * 0.83);
        const qreal pulse = 1.0 + 0.065 * qSin(theta * 1.65 + 0.4);
        const qreal glowAlpha = 0.48 + 0.09 * qSin(theta * 1.2);

        painter.save();
        painter.translate(platterCenter);
        painter.translate(ofsX, ofsY);
        painter.scale(pulse, pulse);
        painter.translate(-platterCenter);

        const QRectF glowRect(
            platterCenter.x() - glowD / 2.0,
            platterCenter.y() - glowD / 2.0,
            glowD,
            glowD);

        QPainterPath glowClip;
        glowClip.addEllipse(glowRect);
        painter.setClipPath(glowClip);

        painter.setOpacity(glowAlpha);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPixmap(glowRect.toRect(), m_glowBackdrop, m_glowBackdrop.rect());

        // 毛玻璃感：中心略亮、边缘柔化（主流播放器常见的 frosted / material 叠层）
        painter.setOpacity(qMin(1.0, glowAlpha + 0.12));
        painter.setClipping(false);
        QRadialGradient frost(platterCenter, glowD * 0.52);
        frost.setColorAt(0.0, QColor(255, 255, 255, 38));
        frost.setColorAt(0.5, QColor(255, 255, 255, 12));
        frost.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setCompositionMode(QPainter::CompositionMode_SoftLight);
        painter.setBrush(frost);
        painter.drawEllipse(glowRect);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // 外缘与主背景融合（Multiply 压边，避免椭圆外方角）
        QRadialGradient edgeBlend(platterCenter, glowD * 0.55);
        edgeBlend.setColorAt(0.65, QColor(26, 26, 46, 0));
        edgeBlend.setColorAt(1.0, QColor(26, 26, 46, 88));
        painter.setOpacity(0.75);
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.setBrush(edgeBlend);
        painter.drawEllipse(glowRect);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setOpacity(1.0);
        painter.restore();
    }

    // --- 暗色转盘（与 #1a1a2e 主界面协调，无浅色悬浮底座） ---
    QRadialGradient platterGrad(platterCenter, platterR);
    platterGrad.setColorAt(0.0, QColor("#2e2e44"));
    platterGrad.setColorAt(0.78, QColor("#232336"));
    platterGrad.setColorAt(1.0, QColor("#181824"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(platterGrad);
    painter.drawEllipse(platterCenter, static_cast<int>(platterR), static_cast<int>(platterR));

    const QColor grooveHi("#3d3d52");
    const QColor grooveLo("#2a2a3e");
    const int grooveCount = 28;
    for (int i = 0; i < grooveCount; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(grooveCount);
        const qreal rr = platterR * (0.72 + t * 0.28);
        const QColor c = (i % 2 == 0) ? grooveHi : grooveLo;
        painter.setPen(QPen(c, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(platterCenter, static_cast<int>(rr), static_cast<int>(rr));
    }

    painter.setPen(QPen(QColor("#0a0a12"), 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(platterCenter, static_cast<int>(labelR), static_cast<int>(labelR));

    // --- 中心封面（旋转） ---
    painter.save();
    painter.translate(platterCenter);
    painter.rotate(m_rotationDeg);
    painter.translate(-platterCenter);

    QPainterPath clipDisk;
    clipDisk.addEllipse(platterCenter, static_cast<int>(artR), static_cast<int>(artR));
    painter.setClipPath(clipDisk);

    if (!m_albumPixmap.isNull()) {
        const QPixmap scaled = m_albumPixmap.scaled(
            QSize(static_cast<int>(artR * 2.0), static_cast<int>(artR * 2.0)),
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);
        const QRect artRect(
            platterCenter.x() - scaled.width() / 2,
            platterCenter.y() - scaled.height() / 2,
            scaled.width(),
            scaled.height());
        painter.drawPixmap(artRect.topLeft(), scaled);
    } else {
        QRadialGradient hole(platterCenter, artR);
        hole.setColorAt(0.0, QColor("#2a2a3e"));
        hole.setColorAt(1.0, QColor("#1a1a2e"));
        painter.fillPath(clipDisk, hole);
        painter.setClipping(false);
        QFont f = font();
        if (f.pointSizeF() > 0) {
            f.setPointSizeF(f.pointSizeF() * 2.2);
        } else {
            f.setPixelSize(qMax(22, static_cast<int>(artR * 0.35)));
        }
        painter.setFont(f);
        painter.setPen(QColor("#7e7e92"));
        painter.drawText(QRectF(platterCenter.x() - artR, platterCenter.y() - artR,
                               artR * 2.0, artR * 2.0),
                         Qt::AlignCenter,
                         QStringLiteral("♪"));
    }
    painter.restore();

    const qreal pivotDx = platterR - static_cast<qreal>(side) * kPivotLeftOfCircleRightByLayout;
    const qreal pivotDy = -static_cast<qreal>(side) * kPivotUpFromCenterByLayout;
    const qreal armLen = static_cast<qreal>(side) * kArmLengthFactor;

    painter.save();
    painter.translate(center);
    painter.translate(pivotDx, pivotDy);
    painter.rotate(m_tonearmDeg);

    QPainterPath arm;
    arm.moveTo(0, 0);
    arm.lineTo(0, armLen * 0.88);
    arm.lineTo(-side * 0.028, armLen * 0.93);
    arm.lineTo(-side * 0.038, armLen * 0.98);
    painter.setPen(QPen(QColor("#c8cad4"), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(arm);

    QLinearGradient wGrad(0, -10, 0, 10);
    wGrad.setColorAt(0.0, QColor("#e8e9ef"));
    wGrad.setColorAt(1.0, QColor("#8b90a0"));
    painter.setBrush(wGrad);
    painter.setPen(QPen(QColor("#6a7080"), 1.0));
    painter.drawEllipse(QPointF(0, 0), 10.0, 10.0);
    painter.setBrush(QColor("#d8dae2"));
    painter.drawRoundedRect(QRectF(-15.0, -21.0, 13.0, 26.0), 4.0, 4.0);
    painter.restore();
}
