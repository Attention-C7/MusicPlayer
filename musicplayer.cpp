#include "musicplayer.h"
#include "./ui_musicplayer.h"

MusicPlayer::MusicPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MusicPlayer)
    , m_stack(nullptr)
    , m_controller(nullptr)
    , m_playWidget(nullptr)
    , m_listWidget(nullptr)
{
    ui->setupUi(this);

    m_controller = new PlayerController(this);
    m_playWidget = new PlayWidget(m_controller, this);
    m_listWidget = new ListWidget(m_controller, this);

    m_stack = ui->stackedWidget;
    m_stack->addWidget(m_playWidget); // index 0
    m_stack->addWidget(m_listWidget); // index 1
    m_stack->setCurrentIndex(0);

    m_listWidget->setRootPath(QStringLiteral("/Music"));

    connect(m_playWidget, &PlayWidget::showListRequested, this, [this]() {
        m_stack->setCurrentIndex(1);
    });
    connect(m_listWidget, &ListWidget::backToPlayerRequested, this, [this]() {
        m_stack->setCurrentIndex(0);
    });

    setFixedSize(520, 320);
}

MusicPlayer::~MusicPlayer()
{
    delete ui;
}
