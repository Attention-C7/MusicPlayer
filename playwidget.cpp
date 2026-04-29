#include "playwidget.h"
#include "ui_playwidget.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSpacerItem>
#include <QVBoxLayout>

PlayWidget::PlayWidget(PlayerController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayWidget)
    , m_currentLrcIndex(-1)
    , m_controller(controller)
    , m_aiController(new AiController(this))
    , m_voiceWidget(nullptr)
    , m_allSongs()
    , m_artistMap()
    , m_albumMap()
    , m_isDragging(false)
    , m_longPressTimer(new QTimer(this))
    , m_pressDirection(0)
    , m_longPressTriggered(false)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(false);
    m_voiceWidget = new VoiceInputWidget(
        m_aiController, m_controller,
        m_allSongs, m_artistMap, m_albumMap, this
    );
    if (ui->verticalLayout_main != nullptr) {
        ui->verticalLayout_main->addWidget(m_voiceWidget);
    }

    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(500);

    ui->slider_progress->setRange(0, 0);
    ui->lbl_currentTime->setText(QStringLiteral("00:00"));
    ui->lbl_totalTime->setText(QStringLiteral("00:00"));
    ui->btn_prev->setText(QString());
    ui->btn_playPause->setText(QString());
    ui->btn_next->setText(QString());
    ui->btn_playMode->setText(QString());
    ui->btn_showList->setText(QString());
    ui->btn_prev->setIcon(QIcon(QStringLiteral(":/icons/icon/1previous.png")));
    ui->btn_playPause->setIcon(QIcon(QStringLiteral(":/icons/icon/2play.png")));
    ui->btn_next->setIcon(QIcon(QStringLiteral(":/icons/icon/4next.png")));
    ui->btn_showList->setIcon(QIcon(QStringLiteral(":/icons/icon/10list.png")));
    const QSize iconSize(28, 28);
    ui->btn_prev->setIconSize(iconSize);
    ui->btn_playPause->setIconSize(iconSize);
    ui->btn_next->setIconSize(iconSize);
    ui->btn_playMode->setIconSize(iconSize);
    ui->btn_showList->setIconSize(iconSize);
    setPlayModeIcon(m_controller->playMode());
    ui->lbl_index->setText(QStringLiteral("0/0"));
    ui->lbl_title->setText(QStringLiteral("-"));
    ui->lbl_artist->setText(QStringLiteral("<unknown>"));
    ui->lbl_album->setText(QStringLiteral("<unknown>"));
    ui->lbl_albumArt->setText(QStringLiteral("♪"));
    ui->lbl_albumArt->setPixmap(QPixmap());
    ui->lbl_title->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->lbl_artist->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->lbl_album->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->lbl_index->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->lbl_currentTime->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->lbl_totalTime->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->scrollArea_lrc->setStyleSheet(QStringLiteral("background:transparent;"));
    ui->scrollAreaWidgetContents_lrc->setStyleSheet(QStringLiteral("background:transparent;"));

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
        updateLrcDisplay(position);
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
            ui->btn_playPause->setIcon(QIcon(QStringLiteral(":/icons/icon/3pause.png")));
        } else {
            ui->btn_playPause->setIcon(QIcon(QStringLiteral(":/icons/icon/2play.png")));
        }
    });

    connect(m_controller, &PlayerController::playModeChanged, this, [this](PlayMode mode) {
        setPlayModeIcon(mode);
    });

    connect(m_controller, &PlayerController::currentIndexChanged, this, [this](int) {
        updateIndexLabel();
    });

    connect(m_controller, &PlayerController::errorOccurred, this, [this](const QString &) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("播放错误"));
        msgBox.setText(QStringLiteral("文件无法播放，请检查文件格式是否正确"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.button(QMessageBox::Ok)->setText(QStringLiteral("确定"));

        // Set minimum width so the message text won't be truncated.
        msgBox.setMinimumWidth(400);

        // Force layout expansion for stable dialog width.
        QSpacerItem *spacer = new QSpacerItem(
            400, 0,
            QSizePolicy::Minimum,
            QSizePolicy::Expanding
        );
        QGridLayout *layout =
            qobject_cast<QGridLayout*>(msgBox.layout());
        if (layout != nullptr) {
            layout->addItem(
                spacer, layout->rowCount(), 0,
                1, layout->columnCount()
            );
        }

        msgBox.exec();
    });

    connect(m_controller, &PlayerController::albumArtChanged, this, [this](const QPixmap &pixmap) {
        if (pixmap.isNull()) {
            ui->lbl_albumArt->setPixmap(QPixmap());
            ui->lbl_albumArt->setText(QStringLiteral("♪"));
            m_bgPixmap = QPixmap();
            update();
            return;
        }

        updateBackground(pixmap);
        ui->lbl_albumArt->setText(QString());
        ui->lbl_albumArt->setPixmap(roundedAlbumArt(pixmap));
    });

    connect(m_controller, &PlayerController::lrcLoaded, this, [this](const QMap<qint64, QString> &lyrics) {
        m_lrcMap = lyrics;
        m_currentLrcIndex = -1;
        clearLrcLabels();
        if (!m_lrcMap.isEmpty()) {
            buildLrcLabels();
        }
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

    for (QWidget *w : findChildren<QWidget*>()) {
        w->setAutoFillBackground(false);
    }
}

void PlayWidget::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap
)
{
    m_allSongs = allSongs;
    m_artistMap = artistMap;
    m_albumMap = albumMap;
    if (m_aiController != nullptr) {
        m_aiController->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }
    if (m_voiceWidget != nullptr) {
        m_voiceWidget->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }
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

void PlayWidget::setPlayModeIcon(PlayMode mode)
{
    switch (mode) {
    case PlayMode::SingleLoop:
        ui->btn_playMode->setIcon(QIcon(QStringLiteral(":/icons/icon/5repeatone.png")));
        return;
    case PlayMode::FolderLoop:
        ui->btn_playMode->setIcon(QIcon(QStringLiteral(":/icons/icon/6repeatlist.png")));
        return;
    case PlayMode::AllLoop:
        ui->btn_playMode->setIcon(QIcon(QStringLiteral(":/icons/icon/7repeatall.png")));
        return;
    case PlayMode::RandomPlay:
        ui->btn_playMode->setIcon(QIcon(QStringLiteral(":/icons/icon/8shuffle.png")));
        return;
    }
    ui->btn_playMode->setIcon(QIcon(QStringLiteral(":/icons/icon/6repeatlist.png")));
}

void PlayWidget::updateLrcDisplay(qint64 position)
{
    if (m_lrcMap.isEmpty() || m_lrcLabels.isEmpty()) {
        return;
    }

    const QList<qint64> keys = m_lrcMap.keys();
    int targetIndex = -1;
    for (int i = 0; i < keys.size(); ++i) {
        if (keys[i] <= position) {
            targetIndex = i;
        } else {
            break;
        }
    }

    if (targetIndex < 0 || targetIndex >= m_lrcLabels.size()) {
        return;
    }
    if (targetIndex == m_currentLrcIndex) {
        return;
    }

    if (m_currentLrcIndex >= 0 && m_currentLrcIndex < m_lrcLabels.size()) {
        m_lrcLabels[m_currentLrcIndex]->setStyleSheet(
            QStringLiteral("color:#888888; font-size:13px; font-weight:normal; background:transparent;")
        );
    }

    m_currentLrcIndex = targetIndex;
    QLabel *current = m_lrcLabels[m_currentLrcIndex];
    current->setStyleSheet(
        QStringLiteral("color:#ffffff; font-size:15px; font-weight:bold; background:transparent;")
    );

    const int centerY = current->y() + (current->height() / 2);
    const int viewportHalf = ui->scrollArea_lrc->viewport()->height() / 2;
    ui->scrollArea_lrc->verticalScrollBar()->setValue(centerY - viewportHalf);
}

void PlayWidget::buildLrcLabels()
{
    QVBoxLayout *layout = ui->verticalLayout_lrc;
    const QList<qint64> keys = m_lrcMap.keys();
    for (qint64 key : keys) {
        QLabel *label = new QLabel(m_lrcMap.value(key), ui->scrollAreaWidgetContents_lrc);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        label->setWordWrap(true);
        label->setStyleSheet(QStringLiteral("color:#888888; font-size:13px; font-weight:normal; background:transparent;"));
        layout->addWidget(label);
        m_lrcLabels.append(label);
    }
    layout->addStretch();
}

void PlayWidget::clearLrcLabels()
{
    QVBoxLayout *layout = ui->verticalLayout_lrc;
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item != nullptr) {
            if (item->widget() != nullptr) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }
    m_lrcLabels.clear();
    m_currentLrcIndex = -1;
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
        m_bgPixmap = QPixmap();
        update();
        return;
    }

    const QSize bgSize(800, 500);
    const QPixmap cover = pixmap.scaled(bgSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap canvas(bgSize);
    canvas.fill(Qt::black);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int coverSize = cover.height();
    const int coverX = 0;
    const int coverY = (bgSize.height() - coverSize) / 2;
    painter.drawPixmap(QRect(coverX, coverY, coverSize, coverSize), cover);

    const int rightStart = coverSize;
    if (rightStart < bgSize.width()) {
        const QImage coverImg = cover.toImage();
        const QColor edgeColor = coverImg.pixelColor(coverImg.width() - 1, coverImg.height() / 2);
        QColor darkColor = edgeColor.darker(220);
        darkColor.setAlpha(255);

        QLinearGradient gradient(rightStart, 0, bgSize.width(), 0);
        gradient.setColorAt(0.0, edgeColor);
        gradient.setColorAt(1.0, darkColor);
        painter.fillRect(QRect(rightStart, 0, bgSize.width() - rightStart, bgSize.height()), gradient);
    }

    painter.fillRect(QRect(0, 0, bgSize.width(), bgSize.height()), QColor(10, 10, 20, 160));
    painter.end();

    m_bgPixmap = canvas;
    update();
}

void PlayWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!m_bgPixmap.isNull()) {
        painter.drawPixmap(rect(), m_bgPixmap);
        painter.fillRect(rect(), QColor(10, 10, 20, 180));
    } else {
        painter.fillRect(rect(), QColor("#1a1a2e"));
    }

    QWidget::paintEvent(event);
}

void PlayWidget::updateIndexLabel()
{
    int index = -1;
    if (m_controller->playMode() == PlayMode::AllLoop) {
        index = m_controller->currentIndex();
    } else {
        const int groupIdx = m_controller->groupIndex();
        index = (groupIdx >= 0) ? groupIdx : m_controller->folderIndex();
    }
    const int total = m_controller->activePlaylistCount();

    const int displayIndex = (index >= 0) ? (index + 1) : 0;
    ui->lbl_index->setText(QStringLiteral("%1/%2").arg(displayIndex).arg(total));
}
