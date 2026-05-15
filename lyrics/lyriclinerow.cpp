#include "lyriclinerow.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>

LyricLineRow::LyricLineRow(qint64 timeMs, const QString &timeText, const QString &lyricText, QWidget *parent)
    : QWidget(parent)
    , m_timeMs(timeMs)
    , m_hoverChrome(new QWidget(this))
    , m_playBtn(new QToolButton(m_hoverChrome))
    , m_timeLabel(new QLabel(timeText, m_hoverChrome))
    , m_textLabel(new QLabel(lyricText, this))
{
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    m_hoverChrome->setMouseTracking(true);
    m_hoverChrome->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_playBtn->setObjectName(QStringLiteral("lyricLinePlayBtn"));
    m_playBtn->setIcon(QIcon(QStringLiteral(":/icons/icon/2play.png")));
    m_playBtn->setIconSize(QSize(11, 11));
    m_playBtn->setFixedSize(22, 22);
    m_playBtn->setAutoRaise(true);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setStyleSheet(QStringLiteral(
        "QToolButton#lyricLinePlayBtn { background-color: rgba(30,30,30,220); border: none; border-radius: 11px; padding: 0; }"
        "QToolButton#lyricLinePlayBtn:hover { background-color: rgba(48,48,48,235); }"
        "QToolButton#lyricLinePlayBtn:pressed { background-color: rgba(22,22,22,240); }"));

    QFont timeFont = m_timeLabel->font();
    if (timeFont.pointSize() > 0) {
        timeFont.setPointSize(qMax(9, timeFont.pointSize() - 1));
    }
    m_timeLabel->setFont(timeFont);
    m_timeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_timeLabel->setStyleSheet(QStringLiteral("color:#aaaaaa; background:transparent;"));
    m_timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *chromeLay = new QHBoxLayout(m_hoverChrome);
    chromeLay->setContentsMargins(0, 0, 4, 0);
    chromeLay->setSpacing(6);
    chromeLay->addWidget(m_playBtn);
    chromeLay->addWidget(m_timeLabel);

    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(2, 2, 2, 2);
    mainLay->setSpacing(6);
    mainLay->addWidget(m_hoverChrome, 0, Qt::AlignVCenter);
    mainLay->addWidget(m_textLabel, 1);

    connect(m_playBtn, &QToolButton::clicked, this, [this]() {
        emit seekRequested(m_timeMs);
    });

    m_hoverChrome->hide();
    setActiveLine(false);
}

void LyricLineRow::setActiveLine(bool active)
{
    if (active) {
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#ffffff; font-size:18px; font-weight:bold; background:transparent;"));
        m_timeLabel->setStyleSheet(QStringLiteral("color:#FF7043; background:transparent;"));
    } else {
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#9696a8; font-size:14px; font-weight:normal; background:transparent;"));
        m_timeLabel->setStyleSheet(QStringLiteral("color:#888888; background:transparent;"));
    }
}

void LyricLineRow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hoverChrome->show();
}

void LyricLineRow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hoverChrome->hide();
}

void LyricLineRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit seekRequested(m_timeMs);
    }
    QWidget::mouseReleaseEvent(event);
}
