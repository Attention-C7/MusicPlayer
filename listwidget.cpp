#include "listwidget.h"
#include "ui_listwidget.h"

#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QPainter>
#include <QPaintEvent>
#include <QVariant>

namespace
{
constexpr int RoleItemType = Qt::UserRole;
constexpr int RolePath = Qt::UserRole + 1;
}

ListWidget::ListWidget(PlayerController *controller, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ListWidget)
    , m_controller(controller)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    updateCurrentPathLabel();

    connect(ui->btn_back, &QPushButton::clicked, this, [this]() {
        if (!m_dirStack.isEmpty()) {
            m_currentPath = m_dirStack.pop();
            refreshList();
            return;
        }
        emit backToPlayerRequested();
    });

    connect(ui->listWidget_files, &QListWidget::itemClicked, this, &ListWidget::handleItemClicked);

    connect(m_controller, &PlayerController::songChanged, this, [this](const SongInfo &info) {
        m_currentPlayingFilePath = info.filePath;
        updatePlayingHighlight();
    });

    connect(m_controller, &PlayerController::currentIndexChanged, this, [this](int) {
        updatePlayingHighlight();
    });
}

ListWidget::~ListWidget()
{
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
    refreshList();
}

void ListWidget::refreshList()
{
    if (m_currentPath.isEmpty()) {
        m_currentPath = m_rootPath;
    }
    if (m_currentPath.isEmpty()) {
        ui->listWidget_files->clear();
        updateCurrentPathLabel();
        return;
    }

    m_subDirs = FileScanner::scanSubDirs(m_currentPath);
    m_currentSongs = FileScanner::scanFiles(m_currentPath);

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
    if (m_currentPath.isEmpty()) {
        ui->lbl_currentPath->setText(QStringLiteral("-"));
        return;
    }
    ui->lbl_currentPath->setText(m_currentPath);
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

    m_controller->setPlaylist(m_currentSongs);

    int targetIndex = -1;
    for (int i = 0; i < m_currentSongs.size(); ++i) {
        if (m_currentSongs[i].filePath == path) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex >= 0) {
        m_controller->playSong(targetIndex);
    }

    emit backToPlayerRequested();
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
