#include "musicplayer.h"
#include "./ui_musicplayer.h"
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QDir>

MusicPlayer::MusicPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPlayer)
    , m_controller(nullptr)
    , m_playWidget(nullptr)
    , m_listWidget(nullptr)
    , m_listAnimation(nullptr)
{
    ui->setupUi(this);

    setFixedSize(800, 500);

    m_controller = new PlayerController(this);
    m_playWidget = new PlayWidget(m_controller, this);
    m_listWidget = new ListWidget(m_controller, this);
    m_listAnimation = new QPropertyAnimation(m_listWidget, "pos", this);

    m_playWidget->setGeometry(0, 0, width(), height());
    m_listWidget->setGeometry(width(), 0, 360, height());
    m_listWidget->hide();

    //m_listWidget->setRootPath(QStringLiteral("/Music"));
    m_listWidget->setRootPath(QDir::homePath() + QStringLiteral("/Music"));


    connect(m_playWidget, &PlayWidget::showListRequested, this, &MusicPlayer::showList);
    connect(m_listWidget, &ListWidget::backToPlayerRequested, this, &MusicPlayer::hideList);
    connect(m_listWidget, &ListWidget::searchContextUpdated, m_playWidget, &PlayWidget::setSearchContext);
}

MusicPlayer::~MusicPlayer()
{
    delete ui;
}

void MusicPlayer::showList()
{
    m_listAnimation->stop();
    m_listAnimation->setDuration(250);
    m_listAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_listAnimation->setStartValue(QPoint(800, 0));
    m_listAnimation->setEndValue(QPoint(440, 0));
    m_listWidget->show();
    m_listWidget->raise();
    m_listAnimation->start();
}

void MusicPlayer::hideList()
{
    m_listAnimation->stop();
    m_listAnimation->setDuration(200);
    m_listAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_listAnimation->setStartValue(QPoint(440, 0));
    m_listAnimation->setEndValue(QPoint(800, 0));

    disconnect(m_listAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
    connect(m_listAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_listWidget->pos().x() >= 800) {
            m_listWidget->hide();
        }
    });
    m_listAnimation->start();
}
