#include "playwidget.h"
#include "ui_playwidget.h"

#include "lyriclinerow.h"
#include "volumesafety.h"

#include <algorithm>

#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpacerItem>
#include <QStyle>
#include <QVBoxLayout>

PlayWidget::PlayWidget(PlayerController *controller, AiController *aiController, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayWidget)
    , m_currentLrcIndex(-1)
    , m_controller(controller)
    , m_aiController(aiController)
    , m_voiceWidget(nullptr)
    , m_allSongs()
    , m_artistMap()
    , m_albumMap()
    , m_beatTimer(new QTimer(this))
    , m_beatEffect(false)
    , m_overlayAlpha(0.0f)
    , m_beatAnim(new QPropertyAnimation(this, "overlayAlpha", this))
    , m_isDragging(false)
    , m_longPressTimer(new QTimer(this))
    , m_pressDirection(0)
    , m_longPressTriggered(false)
    , m_volumePopup(nullptr)
    , m_sliderVolume(nullptr)
    , m_lblVolumePercent(nullptr)
    , m_btnVolumeMute(nullptr)
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
    m_beatTimer->setInterval(500);
    m_beatTimer->setSingleShot(false);
    m_beatAnim->setDuration(200);
    m_beatAnim->setStartValue(0.15f);
    m_beatAnim->setEndValue(0.0f);
    connect(m_beatTimer, &QTimer::timeout, this, &PlayWidget::onBeat);

    ui->slider_progress->setRange(0, 0);
    ui->lbl_currentTime->setText(QStringLiteral("00:00"));
    ui->lbl_totalTime->setText(QStringLiteral("00:00"));
    ui->btn_prev->setText(QString());
    ui->btn_playPause->setText(QString());
    ui->btn_next->setText(QString());
    ui->btn_playMode->setText(QString());
    ui->btn_beat->setText(QString());
    ui->btn_showList->setText(QString());
    ui->btn_prev->setIcon(QIcon(QStringLiteral(":/icons/icon/1previous.png")));
    ui->btn_playPause->setIcon(QIcon(QStringLiteral(":/icons/icon/2play.png")));
    ui->btn_next->setIcon(QIcon(QStringLiteral(":/icons/icon/4next.png")));
    ui->btn_beat->setIcon(QIcon(QStringLiteral(":/icons/icon/11beat.png")));
    ui->btn_showList->setIcon(QIcon(QStringLiteral(":/icons/icon/10list.png")));
    ui->btn_volume->setIcon(QIcon(QStringLiteral(":/icons/icon/9volume.png")));
    const QSize iconSize(28, 28);
    ui->btn_prev->setIconSize(iconSize);
    ui->btn_playPause->setIconSize(iconSize);
    ui->btn_next->setIconSize(iconSize);
    ui->btn_volume->setIconSize(iconSize);
    ui->btn_playMode->setIconSize(iconSize);
    ui->btn_beat->setIconSize(iconSize);
    ui->btn_showList->setIconSize(iconSize);

    setupVolumePopup();
    setPlayModeIcon(m_controller->playMode());
    ui->lbl_index->setText(QStringLiteral("0/0"));
    ui->lbl_title->setText(QStringLiteral("-"));
    ui->lbl_artist->setText(QStringLiteral("<unknown>"));
    ui->lbl_album->setText(QStringLiteral("<unknown>"));
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
            ui->turntableAlbum->setPlaying(true);
            startBeatEffect();
        } else {
            ui->btn_playPause->setIcon(QIcon(QStringLiteral(":/icons/icon/2play.png")));
            ui->turntableAlbum->setPlaying(false);
            stopBeatEffect();
        }
    });

    connect(m_controller, &PlayerController::playModeChanged, this, [this](PlayMode mode) {
        setPlayModeIcon(mode);
        switch (mode) {
        case PlayMode::RandomPlay:
            m_beatTimer->setInterval(400);
            break;
        case PlayMode::SingleLoop:
            m_beatTimer->setInterval(600);
            break;
        case PlayMode::FolderLoop:
        case PlayMode::AllLoop:
            m_beatTimer->setInterval(500);
            break;
        }
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
            ui->turntableAlbum->setAlbumPixmap(QPixmap());
            m_bgPixmap = QPixmap();
            update();
            return;
        }

        updateBackground(pixmap);
        ui->turntableAlbum->setAlbumPixmap(pixmap);
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
    connect(ui->btn_beat, &QPushButton::clicked, this, [this]() {
        setBeatEnabled(!m_beatEffect);
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

    ui->turntableAlbum->setPlaying(m_controller->playbackState() == QMediaPlayer::PlayingState);

    for (QWidget *w : findChildren<QWidget*>()) {
        w->setAutoFillBackground(false);
    }
}

void PlayWidget::startBeatEffect()
{
    if (!m_beatEffect) {
        return;
    }
    if (!m_beatTimer->isActive()) {
        m_beatTimer->start();
    }
}

void PlayWidget::stopBeatEffect()
{
    m_beatTimer->stop();
    m_beatAnim->stop();
    m_overlayAlpha = 0.0f;
    update();
}

void PlayWidget::onBeat()
{
    if (!m_beatEffect) {
        return;
    }

    m_beatAnim->stop();
    setOverlayAlpha(0.15f);
    m_beatAnim->setStartValue(0.15f);
    m_beatAnim->setEndValue(0.0f);
    m_beatAnim->start();
}

void PlayWidget::setBeatEnabled(bool enabled)
{
    m_beatEffect = enabled;
    ui->btn_beat->setStyleSheet(
        enabled
            ? QStringLiteral("background-color: rgba(255,255,255,40); border-radius: 28px;")
            : QString()
    );

    if (!enabled) {
        stopBeatEffect();
        return;
    }

    if (m_controller->playbackState() == QMediaPlayer::PlayingState) {
        startBeatEffect();
    }
}

float PlayWidget::overlayAlpha() const
{
    return m_overlayAlpha;
}

void PlayWidget::setOverlayAlpha(float alpha)
{
    m_overlayAlpha = qBound(0.0f, alpha, 1.0f);
    update();
}

void PlayWidget::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap)
{
    // 转发给VoiceInputWidget
    // VoiceInputWidget再转给AiController
    if (m_voiceWidget) {
        m_voiceWidget->setSearchContext(allSongs, artistMap, albumMap);
    }
}

void PlayWidget::setupVolumePopup()
{
    m_volumePopup = new QFrame(this);
    m_volumePopup->setObjectName(QStringLiteral("frame_volumePopup"));
    m_volumePopup->setFixedSize(88, 228);
    m_volumePopup->hide();

    auto *mainLay = new QVBoxLayout(m_volumePopup);
    mainLay->setContentsMargins(10, 14, 10, 12);
    mainLay->setSpacing(6);

    m_sliderVolume = new QSlider(Qt::Vertical, m_volumePopup);
    m_sliderVolume->setRange(0, 100);
    m_sliderVolume->setSingleStep(1);
    m_sliderVolume->setPageStep(1);
    m_sliderVolume->setTracking(true);
    m_sliderVolume->setMinimumHeight(128);
    m_sliderVolume->setMinimumWidth(52);
    m_sliderVolume->setMaximumWidth(56);

    auto *sliderWrap = new QHBoxLayout();
    sliderWrap->addStretch();
    sliderWrap->addWidget(m_sliderVolume);
    sliderWrap->addStretch();
    mainLay->addLayout(sliderWrap);

    m_lblVolumePercent = new QLabel(QStringLiteral("0%"), m_volumePopup);
    m_lblVolumePercent->setAlignment(Qt::AlignCenter);
    QFont vf = m_lblVolumePercent->font();
    vf.setPointSize(qMax(9, vf.pointSize()));
    m_lblVolumePercent->setFont(vf);

    mainLay->addWidget(m_lblVolumePercent);

    m_btnVolumeMute = new QPushButton(m_volumePopup);
    m_btnVolumeMute->setFlat(true);
    m_btnVolumeMute->setIconSize(QSize(26, 26));
    m_btnVolumeMute->setFixedHeight(34);

    mainLay->addWidget(m_btnVolumeMute);

    // 竖条：min 在下 max 在上 → sub-page 为下端至手柄（已调音量）绿色，add-page 为上端留白
    // 勿给 sub-page/add-page 加 margin，否则会缩小可拖动热区导致只能“点跳”不能拖
    m_volumePopup->setStyleSheet(QStringLiteral(
        "QFrame#frame_volumePopup { background: #ffffff; border-radius: 12px; "
        "border: 1px solid #d8e8dc; }"
        "QLabel { color: #2e7d4a; font-weight: 600; }"
        "QSlider::groove:vertical { border: 1px solid #dde8df; width: 14px; "
        "background: #ffffff; border-radius: 7px; }"
        "QSlider::handle:vertical { background: #3cb371; min-height: 32px; max-height: 32px; "
        "min-width: 32px; max-width: 32px; margin: -14px -12px; border-radius: 16px; "
        "border: 3px solid #ffffff; }"
        // Windows/Fusion 等竖条上，sub-page 实际画在手柄上方、add-page 在下方，与文档命名相反时需对调色
        "QSlider::sub-page:vertical { background: #ffffff; border-radius: 7px; }"
        "QSlider::add-page:vertical { background: #3cb371; border-radius: 7px; }"));

    connect(m_sliderVolume, &QSlider::valueChanged, this, &PlayWidget::onVolumeSliderValueChanged);
    connect(m_sliderVolume, &QSlider::sliderReleased, this, &PlayWidget::onVolumeSliderReleased);
    connect(m_btnVolumeMute, &QPushButton::clicked, this, &PlayWidget::onVolumeMuteButtonClicked);
    connect(ui->btn_volume, &QPushButton::clicked, this, &PlayWidget::onVolumeButtonClicked);
    connect(m_controller, &PlayerController::volumePercentChanged, this,
            &PlayWidget::onControllerVolumePercentChanged);

    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->installEventFilter(this);
    }

    connect(ui->btn_prev, &QPushButton::pressed, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->btn_next, &QPushButton::pressed, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->btn_playPause, &QPushButton::clicked, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->btn_playMode, &QPushButton::clicked, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->btn_beat, &QPushButton::clicked, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->btn_showList, &QPushButton::clicked, this, &PlayWidget::hideVolumePopupIfOpen);
    connect(ui->slider_progress, &QSlider::sliderPressed, this, &PlayWidget::hideVolumePopupIfOpen);

    onControllerVolumePercentChanged(m_controller->volumePercent());
}

