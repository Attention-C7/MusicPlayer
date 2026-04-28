#include "listwidget.h"
#include "ui_listwidget.h"

#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QPainter>
#include <QPaintEvent>
#include <QSet>
#include <QVariant>

namespace
{
constexpr int RoleItemType = Qt::UserRole;
constexpr int RolePath = Qt::UserRole + 1;
constexpr int RoleGroupName = Qt::UserRole + 2;
}

ListWidget::ListWidget(PlayerController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ListWidget)
    , m_controller(controller)
    , m_scanThread(new QThread(this))
    , m_worker(nullptr)
    , m_scanReady(false)
    , m_currentTab(0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    updateCurrentPathLabel();

    connect(ui->btn_back, &QPushButton::clicked, this, [this]() {
        if (m_currentTab == 0) {
            if (!m_dirStack.isEmpty()) {
                m_currentPath = m_dirStack.pop();
                refreshList();
            } else {
                ui->lbl_currentPath->setText(QStringLiteral("已在根目录"));
            }
            return;
        }

        emit backToPlayerRequested();
    });

    connect(ui->listWidget_files, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (m_currentTab == 0) {
            handleItemClicked(item);
        } else {
            handleGroupItemClicked(item);
        }
    });

    connect(ui->btn_tab_dir, &QPushButton::clicked, this, [this]() {
        m_currentPath = m_rootPath;
        m_dirStack.clear();
        m_currentTab = 0;
        refreshList();
    });
    connect(ui->btn_tab_album, &QPushButton::clicked, this, [this]() {
        m_currentTab = 1;
        refreshGroupList(1);
    });
    connect(ui->btn_tab_artist, &QPushButton::clicked, this, [this]() {
        m_currentTab = 2;
        refreshGroupList(2);
    });

    connect(m_controller, &PlayerController::songChanged, this, [this](const SongInfo &info) {
        m_currentPlayingFilePath = info.filePath;
        updatePlayingHighlight();
    });

    connect(m_controller, &PlayerController::currentIndexChanged, this, [this](int) {
        updatePlayingHighlight();
    });

    connect(m_controller, &PlayerController::playModeChanged, this, [this](PlayMode mode) {
        if (mode == PlayMode::AllLoop) {
            m_currentPath = m_rootPath;
            m_dirStack.clear();
            if (isVisible()) {
                refreshList();
            }
        }
    });

    connect(m_controller, &PlayerController::playlistMetaUpdated, this, [this](int index, const SongInfo &info) {
        bool updated = false;
        if (index >= 0 && index < m_allSongs.size() && m_allSongs[index].filePath == info.filePath) {
            m_allSongs[index] = info;
            updated = true;
        } else {
            for (int i = 0; i < m_allSongs.size(); ++i) {
                if (m_allSongs[i].filePath == info.filePath) {
                    m_allSongs[i] = info;
                    updated = true;
                    break;
                }
            }
        }

        if (!updated) {
            return;
        }

        buildGroupMaps();
        if (m_currentTab == 1 || m_currentTab == 2) {
            refreshGroupList(m_currentTab);
        }
    });
}

ListWidget::~ListWidget()
{
    if (m_worker != nullptr) {
        m_worker->cancel();
    }
    if (m_scanThread != nullptr) {
        m_scanThread->quit();
        m_scanThread->wait();
    }
    delete ui;
}

void ListWidget::setRootPath(const QString &path)
{
    const QDir dir(path);
    if (!dir.exists()) {
        return;
    }

    m_rootPath = QDir::cleanPath(dir.absolutePath());
    m_currentPath = m_rootPath;
    m_dirStack.clear();
    startBackgroundScan();
}

