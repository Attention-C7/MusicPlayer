#include "playwidget.h"
#include "ui_playwidget.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>

PlayWidget::PlayWidget(PlayerController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayWidget)
    , m_bgLabel(nullptr)
    , m_controller(controller)
    , m_isDragging(false)
    , m_longPressTimer(new QTimer(this))
    , m_pressDirection(0)
    , m_longPressTriggered(false)
{
    ui->setupUi(this);

    m_bgLabel = new QLabel(this);
    m_bgLabel->setGeometry(0, 0, 800, 500);
    m_bgLabel->setScaledContents(true);
    m_bgLabel->setStyleSheet(QStringLiteral("border: none;"));
    auto *blurEffect = new QGraphicsBlurEffect(m_bgLabel);
    blurEffect->setBlurRadius(40.0);
    m_bgLabel->setGraphicsEffect(blurEffect);
    m_bgLabel->lower();
    m_bgLabel->hide();

    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(500);

    ui->slider_progress->setRange(0, 0);
    ui->lbl_currentTime->setText(QStringLiteral("00:00"));
    ui->lbl_totalTime->setText(QStringLiteral("00:00"));
    ui->btn_playPause->setText(QStringLiteral("▶"));
    ui->btn_playMode->setText(playModeText(m_controller->playMode()));
    ui->lbl_index->setText(QStringLiteral("0/0"));
    ui->lbl_title->setText(QStringLiteral("-"));
    ui->lbl_artist->setText(QStringLiteral("<unknown>"));
    ui->lbl_album->setText(QStringLiteral("<unknown>"));
    ui->lbl_albumArt->setText(QStringLiteral("♪"));
    ui->lbl_albumArt->setPixmap(QPixmap());

    connect(m_controller, &PlayerController::songChanged, this, [this](SongInfo info) {
        QString title = info.title.trimmed();
        if (title.isEmpty()) {
            title = QFileInfo(info.filePath).completeBaseName();
        }
        if (title.isEmpty()) {
            title = QStringLiteral("-");
        }

        QString artist = info.artist.trimmed();
        if (artist.isEmpty()) {
            artist = QStringLiteral("<unknown>");
        }

        QString album = info.album.trimmed();
        if (album.isEmpty()) {
            album = QStringLiteral("<unknown>");
        }

        ui->lbl_title->setText(title);
        ui->lbl_artist->setText(artist);
        ui->lbl_album->setText(album);
        updateIndexLabel();
    });

    connect(m_controller, &PlayerController::positionChanged, this, [this](qint64 position) {
        if (m_isDragging) {
            return;
        }
        ui->slider_progress->setValue(static_cast<int>(position));
        ui->lbl_currentTime->setText(formatTime(position));
    });

    connect(m_controller, &PlayerController::durationChanged, this, [this](qint64 duration) {
        ui->slider_progress->setRange(0, static_cast<int>(duration > 0 ? duration : 0));
        ui->lbl_totalTime->setText(formatTime(duration));
    });

    connect(m_controller, &PlayerController::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            ui->btn_playPause->setText(QStringLiteral("⏸"));
        } else {
            ui->btn_playPause->setText(QStringLiteral("▶"));
        }
    });

    connect(m_controller, &PlayerController::playModeChanged, this, [this](PlayMode mode) {
        ui->btn_playMode->setText(playModeText(mode));
    });

    connect(m_controller, &PlayerController::currentIndexChanged, this, [this](int) {
        updateIndexLabel();
    });

    connect(m_controller, &PlayerController::errorOccurred, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("Playback Error"), message);
    });

    connect(m_controller, &PlayerController::albumArtChanged, this, [this](const QPixmap &pixmap) {
        if (pixmap.isNull()) {
            ui->lbl_albumArt->setPixmap(QPixmap());
            ui->lbl_albumArt->setText(QStringLiteral("♪"));
            m_bgLabel->hide();
            return;
        }

        updateBackground(pixmap);
        ui->lbl_albumArt->setText(QString());
        ui->lbl_albumArt->setPixmap(roundedAlbumArt(pixmap));
    });

    connect(ui->btn_playPause, &QPushButton::clicked, this, [this]() {
        m_controller->playPause();
    });

    connect(ui->btn_showList, &QPushButton::clicked, this, &PlayWidget::showListRequested);

    connect(ui->btn_playMode, &QPushButton::clicked, this, [this]() {
        PlayMode nextMode = PlayMode::SingleLoop;
        switch (m_controller->playMode()) {
        case PlayMode::SingleLoop:
            nextMode = PlayMode::FolderLoop;
            break;
        case PlayMode::FolderLoop:
            nextMode = PlayMode::AllLoop;
            break;
        case PlayMode::AllLoop:
            nextMode = PlayMode::RandomPlay;
            break;
        case PlayMode::RandomPlay:
            nextMode = PlayMode::SingleLoop;
            break;
        }
        m_controller->setPlayMode(nextMode);
    });

    connect(ui->slider_progress, &QSlider::sliderPressed, this, [this]() {
        m_isDragging = true;
    });

    connect(ui->slider_progress, &QSlider::valueChanged, this, [this](int value) {
        if (m_isDragging) {
            ui->lbl_currentTime->setText(formatTime(value));
        }
    });

    connect(ui->slider_progress, &QSlider::sliderReleased, this, [this]() {
        m_controller->seek(static_cast<qint64>(ui->slider_progress->value()));
        m_isDragging = false;
    });

    connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
        m_longPressTriggered = true;
        if (m_pressDirection < 0) {
            m_controller->startSeekBackward();
        } else if (m_pressDirection > 0) {
            m_controller->startSeekForward();
        }
    });

    connect(ui->btn_prev, &QPushButton::pressed, this, [this]() {
        m_pressDirection = -1;
        m_longPressTriggered = false;
        m_longPressTimer->start();
    });

    connect(ui->btn_prev, &QPushButton::released, this, [this]() {
        m_longPressTimer->stop();
        if (m_pressDirection != -1) {
            return;
        }
        if (m_longPressTriggered) {
            m_controller->stopSeekBackward();
        } else {
            m_controller->prev();
        }
        m_pressDirection = 0;
        m_longPressTriggered = false;
    });

    connect(ui->btn_next, &QPushButton::pressed, this, [this]() {
        m_pressDirection = 1;
        m_longPressTriggered = false;
        m_longPressTimer->start();
    });

    connect(ui->btn_next, &QPushButton::released, this, [this]() {
        m_longPressTimer->stop();
        if (m_pressDirection != 1) {
            return;
        }
        if (m_longPressTriggered) {
            m_controller->stopSeekForward();
        } else {
            m_controller->next();
        }
        m_pressDirection = 0;
        m_longPressTriggered = false;
    });
}