void PlayWidget::repositionVolumePopup()
{
    if (m_volumePopup == nullptr || ui->btn_volume == nullptr) {
        return;
    }
    const QPoint anchor = ui->btn_volume->mapTo(this,
        QPoint(ui->btn_volume->width() / 2, 0));
    const int w = m_volumePopup->width();
    const int h = m_volumePopup->height();
    int x = anchor.x() - w / 2;
    int y = anchor.y() - h - 10;
    const int margin = 8;
    x = qBound(margin, x, qMax(margin, width() - w - margin));
    y = qBound(margin, y, qMax(margin, height() - h - margin));
    m_volumePopup->move(x, y);
}

void PlayWidget::refreshVolumeButtonIcon()
{
    if (ui->btn_volume == nullptr) {
        return;
    }
    if (m_controller->isMuted() || m_controller->volumePercent() <= 0) {
        ui->btn_volume->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
    } else {
        ui->btn_volume->setIcon(QIcon(QStringLiteral(":/icons/icon/9volume.png")));
    }
}

void PlayWidget::onVolumeButtonClicked()
{
    if (m_volumePopup == nullptr) {
        return;
    }
    if (m_volumePopup->isVisible()) {
        m_volumePopup->hide();
        return;
    }
    repositionVolumePopup();
    m_volumePopup->show();
    m_volumePopup->raise();
    m_volumePopup->activateWindow();
}

