#include "turntablealbumwidget.h"

#include <QEasingCurve>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

constexpr int kShadowMargin = 6;
constexpr int kBaseCornerRadius = 24;

constexpr qreal kMsPerRevolution = 14000.0;

/** 暂停：0° 臂沿 +Y 竖直向下，针尖在沟槽外；播放略转使针尖落在外圈金属区。 */
constexpr qreal kTonearmParkedDeg = 0.0;
constexpr qreal kTonearmOnRecordDeg = 13.0;
constexpr int kTonearmAnimMs = 280;

constexpr qreal kArmLengthFactor = 0.58;

/** 支点相对底座短边：水平在唱片圆最右端内侧；竖直在圆心上侧（底座右上角区域）。 */
constexpr qreal kPivotLeftOfCircleRightByBase = 0.026;
constexpr qreal kPivotUpFromCenterByBase = 0.32;

} // namespace

TurntableAlbumWidget::TurntableAlbumWidget(QWidget *parent)
    : QWidget(parent)
    , m_rotationAnim(new QVariantAnimation(this))
    , m_tonearmAnim(new QVariantAnimation(this))
    , m_rotationDeg(0.0)
    , m_tonearmDeg(kTonearmParkedDeg)
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
}

void TurntableAlbumWidget::setAlbumPixmap(const QPixmap &pixmap)
{
    m_albumPixmap = pixmap;
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

    // --- 纯白圆角底座（QQ 音乐类）+ 柔和投影 ---
    const QRect baseRect(
        center.x() - side / 2 + kShadowMargin,
        center.y() - side / 2 + kShadowMargin,
        side - 2 * kShadowMargin,
        side - 2 * kShadowMargin);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 38));
    const QRect shadowR = baseRect.adjusted(5, 8, 9, 12);
    painter.drawRoundedRect(shadowR, kBaseCornerRadius + 3, kBaseCornerRadius + 3);

    QLinearGradient baseGrad(baseRect.topLeft(), baseRect.bottomLeft());
    baseGrad.setColorAt(0.0, QColor("#fefefe"));
    baseGrad.setColorAt(0.55, QColor("#f7f7f5"));
    baseGrad.setColorAt(1.0, QColor("#f0f0ed"));
    painter.setBrush(baseGrad);
    painter.setPen(QPen(QColor("#e6e6e2"), 1.0));
    painter.drawRoundedRect(baseRect, kBaseCornerRadius, kBaseCornerRadius);

    // 极轻竖向纹理，避免塑料感（静态、无「波动」动画）
    painter.setPen(QColor(0, 0, 0, 5));
    for (int x = baseRect.left() + 8; x < baseRect.right() - 8; x += 6) {
        painter.drawLine(x, baseRect.top() + 10, x, baseRect.bottom() - 10);
    }

    const QPoint platterCenter(baseRect.center());
    const qreal platterR = qMin(baseRect.width(), baseRect.height()) * 0.42;

    // --- 外圈金属拉丝感（同心细环 + 径向高光） ---
    QRadialGradient platterGrad(platterCenter, platterR);
    platterGrad.setColorAt(0.0, QColor("#e4e6eb"));
    platterGrad.setColorAt(0.72, QColor("#c9ced6"));
    platterGrad.setColorAt(0.92, QColor("#b4bac5"));
    platterGrad.setColorAt(1.0, QColor("#9aa3b0"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(platterGrad);
    painter.drawEllipse(platterCenter, static_cast<int>(platterR), static_cast<int>(platterR));

    const QColor grooveHi("#d8dce3");
    const QColor grooveLo("#bfc6d0");
    const int grooveCount = 32;
    for (int i = 0; i < grooveCount; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(grooveCount);
        const qreal rr = platterR * (0.72 + t * 0.28);
        const QColor c = (i % 2 == 0) ? grooveHi : grooveLo;
        painter.setPen(QPen(c, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(platterCenter, static_cast<int>(rr), static_cast<int>(rr));
    }

    // 唱片内缘深色环（分隔金属区与封面）
    const qreal labelR = platterR * 0.68;
    painter.setPen(QPen(QColor("#2c2c32"), 2.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(platterCenter, static_cast<int>(labelR), static_cast<int>(labelR));

    const qreal artR = labelR - 4.0;

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
        hole.setColorAt(0.0, QColor("#f5f5f7"));
        hole.setColorAt(1.0, QColor("#dcdce2"));
        painter.fillPath(clipDisk, hole);
        painter.setClipping(false);
        QFont f = font();
        if (f.pointSizeF() > 0) {
            f.setPointSizeF(f.pointSizeF() * 2.0);
        } else {
            f.setPixelSize(qMax(20, static_cast<int>(artR * 0.32)));
        }
        painter.setFont(f);
        painter.setPen(QColor("#a0a0b0"));
        painter.drawText(QRectF(platterCenter.x() - artR, platterCenter.y() - artR,
                               artR * 2.0, artR * 2.0),
                         Qt::AlignCenter,
                         QStringLiteral("♪"));
    }
    painter.restore();

    // --- 唱臂：白支点 + 银配重 + 金属臂 + 白头壳 + 针尖（与底座/金属圈统一） ---
    const int baseDim = qMin(baseRect.width(), baseRect.height());
    const qreal pivotDx = platterR - static_cast<qreal>(baseDim) * kPivotLeftOfCircleRightByBase;
    const qreal pivotDy = -static_cast<qreal>(baseDim) * kPivotUpFromCenterByBase;
    const qreal armLen = static_cast<qreal>(side) * kArmLengthFactor;
    const qreal s = static_cast<qreal>(side);

    painter.save();
    painter.translate(center);
    painter.translate(pivotDx, pivotDy);
    painter.rotate(m_tonearmDeg);

    const qreal pivotR = s * 0.052;
    painter.setPen(QPen(QColor("#d8d8dc"), 1.0));
    painter.setBrush(QColor("#fafafa"));
    painter.drawEllipse(QPointF(0, 0), pivotR, pivotR);

    const qreal cwW = s * 0.034;
    const qreal cwH = s * 0.055;
    QLinearGradient cwGrad(-cwW / 2, -cwH - 2, cwW / 2, -2);
    cwGrad.setColorAt(0.0, QColor("#aeb4bf"));
    cwGrad.setColorAt(0.5, QColor("#e8eaef"));
    cwGrad.setColorAt(1.0, QColor("#8e95a3"));
    painter.setBrush(cwGrad);
    painter.setPen(QPen(QColor("#7a808c"), 1.0));
    painter.drawRoundedRect(QRectF(-cwW / 2, -cwH - 3.0, cwW, cwH), 2.5, 2.5);

    const qreal rodTop = pivotR * 0.35;
    const qreal rodBot = armLen * 0.86;
    const qreal rodW = qMax(2.2, s * 0.009);
    QLinearGradient rodGrad(-rodW, 0, rodW, 0);
    rodGrad.setColorAt(0.0, QColor("#a8aeb8"));
    rodGrad.setColorAt(0.45, QColor("#eceef2"));
    rodGrad.setColorAt(1.0, QColor("#9098a5"));
    QPainterPath rodPath;
    rodPath.addRoundedRect(QRectF(-rodW / 2, rodTop, rodW, rodBot - rodTop), rodW * 0.45, rodW * 0.45);
    painter.fillPath(rodPath, rodGrad);
    painter.setPen(QPen(QColor(255, 255, 255, 90), 0.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(-rodW * 0.15, rodTop + 4), QPointF(-rodW * 0.15, rodBot - 8));

    const qreal hsW = s * 0.042;
    const qreal hsH = s * 0.068;
    const qreal hsCy = rodBot + hsH * 0.42;
    painter.setPen(QPen(QColor("#e8e8ec"), 1.0));
    painter.setBrush(QColor("#f6f6f8"));
    painter.drawRoundedRect(QRectF(-hsW / 2, hsCy - hsH / 2, hsW, hsH), 3.5, 3.5);

    painter.setPen(QPen(QColor("#c0c4cc"), 1.2));
    painter.setBrush(QColor("#d0d4dc"));
    const QPointF tip(0, hsCy + hsH * 0.48);
    QPainterPath needle;
    needle.moveTo(tip);
    needle.lineTo(-s * 0.006, tip.y() + s * 0.022);
    needle.lineTo(s * 0.006, tip.y() + s * 0.022);
    needle.closeSubpath();
    painter.drawPath(needle);

    painter.restore();
}
