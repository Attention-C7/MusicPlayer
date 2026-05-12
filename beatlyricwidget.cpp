#include "beatlyricwidget.h"

#include "beatdetector.h"
#include "playwidget.h"

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
#include <QTimer>
#include <QHideEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kLyricFontPx = 48;
constexpr int kLyricFadeMs = 600;
constexpr int kCloseBtnSize = 48;
constexpr int kCloseBtnMargin = 16;

void layoutCloseButtonTopRight(QWidget *host, QPushButton *btn)
{
    if (host == nullptr || btn == nullptr) {
        return;
    }
    const int s = btn->width();
    btn->move(host->width() - s - kCloseBtnMargin, kCloseBtnMargin);
    btn->raise();
}

QColor warmShift(const QColor &sample)
{
    const int r = qBound(0, sample.red() + 40, 255);
    const int g = qBound(0, sample.green() - 10, 255);
    const int b = qBound(0, sample.blue() - 30, 255);
    return QColor(r, g, b);
}

} // namespace

BeatLyricWidget::BeatLyricWidget(QWidget *parent)
    : QWidget(parent)
    , m_lyricAnim(new QPropertyAnimation(this, "lyricAlpha", this))
    , m_beatFlashRise(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashFall(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_beatFlashGroup(new QSequentialAnimationGroup(this))
    , m_breathTimer(new QTimer(this))
    , m_gradTop(80, 40, 30)
    , m_gradBottom(40, 20, 50)
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

    m_breathTimer->setSingleShot(true);
    connect(m_breathTimer, &QTimer::timeout, this, &BeatLyricWidget::onBreathTimeout);

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
    QObject::disconnect(m_bpmConn);
    QObject::disconnect(m_lyricConn);
}

void BeatLyricWidget::setAudioController(BeatDetector *detector)
{
    QObject::disconnect(m_beatConn);
    QObject::disconnect(m_bpmConn);
    m_beatConn = QMetaObject::Connection();
    m_bpmConn = QMetaObject::Connection();
    if (detector == nullptr) {
        return;
    }
    m_beatConn = connect(
        detector,
        &BeatDetector::beatDetected,
        this,
        &BeatLyricWidget::onBeat,
        Qt::QueuedConnection);
    m_bpmConn = connect(
        detector,
        &BeatDetector::bpmUpdated,
        this,
        &BeatLyricWidget::onBpmUpdated,
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
        m_gradTop = QColor(120, 60, 40);
        m_gradBottom = QColor(50, 25, 35);
        return;
    }

    const QImage img = cover.toImage().scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (img.isNull()) {
        m_gradTop = QColor(120, 60, 40);
        m_gradBottom = QColor(50, 25, 35);
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
        m_gradTop = QColor(120, 60, 40);
        m_gradBottom = QColor(50, 25, 35);
        return;
    }

    const QColor avg(static_cast<int>(rs / n), static_cast<int>(gs / n), static_cast<int>(bs / n));
    const QColor w = warmShift(avg);
    m_gradTop = QColor(qMin(255, w.red() + 35), qMin(255, w.green() + 15), qMax(20, w.blue() - 25));
    m_gradBottom = QColor(qMax(25, w.red() - 45), qMax(15, w.green() - 35), qMin(120, w.blue() + 40));
}

void BeatLyricWidget::applyBeatFlash(float intensity)
{
    const float i = qBound(0.0f, intensity, 1.0f);
    float peakAlpha = 0.45f * i;
    peakAlpha = std::max(peakAlpha, 0.05f);
    int durationMs = static_cast<int>(150.0f * (0.5f + 0.5f * i));
    durationMs = std::clamp(durationMs, 70, 200);

    m_beatFlashGroup->stop();
    setOverlayAlpha(0.0f);

    m_beatFlashRise->setDuration(durationMs);
    m_beatFlashRise->setStartValue(0.0f);
    m_beatFlashRise->setEndValue(peakAlpha);

    m_beatFlashFall->setDuration(durationMs);
    m_beatFlashFall->setStartValue(peakAlpha);
    m_beatFlashFall->setEndValue(0.0f);

    m_beatFlashGroup->start();
}

void BeatLyricWidget::scheduleBreathAfterSilence()
{
    if (!isVisible()) {
        return;
    }
    const float bpm = qMax(40.0f, m_currentBpm);
    const int ms = std::max(2000, static_cast<int>(60000.0f / bpm * 1.8f + 0.5f));
    m_breathTimer->start(ms);
}

void BeatLyricWidget::onBeat(float intensity)
{
    if (!isVisible()) {
        return;
    }
    applyBeatFlash(intensity);
    if (!m_inBreathTimeout) {
        scheduleBreathAfterSilence();
    }
}

void BeatLyricWidget::onBpmUpdated(float bpm)
{
    if (std::isfinite(bpm) && bpm > 0.0f) {
        m_currentBpm = std::clamp(bpm, 40.0f, 220.0f);
    }
}

void BeatLyricWidget::onBreathTimeout()
{
    if (!isVisible()) {
        return;
    }
    m_inBreathTimeout = true;
    applyBeatFlash(0.08f);
    m_inBreathTimeout = false;
    const float bpm = qMax(40.0f, m_currentBpm);
    const int nextMs = qMax(50, static_cast<int>(60000.0f / bpm + 0.5f));
    m_breathTimer->start(nextMs);
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

    const QString active = !m_line2.isEmpty() ? m_line2 : m_line1;
    if (active.size() >= 2) {
        m_watermarkText = active.right(2);
    } else if (!active.isEmpty()) {
        m_watermarkText = active;
    } else {
        m_watermarkText.clear();
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
    scheduleBreathAfterSilence();
}

void BeatLyricWidget::hideEvent(QHideEvent *event)
{
    m_breathTimer->stop();
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
    const QColor leftTone = m_gradTop.darker(115);
    const QColor rightTone = m_gradBottom.lighter(130);
    bg.setColorAt(0.0, leftTone);
    bg.setColorAt(0.55, m_gradTop);
    bg.setColorAt(1.0, rightTone);
    painter.fillRect(r, bg);

    painter.setPen(Qt::NoPen);
    const int maxRipple = qMax(r.width(), r.height());
    for (int i = 1; i <= 8; ++i) {
        const int radius = 80 + i * (maxRipple / 14);
        const int a = qMax(8, 55 - i * 6);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, a), i <= 3 ? 2 : 1));
        painter.drawEllipse(rc, radius, radius);
    }
    painter.setPen(Qt::NoPen);

    if (!m_watermarkText.isEmpty()) {
        QFont wf = painter.font();
        wf.setPixelSize(qMin(260, qMax(120, r.height() / 3)));
        wf.setBold(true);
        painter.setFont(wf);
        painter.setPen(QColor(255, 255, 255, 28));
        painter.drawText(r, Qt::AlignCenter, m_watermarkText);
    }

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
