#include "beatlyricwidget.h"

#include "playwidget.h"
#include "playercontroller.h"

#include <QFont>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QPixmap>
#include <QRadialGradient>
#include <QScreen>
#include <QMetaObject>
#include <QPushButton>
#include <QResizeEvent>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimer>

#include <cmath>

namespace {

constexpr int kLyricFontPx = 48;
constexpr int kLyricFadeMs = 600;
constexpr int kCloseBtnSize = 48;
constexpr int kCloseBtnMargin = 16;

/** 无封面时：灰玫粉（压低亮度，避免过曝）。 */
constexpr int kDefaultPinkTopR = 198;
constexpr int kDefaultPinkTopG = 158;
constexpr int kDefaultPinkTopB = 172;
constexpr int kDefaultPinkBotR = 158;
constexpr int kDefaultPinkBotG = 122;
constexpr int kDefaultPinkBotB = 138;
constexpr int kDefaultPinkAccentR = 212;
constexpr int kDefaultPinkAccentG = 138;
constexpr int kDefaultPinkAccentB = 168;

void layoutCloseButtonTopRight(QWidget *host, QPushButton *btn)
{
    if (host == nullptr || btn == nullptr) {
        return;
    }
    const int s = btn->width();
    btn->move(host->width() - s - kCloseBtnMargin, kCloseBtnMargin);
    btn->raise();
}

void setDefaultPinkTheme(QColor *gradTop, QColor *gradBottom, QColor *themeColor)
{
    *gradTop = QColor(kDefaultPinkTopR, kDefaultPinkTopG, kDefaultPinkTopB);
    *gradBottom = QColor(kDefaultPinkBotR, kDefaultPinkBotG, kDefaultPinkBotB);
    *themeColor = QColor(kDefaultPinkAccentR, kDefaultPinkAccentG, kDefaultPinkAccentB);
}

/** 由封面平均色生成柔和渐变顶/底与波纹主题色。 */
void gradientFromAverage(const QColor &avg, QColor *gradTop, QColor *gradBottom, QColor *themeColor)
{
    *themeColor = avg;
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.5f;
    float a = 1.0f;
    avg.getHslF(&h, &s, &l, &a);
    const float sTop = qBound(0.0f, s * 0.48f, 1.0f);
    const float lTop = qBound(0.48f, l + 0.04f, 0.78f);
    const float sBot = qBound(0.0f, s * 0.58f, 1.0f);
    const float lBot = qBound(0.32f, l - 0.14f, 0.62f);
    *gradTop = QColor::fromHslF(h, sTop, lTop, a);
    *gradBottom = QColor::fromHslF(h, sBot, lBot, a);
}

} // namespace

