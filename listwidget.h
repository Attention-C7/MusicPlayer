#pragma once

#include <QList>
#include <QMap>
#include <QStack>
#include <QString>
#include <QThread>
#include <QWidget>

#include "filescanworker.h"
#include "filescanner.h"
#include "playercontroller.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ListWidget;
}
QT_END_NAMESPACE

class QListWidgetItem;

class ListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ListWidget(PlayerController *controller, QWidget *parent = nullptr);
    ~ListWidget();

    void setRootPath(const QString &path);
    void refreshList();

signals:
    void backToPlayerRequested();
    void requestScan(QString rootPath);

private:
    void paintEvent(QPaintEvent *event) override;

    enum ItemType
    {
        FolderItem = 1,
        FileItem = 2,
        GroupItem = 3
    };

    void updateCurrentPathLabel();
    void updatePlayingHighlight();
    void handleItemClicked(QListWidgetItem *item);
    void buildGroupMaps();
    void refreshGroupList(int tab);
    void handleGroupItemClicked(QListWidgetItem *item);
    void startBackgroundScan();
    void onScanFinished(QList<SongInfo> songs);

    Ui::ListWidget *ui;
    PlayerController *m_controller;
    QThread *m_scanThread;
    FileScanWorker *m_worker;
    bool m_scanReady;
    QString m_rootPath;
    QStack<QString> m_dirStack;
    QString m_currentPath;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_albumMap;
    QMap<QString, QList<SongInfo>> m_artistMap;
    int m_currentTab;
    QString m_expandedGroup;
    QStringList m_subDirs;
    QList<SongInfo> m_currentSongs;
    QString m_currentPlayingFilePath;
};