void ListWidget::refreshList()
{
    if (m_currentTab != 0) {
        refreshGroupList(m_currentTab);
        return;
    }

    if (m_currentPath.isEmpty()) {
        m_currentPath = m_rootPath;
    }
    if (m_currentPath.isEmpty()) {
        ui->listWidget_files->clear();
        updateCurrentPathLabel();
        return;
    }

    if (!m_scanReady) {
        ui->listWidget_files->clear();
        ui->listWidget_files->addItem(QStringLiteral("加载中..."));
        updateCurrentPathLabel();
        return;
    }

    m_subDirs = FileScanner::scanSubDirs(m_currentPath);
    m_currentSongs.clear();
    const QString currentDir = QDir::cleanPath(QDir(m_currentPath).absolutePath());
    for (const SongInfo &song : m_allSongs) {
        const QString parentDir = QFileInfo(song.filePath).dir().absolutePath();
        if (QDir::cleanPath(parentDir) == currentDir) {
            m_currentSongs.append(song);
        }
    }

    ui->listWidget_files->clear();

    for (const QString &subDirPath : m_subDirs) {
        const QFileInfo folderInfo(subDirPath);
        auto *item = new QListWidgetItem(QStringLiteral("[%1]").arg(folderInfo.fileName()));
        item->setData(RoleItemType, FolderItem);
        item->setData(RolePath, subDirPath);
        item->setSizeHint(QSize(0, 50));
        ui->listWidget_files->addItem(item);
    }

    const int numberWidth = QString::number(m_currentSongs.size()).size();
    for (int i = 0; i < m_currentSongs.size(); ++i) {
        const SongInfo &song = m_currentSongs[i];
        const QFileInfo songInfo(song.filePath);
        QString title = song.title.trimmed();
        if (title.isEmpty()) {
            title = songInfo.completeBaseName();
        }
        const QString artist = song.artist.trimmed();
        const QString displayText = QStringLiteral("%1  %2\n%3")
                                        .arg(i + 1, numberWidth, 10, QChar(' '))
                                        .arg(title)
                                        .arg(artist);

        auto *item = new QListWidgetItem(displayText);
        item->setData(RoleItemType, FileItem);
        item->setData(RolePath, song.filePath);
        item->setSizeHint(QSize(0, 50));
        ui->listWidget_files->addItem(item);
    }

    updateCurrentPathLabel();
    updatePlayingHighlight();
}

void ListWidget::updateCurrentPathLabel()
{
    if (m_currentTab == 1) {
        ui->lbl_currentPath->setText(QStringLiteral("专辑"));
        return;
    }
    if (m_currentTab == 2) {
        ui->lbl_currentPath->setText(QStringLiteral("歌手"));
        return;
    }

    if (m_currentPath.isEmpty() || m_rootPath.isEmpty()) {
        ui->lbl_currentPath->setText(QStringLiteral("/"));
        return;
    }

    const QString cleanRoot = QDir::cleanPath(m_rootPath);
    const QString cleanCurrent = QDir::cleanPath(m_currentPath);
    if (cleanCurrent == cleanRoot) {
        ui->lbl_currentPath->setText(QStringLiteral("/"));
        return;
    }

    QString relative = cleanCurrent;
    if (relative.startsWith(cleanRoot)) {
        relative = relative.mid(cleanRoot.size());
    }
    if (relative.isEmpty()) {
        relative = QStringLiteral("/");
    } else if (!relative.startsWith('/')) {
        relative.prepend('/');
    }
    ui->lbl_currentPath->setText(relative);
}

void ListWidget::updatePlayingHighlight()
{
    for (int i = 0; i < ui->listWidget_files->count(); ++i) {
        QListWidgetItem *item = ui->listWidget_files->item(i);
        const int type = item->data(RoleItemType).toInt();
        const QString path = item->data(RolePath).toString();

        if (type == FileItem && !m_currentPlayingFilePath.isEmpty() && path == m_currentPlayingFilePath) {
            item->setForeground(QBrush(QColor("#ff6900")));
        } else {
            item->setForeground(QBrush(QColor("#ffffff")));
        }
    }
}

void ListWidget::handleItemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const int type = item->data(RoleItemType).toInt();
    const QString path = item->data(RolePath).toString();

    if (type == FolderItem) {
        m_dirStack.push(m_currentPath);
        m_currentPath = path;
        refreshList();
        return;
    }

    if (type != FileItem) {
        return;
    }

    if (!m_currentPlayingFilePath.isEmpty() && path == m_currentPlayingFilePath) {
        emit backToPlayerRequested();
        return;
    }

    m_controller->setPlaylist(m_allSongs);
    m_controller->setFolderPlaylist(m_currentSongs);

    int targetIndex = -1;
    for (int i = 0; i < m_allSongs.size(); ++i) {
        if (m_allSongs[i].filePath == path) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex >= 0) {
        m_controller->playSong(targetIndex);
    }

    emit backToPlayerRequested();
}

void ListWidget::buildGroupMaps()
{
    m_albumMap.clear();
    m_artistMap.clear();
    QSet<QString> visitedPaths;

    for (const SongInfo &song : m_allSongs) {
        if (visitedPaths.contains(song.filePath)) {
            continue;
        }
        visitedPaths.insert(song.filePath);

        QString album = song.album.trimmed();
        if (album.isEmpty()) {
            album = QStringLiteral("未知专辑");
        }
        QString artist = song.artist.trimmed();
        if (artist.isEmpty()) {
            artist = QStringLiteral("未知歌手");
        }

        m_albumMap[album].append(song);
        m_artistMap[artist].append(song);
    }
}