void PlayWidget::hideVolumePopupIfOpen()
{
    if (m_volumePopup != nullptr && m_volumePopup->isVisible()) {
        m_volumePopup->hide();
    }
}

void PlayWidget::onVolumeSliderValueChanged(int value)
{
    m_lblVolumePercent->setText(QString::number(value) + QLatin1Char('%'));

    const int applied = m_controller->volumePercent();
    const int th = VolumeSafety::kWarningThresholdPercent;

    if (value < th) {
        m_controller->setVolumePercent(value);
        return;
    }
    if (applied >= th) {
        m_controller->setVolumePercent(value);
        return;
    }
}

void PlayWidget::onVolumeSliderReleased()
{
    if (m_sliderVolume == nullptr) {
        return;
    }
    const int v = m_sliderVolume->value();
    const int applied = m_controller->volumePercent();
    const int th = VolumeSafety::kWarningThresholdPercent;

    if (v >= th && applied < th) {
        if (VolumeSafety::confirmHighVolumeIfNeeded(v, applied, this)) {
            m_controller->setVolumePercent(v);
        } else {
            const QSignalBlocker blocker(m_sliderVolume);
            m_sliderVolume->setValue(applied);
            m_lblVolumePercent->setText(QString::number(applied) + QLatin1Char('%'));
        }
    }
}

void PlayWidget::onVolumeMuteButtonClicked()
{
    if (m_controller->isMuted()) {
        const int restore = m_controller->volumePercentBeforeMute();
        const int effectiveRestore = restore > 0 ? restore : 50;
        const int curOut = m_controller->volumePercent();
        if (!VolumeSafety::confirmHighVolumeIfNeeded(effectiveRestore, curOut, this)) {
            return;
        }
        m_controller->setMuted(false);
        return;
    }
    m_controller->setMuted(true);
}