/** 铺满 target、中心裁剪，再缩小放大做轻量高斯近似模糊。 */
static QPixmap makeBlurredFillBackdrop(const QPixmap &source, const QSize &target)
{
    if (source.isNull() || target.width() < 8 || target.height() < 8) {
        return QPixmap();
    }

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
    const int divisor = qBound(10, maxSide / 22, 44);
    const int wSmall = qMax(32, target.width() / divisor);
    const int hSmall = qMax(32, target.height() / divisor);
    const QPixmap tiny = cropped.scaled(wSmall, hSmall, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return tiny.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

BeatLyricWidget::BeatLyricWidget(QWidget *parent)
    : QWidget(parent)
    , m_lyricAnim(new QPropertyAnimation(this, "lyricAlpha", this))
    , m_beatFlashRise(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashFall(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashGroup(new QSequentialAnimationGroup(this))
    , m_gradTop(kDefaultPinkTopR, kDefaultPinkTopG, kDefaultPinkTopB)
    , m_gradBottom(kDefaultPinkBotR, kDefaultPinkBotG, kDefaultPinkBotB)
    , m_themeColor(kDefaultPinkAccentR, kDefaultPinkAccentG, kDefaultPinkAccentB)
    , m_waveTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("BeatLyricWidget"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    m_lyricAnim->setDuration(kLyricFadeMs);
    m_lyricAnim->setStartValue(0.0f);
    m_lyricAnim->setEndValue(1.0f);

    m_beatFlashGroup->addAnimation(m_beatFlashRise);
    m_beatFlashGroup->addAnimation(m_beatFlashFall);

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setObjectName(QStringLiteral("beatLyricClose"));
    m_closeButton->setFixedSize(kCloseBtnSize, kCloseBtnSize);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setFlat(true);
    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton#beatLyricClose {"
        "background-color: rgba(0, 0, 0, 130);"
        "color: rgba(255, 255, 255, 240);"
        "border: 2px solid rgba(255, 255, 255, 200);"
        "border-radius: 24px;"
        "font-size: 26px;"
        "font-weight: bold;"
        "padding: 0;"
        "}"
        "QPushButton#beatLyricClose:hover {"
        "background-color: rgba(0, 0, 0, 150);"
        "color: rgba(255, 255, 255, 255);"
        "}"
        "QPushButton#beatLyricClose:pressed {"
        "background-color: rgba(0, 0, 0, 190);"
        "}"));
    connect(m_closeButton, &QPushButton::clicked, this, &BeatLyricWidget::onCloseButtonClicked);

    m_waveTimer->setInterval(42);
    m_waveTimer->setObjectName(QStringLiteral("BeatLyricWaveTimer"));
    connect(m_waveTimer, &QTimer::timeout, this, [this]() {
        m_ripplePhase += 0.09f;
        if (m_ripplePhase > 1000.0f) {
            m_ripplePhase = 0.0f;
        }
        update();
    });
}

BeatLyricWidget::~BeatLyricWidget()
{
    if (m_waveTimer != nullptr) {
        m_waveTimer->stop();
    }
    QObject::disconnect(m_beatConn);
    QObject::disconnect(m_lyricConn);
}

void BeatLyricWidget::setBeatSource(PlayerController *controller)
{
    QObject::disconnect(m_beatConn);
    m_beatConn = QMetaObject::Connection();
    if (controller == nullptr) {
        return;
    }
    m_beatConn = connect(
        controller,
        &PlayerController::beatDetected,
        this,
        &BeatLyricWidget::onBeat,
        Qt::QueuedConnection);
}

void BeatLyricWidget::setLyricController(PlayWidget *lyricSource)
{
    QObject::disconnect(m_lyricConn);
    m_lyricConn = QMetaObject::Connection();
    if (lyricSource == nullptr) {
        return;
    }
    m_lyricConn = connect(
        lyricSource,
        &PlayWidget::lyricCurrentLineChanged,
        this,
        &BeatLyricWidget::onLyricLineChanged,
        Qt::QueuedConnection);
}

void BeatLyricWidget::setBackgroundCover(const QPixmap &cover)
{
    if (cover.isNull()) {
        m_sourceCover = QPixmap();
        m_coverBackdrop = QPixmap();
        m_backdropSize = QSize(0, 0);
        updateWarmGradientFromCover(cover);
        update();
        return;
    }

    constexpr int kMaxSourceSide = 720;
    if (cover.width() > kMaxSourceSide || cover.height() > kMaxSourceSide) {
        m_sourceCover = cover.scaled(
            kMaxSourceSide,
            kMaxSourceSide,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    } else {
        m_sourceCover = cover;
    }

    updateWarmGradientFromCover(m_sourceCover);
    m_backdropSize = QSize(0, 0);
    rebuildBackdrop();
    update();
}

float BeatLyricWidget::lyricAlpha() const
{
    return m_lyricAlpha;
}

void BeatLyricWidget::setLyricAlpha(float alpha)
{
    m_lyricAlpha = qBound(0.0f, alpha, 1.0f);
    update();
}

float BeatLyricWidget::overlayAlpha() const
{
    return m_overlayAlpha;
}

void BeatLyricWidget::setOverlayAlpha(float alpha)
{
    m_overlayAlpha = qBound(0.0f, alpha, 1.0f);
    update();
}

void BeatLyricWidget::updateWarmGradientFromCover(const QPixmap &cover)
{
    if (cover.isNull()) {
        setDefaultPinkTheme(&m_gradTop, &m_gradBottom, &m_themeColor);
        return;
    }

    const QImage img = cover.toImage().scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (img.isNull()) {
        setDefaultPinkTheme(&m_gradTop, &m_gradBottom, &m_themeColor);
        return;
    }

    qint64 rs = 0;
    qint64 gs = 0;
    qint64 bs = 0;
    qint64 n = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            rs += c.red();
            gs += c.green();
            bs += c.blue();
            ++n;
        }
    }
    if (n <= 0) {
        setDefaultPinkTheme(&m_gradTop, &m_gradBottom, &m_themeColor);
        return;
    }

    const QColor avg(static_cast<int>(rs / n), static_cast<int>(gs / n), static_cast<int>(bs / n));
    gradientFromAverage(avg, &m_gradTop, &m_gradBottom, &m_themeColor);
}

void BeatLyricWidget::rebuildBackdrop()
{
    const QSize sz = size();
    if (sz.width() < 32 || sz.height() < 32) {
        m_coverBackdrop = QPixmap();
        m_backdropSize = QSize(0, 0);
        return;
    }
    if (m_sourceCover.isNull()) {
        m_coverBackdrop = QPixmap();
        m_backdropSize = QSize(0, 0);
        return;
    }

    m_coverBackdrop = makeBlurredFillBackdrop(m_sourceCover, sz);
    m_backdropSize = sz;
}

void BeatLyricWidget::applyBeatFlash(float intensity)
{
    const float i = qBound(0.0f, intensity, 1.0f);
    const float peakAlpha = 0.15f + i * 0.3f;
    const int durationMs = 150;

    m_beatFlashGroup->stop();
    const float startAlpha = overlayAlpha();

    m_beatFlashRise->setDuration(durationMs);
    m_beatFlashRise->setStartValue(startAlpha);
    m_beatFlashRise->setEndValue(peakAlpha);

    m_beatFlashFall->setDuration(durationMs);
    m_beatFlashFall->setStartValue(peakAlpha);
    m_beatFlashFall->setEndValue(0.0f);

    m_beatFlashGroup->start();
}

void BeatLyricWidget::onBeat(float intensity)
{
    if (!isVisible()) {
        return;
    }
    if (intensity < 0.6f) {
        return;
    }
    applyBeatFlash(intensity);
}

void BeatLyricWidget::onLyricLineChanged(int lineIndex, const QString &text)
{
    m_currentIndex = lineIndex;

    if (m_lyricAnim == nullptr) {
        return;
    }

    if ((lineIndex % 2) == 0) {
        m_line1 = text;
        m_line2 = QString();

        setLyricAlpha(0.0f);
        m_lyricAnim->stop();
        m_lyricAnim->setStartValue(0.0f);
        m_lyricAnim->setEndValue(1.0f);
        m_lyricAnim->setDuration(600);
        m_lyricAnim->start();
    } else {
        m_line2 = text;
    }

    update();
}

void BeatLyricWidget::onCloseButtonClicked()
{
    closeWidget();
}

void BeatLyricWidget::closeWidget()
{
    if (m_waveTimer != nullptr) {
        m_waveTimer->stop();
    }
    hide();
    emit closed();
}

void BeatLyricWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (windowHandle() != nullptr && screen() != nullptr) {
        setGeometry(screen()->availableGeometry());
    }
    layoutCloseButtonTopRight(this, m_closeButton);
    if (!m_sourceCover.isNull()) {
        rebuildBackdrop();
    }
    if (m_waveTimer != nullptr) {
        m_ripplePhase = 0.0f;
        m_waveTimer->start();
    }
}