PlayWidget::~PlayWidget()
{
    delete ui;
}

QString PlayWidget::formatTime(qint64 ms) const
{
    if (ms < 0) {
        ms = 0;
    }
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString PlayWidget::playModeText(PlayMode mode) const
{
    switch (mode) {
    case PlayMode::SingleLoop:
        return QStringLiteral("🔂");
    case PlayMode::FolderLoop:
        return QStringLiteral("📁🔁");
    case PlayMode::AllLoop:
        return QStringLiteral("🔁");
    case PlayMode::RandomPlay:
        return QStringLiteral("🔀");
    }
    return QStringLiteral("🔁");
}

QPixmap PlayWidget::roundedAlbumArt(const QPixmap &pixmap) const
{
    if (pixmap.isNull()) {
        return QPixmap();
    }

    const QSize targetSize(160, 160);
    const QPixmap scaled = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap result(targetSize);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addRoundedRect(QRectF(0.0, 0.0, targetSize.width(), targetSize.height()), 12.0, 12.0);
    painter.setClipPath(path);

    const int offsetX = (scaled.width() - targetSize.width()) / 2;
    const int offsetY = (scaled.height() - targetSize.height()) / 2;
    painter.drawPixmap(-offsetX, -offsetY, scaled);
    painter.end();

    return result;
}

void PlayWidget::updateBackground(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        m_bgLabel->hide();
        return;
    }

    const QSize bgSize(800, 500);
    QPixmap scaled = pixmap.scaled(bgSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (scaled.size() != bgSize) {
        const int offsetX = (scaled.width() - bgSize.width()) / 2;
        const int offsetY = (scaled.height() - bgSize.height()) / 2;
        scaled = scaled.copy(offsetX, offsetY, bgSize.width(), bgSize.height());
    }

    QPainter painter(&scaled);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(scaled.rect(), QColor(10, 10, 20, 200));
    painter.end();

    m_bgLabel->setPixmap(scaled);
    m_bgLabel->show();
    m_bgLabel->lower();
}

void PlayWidget::updateIndexLabel()
{
    const int index = m_controller->currentIndex();
    const int total = m_controller->playlistCount();

    const int displayIndex = (index >= 0) ? (index + 1) : 0;
    ui->lbl_index->setText(QStringLiteral("%1/%2").arg(displayIndex).arg(total));
}
