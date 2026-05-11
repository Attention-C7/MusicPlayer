#include "turntablealbumwidget.h"

#include <QEasingCurve>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {

constexpr int kShadowMargin = 6;
constexpr int kBaseCornerRadius = 22;
constexpr qreal kMsPerRevolution = 14000.0;

/** 暂停：0° 臂身为竖直向下（在底座上、针尖在唱片圆外）。播放：顺时针略转使针尖落在外圈沟槽。 */
constexpr qreal kTonearmParkedDeg = 0.0;
constexpr qreal kTonearmOnRecordDeg = 14.0;
constexpr int kTonearmAnimMs = 750;

/** 臂长相对 widget 短边比例（加长后可减小播放态转角仍能碰到外圈）。 */
constexpr qreal kArmLengthFactor = 0.62;
/** 支点相对唱片中心水平偏离：略大于 platterR，保证竖直时针身在圆外。 */
constexpr qreal kPivotBeyondPlatterFactor = 0.09;

} // namespace

TurntableAlbumWidget::TurntableAlbumWidget(QWidget *parent)
    : QWidget(parent)
    , m_rotationAnim(new QVariantAnimation(this))
    , m_tonearmAnim(new QVariantAnimation(this))
    , m_rotationDeg(0.0)
    , m_tonearmDeg(kTonearmParkedDeg)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
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

void TurntableAlbumWidget::setPlaying(bool playing)
{
    animateTonearmTo(playing ? kTonearmOnRecordDeg : kTonearmParkedDeg);

    if (playing) {
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

    const int side = qMin(wr.width(), wr.height());
    const QPoint center(wr.center());

    // 底座（圆角矩形）
    const QRect baseRect(
        center.x() - side / 2 + kShadowMargin,
        center.y() - side / 2 + kShadowMargin,
        side - 2 * kShadowMargin,
        side - 2 * kShadowMargin);

    // 柔和投影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 45));
    const QRect shadowR = baseRect.adjusted(4, 6, 8, 10);
    painter.drawRoundedRect(shadowR, kBaseCornerRadius + 2, kBaseCornerRadius + 2);

    // 木纹感浅色底座
    QLinearGradient baseGrad(baseRect.topLeft(), baseRect.bottomRight());
    baseGrad.setColorAt(0.0, QColor("#f7f6f2"));
    baseGrad.setColorAt(0.45, QColor("#ebe8e0"));
    baseGrad.setColorAt(1.0, QColor("#e2dfd6"));
    painter.setBrush(baseGrad);
    painter.setPen(QPen(QColor("#d8d4ca"), 1.0));
    painter.drawRoundedRect(baseRect, kBaseCornerRadius, kBaseCornerRadius);

    // 转盘区域（圆心与底座一致）
    const QPoint platterCenter(baseRect.center());
    const qreal platterR = qMin(baseRect.width(), baseRect.height()) * 0.42;

    QRadialGradient platterGrad(platterCenter, platterR);
    platterGrad.setColorAt(0.0, QColor("#e8eaee"));
    platterGrad.setColorAt(0.85, QColor("#b9bec8"));
    platterGrad.setColorAt(1.0, QColor("#9aa0ab"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(platterGrad);
    painter.drawEllipse(platterCenter, static_cast<int>(platterR), static_cast<int>(platterR));

    const QColor grooveHi("#c8ccd4");
    const QColor grooveLo("#aeb4bf");
    const int grooveCount = 28;
    for (int i = 0; i < grooveCount; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(grooveCount);
        const qreal rr = platterR * (0.72 + t * 0.28);
        const QColor c = (i % 2 == 0) ? grooveHi : grooveLo;
        painter.setPen(QPen(c, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(platterCenter, static_cast<int>(rr), static_cast<int>(rr));
    }

    // 内缘深色环（唱片边缘）
    const qreal labelR = platterR * 0.68;
    painter.setPen(QPen(QColor("#1a1410"), 3.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(platterCenter, static_cast<int>(labelR), static_cast<int>(labelR));

    // 中心封面（旋转）
    const qreal artR = labelR - 4.0;
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
        hole.setColorAt(0.0, QColor("#f0eef5"));
        hole.setColorAt(1.0, QColor("#c5c0cd"));
        painter.fillPath(clipDisk, hole);
        painter.setClipping(false);
        QFont f = font();
        if (f.pointSizeF() > 0) {
            f.setPointSizeF(f.pointSizeF() * 2.2);
        } else {
            f.setPixelSize(qMax(22, static_cast<int>(artR * 0.35)));
        }
        painter.setFont(f);
        painter.setPen(QColor("#6a6570"));
        painter.drawText(QRectF(platterCenter.x() - artR, platterCenter.y() - artR,
                               artR * 2.0, artR * 2.0),
                         Qt::AlignCenter,
                         QStringLiteral("♪"));
    }
    painter.restore();

    // 唱臂：支点在唱片右侧圆外，暂停为竖直；播放为顺时针小角度，针尖落在外圈金属区
    const int baseDim = qMin(baseRect.width(), baseRect.height());
    const qreal pivotDx = platterR + baseDim * kPivotBeyondPlatterFactor;
    const qreal pivotDy = -baseDim * 0.34;
    const qreal armLen = side * kArmLengthFactor;

    painter.save();
    painter.translate(center);
    painter.translate(pivotDx, pivotDy);
    painter.rotate(m_tonearmDeg);

    // 臂身沿 +Y 为竖直向下；末端略向 -X 弯成唱头，顺时针旋转后针尖移向转盘
    QPainterPath arm;
    arm.moveTo(0, 0);
    arm.lineTo(0, armLen * 0.88);
    arm.lineTo(-side * 0.028, armLen * 0.93);
    arm.lineTo(-side * 0.038, armLen * 0.98);
    painter.setPen(QPen(QColor("#e8eaef"), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(arm);

    QLinearGradient wGrad(0, -10, 0, 10);
    wGrad.setColorAt(0.0, QColor("#f2f3f6"));
    wGrad.setColorAt(1.0, QColor("#aeb2bd"));
    painter.setBrush(wGrad);
    painter.setPen(QPen(QColor("#a8acb5"), 1.0));
    painter.drawEllipse(QPointF(0, 0), 10.0, 10.0);
    painter.setBrush(QColor("#ededf0"));
    painter.drawRoundedRect(QRectF(-15.0, -21.0, 13.0, 26.0), 4.0, 4.0);
    painter.restore();
}