void ListWidget::refreshGroupList(int tab)
{
    ui->listWidget_files->clear();

    if (!m_scanReady) {
        ui->listWidget_files->addItem(QStringLiteral("加载中..."));
        return;
    }

    const QMap<QString, QList<SongInfo>> &groupMap = (tab == 1) ? m_albumMap : m_artistMap;

    for (auto it = groupMap.cbegin(); it != groupMap.cend(); ++it) {
        const QString groupName = it.key();
        const QList<SongInfo> &songs = it.value();

        auto *groupItem = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(groupName).arg(songs.size()));
        groupItem->setData(RoleItemType, GroupItem);
        groupItem->setData(RoleGroupName, groupName);
        groupItem->setSizeHint(QSize(0, 42));
        ui->listWidget_files->addItem(groupItem);

        if (m_expandedGroup == groupName) {
            const int numberWidth = QString::number(songs.size()).size();
            for (int i = 0; i < songs.size(); ++i) {
                const SongInfo &song = songs[i];
                QString title = song.title.trimmed();
                if (title.isEmpty()) {
                    title = QFileInfo(song.filePath).completeBaseName();
                }
                const QString artist = song.artist.trimmed();
                const QString displayText = QStringLiteral("    %1  %2\n    %3")
                                                .arg(i + 1, numberWidth, 10, QChar(' '))
                                                .arg(title)
                                                .arg(artist);

                auto *songItem = new QListWidgetItem(displayText);
                songItem->setData(RoleItemType, FileItem);
                songItem->setData(RolePath, song.filePath);
                songItem->setData(RoleGroupName, groupName);
                songItem->setSizeHint(QSize(0, 50));
                ui->listWidget_files->addItem(songItem);
            }
        }
    }

    updatePlayingHighlight();
}

void ListWidget::handleGroupItemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const int type = item->data(RoleItemType).toInt();
    const QString groupName = item->data(RoleGroupName).toString();

    if (type == GroupItem) {
        if (m_expandedGroup == groupName) {
            m_expandedGroup.clear();
        } else {
            m_expandedGroup = groupName;
        }
        refreshGroupList(m_currentTab);
        return;
    }

    if (type != FileItem) {
        return;
    }

    const QString filePath = item->data(RolePath).toString();
    const QMap<QString, QList<SongInfo>> &groupMap = (m_currentTab == 1) ? m_albumMap : m_artistMap;
    const QList<SongInfo> groupSongs = groupMap.value(groupName);
    if (groupSongs.isEmpty()) {
        return;
    }

    if (!m_currentPlayingFilePath.isEmpty() && m_currentPlayingFilePath == filePath) {
        emit backToPlayerRequested();
        return;
    }

    m_controller->setPlaylist(groupSongs);
    int targetIndex = -1;
    for (int i = 0; i < groupSongs.size(); ++i) {
        if (groupSongs[i].filePath == filePath) {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex >= 0) {
        m_controller->playSong(targetIndex);
    }
    emit backToPlayerRequested();
}

void ListWidget::startBackgroundScan()
{
    if (m_scanThread->isRunning()) {
        if (m_worker != nullptr) {
            m_worker->cancel();
        }
        m_scanThread->quit();
        m_scanThread->wait();
    }

    m_scanReady = false;
    m_allSongs.clear();
    ui->listWidget_files->clear();
    ui->listWidget_files->addItem(QStringLiteral("加载中..."));
    updateCurrentPathLabel();

    m_worker = new FileScanWorker();
    m_worker->moveToThread(m_scanThread);
    connect(this, &ListWidget::requestScan, m_worker, &FileScanWorker::startScan);
    connect(m_worker, &FileScanWorker::scanFinished, this, &ListWidget::onScanFinished);
    connect(m_worker, &FileScanWorker::scanFinished, m_scanThread, &QThread::quit);
    connect(m_scanThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_scanThread, &QThread::finished, this, [this]() {
        m_worker = nullptr;
    });
    connect(m_worker, &FileScanWorker::scanError, this, [this](const QString &message) {
        ui->listWidget_files->clear();
        ui->listWidget_files->addItem(message);
    });

    m_scanThread->start();
    emit requestScan(m_rootPath);
}

void ListWidget::onScanFinished(QList<SongInfo> songs)
{
    m_allSongs = songs;
    buildGroupMaps();
    m_scanReady = true;
    if (m_currentTab == 0) {
        refreshList();
    } else {
        refreshGroupList(m_currentTab);
    }
}

void ListWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 30, 210));

    const QRectF bgRect = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    painter.drawRoundedRect(bgRect, 12.0, 12.0);
}