void BeatLyricWidget::hideEvent(QHideEvent *event)
{
    if (m_waveTimer != nullptr) {
        m_waveTimer->stop();
    }
    m_beatFlashGroup->stop();
    QWidget::hideEvent(event);
}

void BeatLyricWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutCloseButtonTopRight(this, m_closeButton);
    if (!m_sourceCover.isNull() && size() != m_backdropSize) {
        rebuildBackdrop();
        update();
    }
}

void BeatLyricWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        closeWidget();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void BeatLyricWidget::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
}

void BeatLyricWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect r = rect();
    const QPoint rc = r.center();

    const bool hasBackdrop = !m_coverBackdrop.isNull();
    if (hasBackdrop) {
        if (m_coverBackdrop.size() == r.size()) {
            painter.drawPixmap(r, m_coverBackdrop);
        } else {
            painter.drawPixmap(r, m_coverBackdrop.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        painter.fillRect(r, QColor(0, 0, 0, 128));
        QLinearGradient tint(0, 0, r.width(), 0);
        const QColor leftTone = m_gradTop.darker(112);
        const QColor rightTone = m_gradBottom.lighter(105);
        tint.setColorAt(0.0, QColor(leftTone.red(), leftTone.green(), leftTone.blue(), 70));
        tint.setColorAt(0.5, QColor(m_gradTop.red(), m_gradTop.green(), m_gradTop.blue(), 58));
        tint.setColorAt(1.0, QColor(rightTone.red(), rightTone.green(), rightTone.blue(), 82));
        painter.fillRect(r, tint);
    } else {
        QLinearGradient bg(0, 0, r.width(), 0);
        const QColor leftTone = m_gradTop.darker(112);
        const QColor rightTone = m_gradBottom.lighter(105);
        bg.setColorAt(0.0, leftTone);
        bg.setColorAt(0.5, m_gradTop);
        bg.setColorAt(1.0, rightTone);
        painter.fillRect(r, bg);
    }

    const float fade = qBound(0.0f, m_lyricAlpha, 1.0f);
    const int maxDim = qMax(r.width(), r.height());
    QRadialGradient vig(static_cast<qreal>(rc.x()), static_cast<qreal>(rc.y()), maxDim * 0.72);
    vig.setColorAt(0.0, QColor(0, 0, 0, 0));
    vig.setColorAt(0.55, QColor(0, 0, 0, static_cast<int>(20.0f * (0.3f + 0.7f * fade))));
    vig.setColorAt(1.0, QColor(0, 0, 0, static_cast<int>(78.0f * (0.25f + 0.75f * fade))));
    painter.fillRect(r, vig);

    const float ph = m_ripplePhase;
    constexpr int kRippleCount = 11;
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < kRippleCount; ++i) {
        const float fi = static_cast<float>(i);
        const float breathe = 0.55f + 0.45f * std::sin(ph + fi * 0.62f);
        const float spreadWave = std::sin(ph * 0.88f - fi * 0.38f);
        const float t = (fi + 1.0f) / static_cast<float>(kRippleCount);

        const int jx = static_cast<int>(std::sin(ph * 0.71f + fi * 0.95f) * 16.0f * fade);
        const int jy = static_cast<int>(std::cos(ph * 0.66f + fi * 0.78f) * 11.0f * fade);
        const QPoint c = rc + QPoint(jx, jy);

        const int baseR = 44 + (i * maxDim) / 10;
        const int spread = static_cast<int>(
            fade * maxDim * 0.15f * (1.0f - t * 0.48f) * (0.78f + 0.22f * breathe));
        const int wobble = static_cast<int>(9.0f * std::sin(ph + fi * 0.47f) * fade);
        const int radius = baseR + spread + wobble;

        const int baseA = 36 - i * 3;
        const float shimmer = 0.5f + 0.5f * breathe * (0.88f + 0.12f * spreadWave);
        const int a = static_cast<int>(static_cast<float>(qMax(0, baseA)) * fade * shimmer);
        if (a < 5) {
            continue;
        }

        QColor halo = m_themeColor;
        halo.setAlpha(qBound(0, static_cast<int>(static_cast<float>(a) * 0.42f), 105));
        painter.setPen(QPen(halo, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(c, radius + 3, radius + 3);

        QColor core = m_themeColor.lighter(102 + i * 3);
        core.setAlpha(qBound(0, a, 195));
        painter.setPen(QPen(core, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(c, radius, radius);
    }
    painter.setPen(Qt::NoPen);

    if (m_overlayAlpha > 0.0f) {
        const int a = static_cast<int>(qBound(0.0f, m_overlayAlpha, 1.0f) * 255.0f);
        painter.fillRect(r, QColor(255, 255, 255, a));
    }

    const int wText = r.width() - 100;
    const int midY = r.center().y();
    const QRect line1Rect(rc.x() - wText / 2, midY - 88, wText, 64);
    const QRect line2Rect(rc.x() - wText / 2, midY + 20, wText, 64);

    QFont f = painter.font();
    f.setPixelSize(kLyricFontPx);
    f.setBold(true);
    painter.setFont(f);

    const int textA = static_cast<int>(qBound(0.0f, m_lyricAlpha, 1.0f) * 255.0f);
    const QColor textColor(255, 255, 255, textA);
    const QColor shadowColor(0, 0, 0, qMin(200, textA));

    auto drawLyricLine = [&](const QRect &rect, const QString &txt) {
        if (txt.isEmpty()) {
            return;
        }
        painter.setPen(shadowColor);
        painter.drawText(rect.translated(2, 2), Qt::AlignHCenter | Qt::AlignVCenter, txt);
        painter.setPen(textColor);
        painter.drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, txt);
    };

    drawLyricLine(line1Rect, m_line1);
    drawLyricLine(line2Rect, m_line2);
}
