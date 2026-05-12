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
#include <QScreen>
#include <QMetaObject>
#include <QPushButton>
#include <QResizeEvent>
#include <QSequentialAnimationGroup>
#include <QShowEvent>
#include <QHideEvent>

namespace {

constexpr int kLyricFontPx = 48;
constexpr int kLyricFadeMs = 600;
constexpr int kCloseBtnSize = 48;
constexpr int kCloseBtnMargin = 16;

/** 无封面时的淡粉渐变与波纹取色。 */
constexpr int kDefaultPinkTopR = 255;
constexpr int kDefaultPinkTopG = 235;
constexpr int kDefaultPinkTopB = 244;
constexpr int kDefaultPinkBotR = 255;
constexpr int kDefaultPinkBotG = 205;
constexpr int kDefaultPinkBotB = 224;
constexpr int kDefaultPinkAccentR = 255;
constexpr int kDefaultPinkAccentG = 160;
constexpr int kDefaultPinkAccentB = 190;

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
    const float sTop = qBound(0.0f, s * 0.42f, 1.0f);
    const float lTop = qBound(0.78f, l + 0.14f, 0.97f);
    const float sBot = qBound(0.0f, s * 0.55f, 1.0f);
    const float lBot = qBound(0.52f, l - 0.08f, 0.90f);
    *gradTop = QColor::fromHslF(h, sTop, lTop, a);
    *gradBottom = QColor::fromHslF(h, sBot, lBot, a);
}

} // namespace

BeatLyricWidget::BeatLyricWidget(QWidget *parent)
    : QWidget(parent)
    , m_lyricAnim(new QPropertyAnimation(this, "lyricAlpha", this))
    , m_beatFlashRise(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashFall(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashGroup(new QSequentialAnimationGroup(this))
    , m_gradTop(kDefaultPinkTopR, kDefaultPinkTopG, kDefaultPinkTopB)
    , m_gradBottom(kDefaultPinkBotR, kDefaultPinkBotG, kDefaultPinkBotB)
    , m_themeColor(kDefaultPinkAccentR, kDefaultPinkAccentG, kDefaultPinkAccentB)
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
}

BeatLyricWidget::~BeatLyricWidget()
{
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
    updateWarmGradientFromCover(cover);
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
}

void BeatLyricWidget::hideEvent(QHideEvent *event)
{
    m_beatFlashGroup->stop();
    QWidget::hideEvent(event);
}

void BeatLyricWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutCloseButtonTopRight(this, m_closeButton);
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

    QLinearGradient bg(0, 0, r.width(), 0);
    const QColor leftTone = m_gradTop.darker(108);
    const QColor rightTone = m_gradBottom.lighter(108);
    bg.setColorAt(0.0, leftTone);
    bg.setColorAt(0.5, m_gradTop);
    bg.setColorAt(1.0, rightTone);
    painter.fillRect(r, bg);

    const float fade = qBound(0.0f, m_lyricAlpha, 1.0f);
    const int maxRipple = qMax(r.width(), r.height());
    constexpr int kRippleCount = 10;
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < kRippleCount; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(kRippleCount);
        const int baseR = 48 + (i * maxRipple) / 11;
        const int spread = static_cast<int>(fade * maxRipple * 0.14f * (1.0f - t * 0.45f));
        const int radius = baseR + spread;
        const int baseA = 46 - i * 4;
        const int a = static_cast<int>(static_cast<float>(qMax(0, baseA)) * fade);
        if (a < 3) {
            continue;
        }
        QColor ring = m_themeColor.lighter(100 + i * 5);
        ring.setAlpha(qBound(0, a, 220));
        const int penW = (fade >= 0.35f) ? (i < 4 ? 2 : 1) : 1;
        painter.setPen(QPen(ring, penW));
        painter.drawEllipse(rc, radius, radius);
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
