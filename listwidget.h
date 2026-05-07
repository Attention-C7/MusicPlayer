#pragma once

#include <QList>
#include <QMap>
#include <QStack>   //目录后退栈
#include <QString>
#include <QThread>   //线程，用于后台扫描
#include <QWidget>

#include "filescanworker.h"   //文件扫描工作线程，用于后台扫描：在 QThread 上跑的文件扫描工作者
#include "filescanner.h"   //文件扫描器，用于扫描文件：具体扫描逻辑（由 worker 驱动）
#include "playercontroller.h"   //注入的播放与列表中枢，ListWidget 监听其 songChanged 等以高亮当前曲、同步元数据。

QT_BEGIN_NAMESPACE   //前向声明 Ui::ListWidget。真正的定义在生成的 ui_listwidget.h 里。
namespace Ui {
class ListWidget;
}
QT_END_NAMESPACE

class QListWidgetItem;   //前向声明 QListWidgetItem：私有方法参数里用到指针，不必在头里 #include <QListWidgetItem>，减轻依赖。

class ListWidget : public QWidget
{
    Q_OBJECT  //Q_OBJECT 宏声明 ListWidget 类为 Qt 对象，自动生成信号和槽机制。本类有 signals 和 Q_PROPERTY，必须由 moc 处理。

public:
    explicit ListWidget(PlayerController *controller, QWidget *parent = nullptr);  //构造：必须传入 PlayerController *；parent 交给 QWidget。
    ~ListWidget();  //析构：头文件未标 override，实现里应释放 ui 等（与常见 Qt 写法一致）。

    void setRootPath(const QString &path);  //设置根目录：设置扫描/浏览根目录（musicplayer.cpp 里用 QDir::homePath() + "/Music" 等）。
    void refreshList();  //刷新列表：扫描/刷新当前路径下所有文件，更新 m_allSongs、m_albumMap、m_artistMap，并 emit searchContextUpdated 信号。

signals:
    void backToPlayerRequested();  //用户点「返回」时由 ListWidget emit，MusicPlayer 收到后执行 hideList()，从而列表页不直接操作播放页几何，只发「请求」，降低与主窗口布局的耦合。
    void requestScan(QString rootPath);  //扫描请求：由 startBackgroundScan() 触发，传入根目录路径。
    void searchContextUpdated(  //当前曲库快照：与 PlayWidget::setSearchContext 参数对齐，同步语音/AI 的检索上下文。
        QList<SongInfo> allSongs,  //全量列表，供语音/AI 在「当前扫描结果」里检索。
        QMap<QString, QList<SongInfo>> artistMap,  //艺人分组，供语音/AI 在「当前扫描结果」里检索。
        QMap<QString, QList<SongInfo>> albumMap   //专辑分组，供语音/AI 在「当前扫描结果」里检索。
    );

private:
    void paintEvent(QPaintEvent *event) override;   //自绘背景或列表区域视觉效果（与 PlayWidget 类似，用 paintEvent 做统一皮肤下的补充绘制）。

    enum ItemType
    {
        FolderItem = 1,
        FileItem = 2,
        GroupItem = 3
    };

    void updateCurrentPathLabel();   //更新当前路径标签：显示当前扫描/浏览的目录路径。与 main.cpp 里 lbl_currentPath 的 QSS 对应
    void updatePlayingHighlight();   //高亮当前播放曲：根据 m_currentPlayingFilePath 更新列表项颜色。
    void handleItemClicked(QListWidgetItem *item);   //处理单曲点击：根据 item 类型（文件/文件夹/分组）调用不同处理逻辑。
    void buildGroupMaps();   //构建分组映射：根据 m_allSongs 构建 m_albumMap 和 m_artistMap，供语音/AI 在「当前扫描结果」里检索。
    void refreshGroupList(int tab);   //刷新分组列表：根据 m_currentTab 刷新 m_albumMap 或 m_artistMap。
    void refreshAllSongsList();   //刷新全量列表：刷新 m_allSongs 列表，并 emit searchContextUpdated 信号。「全部」Tab 平铺 m_allSongs。
    void handleGroupItemClicked(QListWidgetItem *item);   //处理分组点击：根据 item 类型（文件/文件夹/分组）调用不同处理逻辑。
    void startBackgroundScan();   //启动后台扫描：初始化扫描线程和工作者，连接信号槽，启动线程。起线程、建 worker、连 requestScan、在扫描结束后 onScanFinished。
    void onScanFinished(QList<SongInfo> songs);   //扫描完成：更新 m_allSongs、m_albumMap、m_artistMap，并 emit searchContextUpdated 信号。

    Ui::ListWidget *ui;  //Designer 生成控件树（列表、Tab 按钮、返回键等）。
    PlayerController *m_controller;  //播放与播放列表，不负责 delete（由 MusicPlayer 拥有）。
    QThread *m_scanThread;  //扫描线程，用于后台扫描：专用扫描线程，避免卡 UI。
    FileScanWorker *m_worker;  //FileScanWorker，moveToThread(m_scanThread) 后在子线程执行 startScan。
    bool m_scanReady;  //扫描准备状态：true 表示扫描已完成，false 表示扫描进行中。防止重复启动或 UI 未准备好（具体逻辑在 .cpp）。
    QString m_rootPath;  //用户设定的音乐库根。
    QStack<QString> m_dirStack;  //目录后退栈：用于「返回」按钮，记录浏览历史。与「返回」按钮在 Tab 0 时 pop 路径一致
    QString m_currentPath;  //当前扫描/浏览的目录路径。
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_albumMap;
    QMap<QString, QList<SongInfo>> m_artistMap;
    int m_currentTab;  //当前选中 Tab 索引：0-全部、1-专辑、2-艺人、3-文件夹
    QString m_expandedGroup;  //当前展开的分组名称：用于「专辑」和「艺人」Tab 的折叠/展开状态。
    QStringList m_subDirs;  //当前目录下所有子目录路径列表。
    QList<SongInfo> m_currentSongs;  //当前目录下所有歌曲列表。
    QString m_currentPlayingFilePath;  //当前播放歌曲路径：用于高亮当前播放曲。
};
