#pragma once

#include <QList>
#include <QStack>
#include <QString>
#include <QWidget>

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

private:
    enum ItemType
    {
        FolderItem = 1,
        FileItem = 2
    };

    void updateCurrentPathLabel();
    void updatePlayingHighlight();
    void handleItemClicked(QListWidgetItem *item);

    Ui::ListWidget *ui;
    PlayerController *m_controller;
    QString m_rootPath;
    QStack<QString> m_dirStack;
    QString m_currentPath;
    QStringList m_subDirs;
    QList<SongInfo> m_currentSongs;
    QString m_currentPlayingFilePath;
};