void PlayWidget::onControllerVolumePercentChanged(int percent)
{
    if (m_sliderVolume != nullptr) {
        const QSignalBlocker blocker(m_sliderVolume);
        m_sliderVolume->setValue(percent);
    }
    if (m_lblVolumePercent != nullptr) {
        m_lblVolumePercent->setText(QString::number(percent) + QLatin1Char('%'));
    }
    refreshVolumeButtonIcon();
    if (m_btnVolumeMute != nullptr) {
        m_btnVolumeMute->setIcon(style()->standardIcon(m_controller->isMuted()
                ? QStyle::SP_MediaVolumeMuted
                : QStyle::SP_MediaVolume));
    }
}

void PlayWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_volumePopup != nullptr && m_volumePopup->isVisible()) {
        repositionVolumePopup();
    }
}

bool PlayWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (m_volumePopup == nullptr || !m_volumePopup->isVisible()) {
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        const auto *me = static_cast<const QMouseEvent *>(event);
        const QPoint gp = me->globalPosition().toPoint();
        const QRect popupRect(m_volumePopup->mapToGlobal(QPoint(0, 0)), m_volumePopup->size());
        const QRect volumeBtnRect(ui->btn_volume->mapToGlobal(QPoint(0, 0)), ui->btn_volume->size());
        if (popupRect.contains(gp) || volumeBtnRect.contains(gp)) {
            return false;
        }
        m_volumePopup->hide();
    }
    return false;
}

PlayWidget::~PlayWidget()
{
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
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
    if (m_lrcTimesMs.isEmpty() || m_lrcRows.isEmpty()) {
        return;
    }

    const int n = m_lrcTimesMs.size();

    // 快进路径：进度单调递增时多数帧仍落在「当前行～下一行」区间内，避免每帧 upper_bound。
    if (m_currentLrcIndex >= 0 && m_currentLrcIndex < n) {
        const qint64 lineStartMs = m_lrcTimesMs[m_currentLrcIndex];
        if (position >= lineStartMs) {
            const bool lastLine = (m_currentLrcIndex + 1 >= n);
            if (lastLine || position < m_lrcTimesMs[m_currentLrcIndex + 1]) {
                return;
            }
        }
    }

    const auto it = std::upper_bound(m_lrcTimesMs.cbegin(), m_lrcTimesMs.cend(), position);
    const int targetIndex = static_cast<int>(it - m_lrcTimesMs.cbegin()) - 1;

    if (targetIndex < 0 || targetIndex >= m_lrcRows.size()) {
        return;
    }
    if (targetIndex == m_currentLrcIndex) {
        return;
    }

    if (m_currentLrcIndex >= 0 && m_currentLrcIndex < m_lrcRows.size()) {
        m_lrcRows[m_currentLrcIndex]->setActiveLine(false);
    }

    m_currentLrcIndex = targetIndex;
    LyricLineRow *current = m_lrcRows[m_currentLrcIndex];
    current->setActiveLine(true);

    const int centerY = current->y() + (current->height() / 2);
    const int viewportHalf = ui->scrollArea_lrc->viewport()->height() / 2;
    ui->scrollArea_lrc->verticalScrollBar()->setValue(centerY - viewportHalf);
}

void PlayWidget::buildLrcLabels()
{
    QVBoxLayout *layout = ui->verticalLayout_lrc;
    const QList<qint64> keys = m_lrcMap.keys();
    m_lrcTimesMs.clear();
    m_lrcTimesMs.reserve(keys.size());
    for (qint64 key : keys) {
        m_lrcTimesMs.append(key);
        auto *row = new LyricLineRow(key, formatTime(key), m_lrcMap.value(key), ui->scrollAreaWidgetContents_lrc);
        connect(row, &LyricLineRow::seekRequested, m_controller, &PlayerController::seek);
        layout->addWidget(row);
        m_lrcRows.append(row);
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
    m_lrcRows.clear();
    m_lrcTimesMs.clear();
    m_currentLrcIndex = -1;
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
    if (m_beatEffect && m_overlayAlpha > 0.0f) {
        painter.fillRect(rect(), QColor(255, 255, 255, static_cast<int>(m_overlayAlpha * 255.0f)));
    }

    QWidget::paintEvent(event);
}

void PlayWidget::updateIndexLabel()
{
    const PlayContext ctx = m_controller->currentContext();
    const int scope = ctx.scopeIndex + 1;
    const int total = ctx.scopeList.size();
    ui->lbl_index->setText(QStringLiteral("%1/%2").arg(scope).arg(total));

    /*
    const int index = m_controller->currentScopeIndex();
    const int totOld = m_controller->activePlaylistCount();
    const int displayIndex = (index >= 0) ? (index + 1) : 0;
    ui->lbl_index->setText(QStringLiteral("%1/%2").arg(displayIndex).arg(totOld));
    */
}
