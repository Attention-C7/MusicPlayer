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
#include <QRadialGradient>
#include <QScreen>
#include <QMetaObject>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>

namespace {

constexpr int kLyricFontPx = 48;
constexpr int kBeatAnimMs = 150;
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
    , m_beatAnim(new QPropertyAnimation(this, "overlayAlpha", this))
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

    m_beatAnim->setDuration(kBeatAnimMs);
    m_beatAnim->setStartValue(0.45f);
    m_beatAnim->setEndValue(0.0f);

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setObjectName(QStringLiteral("beatLyricClose"));
    m_closeButton->setFixedSize(kCloseBtnSize, kCloseBtnSize);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setFlat(true);
    m_closeButton->setStyleSheet(QStringLiteral(
        "QPushButton#beatLyricClose {"
        "background-color: rgba(0, 0, 0, 100);"
        "color: rgba(255, 255, 255, 200);"
        "border: 1px solid rgba(255, 255, 255, 90);"
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

void BeatLyricWidget::setAudioController(BeatDetector *detector)
{
    QObject::disconnect(m_beatConn);
    m_beatConn = QMetaObject::Connection();
    if (detector == nullptr) {
        return;
    }
    m_beatConn = connect(
        detector,
        &BeatDetector::beatDetected,
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

void BeatLyricWidget::onBeat()
{
    if (m_beatAnim == nullptr) {
        return;
    }
    m_beatAnim->stop();
    setOverlayAlpha(0.45f);
    m_beatAnim->setStartValue(0.45f);
    m_beatAnim->setEndValue(0.0f);
    m_beatAnim->setDuration(kBeatAnimMs);
    m_beatAnim->start();
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
    if (event->button() == Qt::LeftButton) {
        closeWidget();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void BeatLyricWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect r = rect();

    QLinearGradient bg(0, 0, 0, r.height());
    bg.setColorAt(0.0, m_gradTop);
    bg.setColorAt(1.0, m_gradBottom);
    painter.fillRect(r, bg);

    if (m_overlayAlpha > 0.0f) {
        const int a = static_cast<int>(qBound(0.0f, m_overlayAlpha, 1.0f) * 255.0f);
        painter.fillRect(r, QColor(255, 200, 100, a));
    }

    const int decorR = qMin(220, r.height() / 3);
    const int cx = decorR + 40;
    const int cy = r.center().y();
    QRadialGradient vinyl(cx, cy, static_cast<qreal>(decorR));
    vinyl.setColorAt(0.0, QColor(30, 30, 35, 140));
    vinyl.setColorAt(0.55, QColor(20, 20, 28, 180));
    vinyl.setColorAt(1.0, QColor(10, 10, 15, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(vinyl);
    painter.drawEllipse(QPoint(cx, cy), decorR, decorR);
    painter.setPen(QPen(QColor(255, 255, 255, 50), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPoint(cx, cy), decorR - 18, decorR - 18);

    QFont f = painter.font();
    f.setPixelSize(kLyricFontPx);
    f.setBold(true);
    painter.setFont(f);

    const int textA = static_cast<int>(qBound(0.0f, m_lyricAlpha, 1.0f) * 255.0f);
    const QColor textColor(255, 255, 255, textA);

    const int centerX = r.center().x() + decorR / 4;
    const int wText = r.width() - decorR - 80;
    const int midY = r.center().y();
    const QRect line1Rect(centerX - wText / 2, midY - 130, wText, 70);
    const QRect line2Rect(centerX - wText / 2, midY + 60, wText, 70);

    painter.setPen(textColor);
    painter.drawText(line1Rect, Qt::AlignHCenter | Qt::AlignVCenter, m_line1);

    if (!m_line2.isEmpty()) {
        painter.drawText(line2Rect, Qt::AlignHCenter | Qt::AlignVCenter, m_line2);
    }
}
