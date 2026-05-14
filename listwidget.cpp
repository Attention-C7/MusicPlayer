#include "listwidget.h"
#include "librarylistdelegate.h"
#include "ui_listwidget.h"

#include <QAbstractItemView>
#include <QDir>   //目录，用于操作文件系统：路径规范化与显示名
#include <QFileInfo>
#include <QListWidgetItem>   //列表项，用于显示文件/文件夹/分组：数据绑定、类型识别、样式设置等
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QQueue>   //BFS 构建 m_dirSubdirsMap
#include <QSet>   //集合，用于存储访问过的文件路径：去重与查找
#include <QVariant>   //变体，用于存储列表项数据：类型安全的数据容器

ListWidget::ListWidget(PlayerController *controller, QWidget *parent)
    : QWidget(parent)   //父窗口是parent，QWidget是所有窗口组件的基类，提供窗口管理、事件处理、绘制等功能
    , ui(new Ui::ListWidget)   //初始化UI：加载界面设计器生成的界面，设置窗口无边框、透明背景
    , m_controller(controller)   //播放控制器，负责播放逻辑
    , m_scanThread(new QThread(this))   //创建一个QThread，用于后台扫描：专用扫描线程，避免卡 UI。
    , m_worker(nullptr)   //初始化扫描工作者：nullptr，表示未创建
    , m_scanReady(false)   //初始化扫描准备状态：false，表示未准备好
    , m_currentTab(0)   //初始化当前标签页：0=文件夹，1=专辑，2=歌手，3=全部
    , m_libraryListDelegate(nullptr)
{   //初始化UI：加载界面设计器生成的界面，设置窗口无边框、透明背景
    ui->setupUi(this);   //在this上创建.ui文件对应的UI对象，并初始化其子控件
    m_libraryListDelegate = new LibraryListDelegate(ui->listWidget_files);
    ui->listWidget_files->setItemDelegate(m_libraryListDelegate);
    ui->listWidget_files->setSpacing(4);
    ui->listWidget_files->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listWidget_files->setStyleSheet(QStringLiteral(
        "QListWidget { background: rgba(0,0,0,0.2); border: none; outline: none; color: #e8e8ef; }"
        "QListWidget::item:selected { background: transparent; }"));
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    updateCurrentPathLabel();   //更新当前路径标签
    // 监听返回按钮点击
    connect(ui->btn_back, &QPushButton::clicked, this, [this]() {
        if (m_currentTab == 0) {// 如果当前在【文件夹浏览页】(tab 0)
            if (!m_dirStack.isEmpty()) {//m_dirStack 是目录栈，记录你进入过的文件夹路径
                m_currentPath = m_dirStack.pop();// 取出上一级路径
                refreshList();// 刷新列表，显示上一级文件夹
            } else {// 已经在最顶层了
                ui->lbl_currentPath->setText(QStringLiteral("已在根目录"));
            }
            return;
        }
        // 如果当前在【其他页面（播放列表页）】，不是文件夹页，发出信号：返回播放器界面
        emit backToPlayerRequested();
    });
    // 监听文件列表的【点击事件】
    connect(ui->listWidget_files, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (m_currentTab == 0 || m_currentTab == 3) {// 如果当前在【文件夹浏览页】或【分组列表页】
            handleItemClicked(item); // 统一处理：文件夹/歌曲项
        } else {// 其他页面（播放列表/分组内列表）
            handleGroupItemClicked(item);
        }
    });
    // 切换到【文件夹浏览】页
    connect(ui->btn_tab_dir, &QPushButton::clicked, this, [this]() {
        m_currentPath = m_rootPath;
        m_dirStack.clear();
        m_currentTab = 0;
        refreshList();// 刷新列表
        syncTabVisuals();
    });//切换到【专辑分类】页
    connect(ui->btn_tab_album, &QPushButton::clicked, this, [this]() {
        m_currentTab = 1;
        refreshGroupList(1);// 按专辑分组显示
        syncTabVisuals();
    });//切换到【歌手分类】页
    connect(ui->btn_tab_artist, &QPushButton::clicked, this, [this]() {
        m_currentTab = 2;
        refreshGroupList(2);// 按歌手分组显示
        syncTabVisuals();
    });//切换到【全部歌曲】页
    connect(ui->btn_tab_all, &QPushButton::clicked, this, [this]() {
        m_currentTab = 3;
        refreshAllSongsList();// 显示所有歌曲
        syncTabVisuals();
    });
    // 歌曲切换时 → 同步高亮当前播放歌曲
    connect(m_controller, &PlayerController::songChanged, this, [this](const SongInfo &info) {
        m_currentPlayingFilePath = info.filePath; // 更新当前播放歌曲路径
        updatePlayingHighlight();
    });
    // 歌曲索引变化时 → 同步高亮当前播放歌曲
    connect(m_controller, &PlayerController::currentIndexChanged, this, [this](int) {
        updatePlayingHighlight();
    });
    // 播放模式与 Tab 解耦：不再随 playModeChanged 自动切换文件夹/全部页（仅用户点击 Tab 切换）。
    /*
    connect(m_controller, &PlayerController::playModeChanged, this, [this](PlayMode mode) {
        if (mode == PlayMode::AllLoop) {
            m_currentTab = 3;
            refreshAllSongsList();
        } else {
            m_currentTab = 0;
            refreshList();
        }
    });
    */
    // 播放列表元数据更新时 → 同步更新当前歌曲信息
    connect(m_controller, &PlayerController::playlistMetaUpdated, this, [this](int index, const SongInfo &info) {
        bool updated = false;//先用 index 找（最快）
        if (index >= 0 && index < m_allSongs.size() && m_allSongs[index].filePath == info.filePath) {
            m_allSongs[index] = info;
            updated = true;
        } else {//index 不对 → 遍历用路径找
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
        //重新构建歌手、专辑分组
        buildGroupMaps();
        emit searchContextUpdated(m_allSongs, m_artistMap, m_albumMap);
        if (m_currentTab == 1 || m_currentTab == 2) {//如果当前在【专辑分类】页或【歌手分类】页
            refreshGroupList(m_currentTab);// 刷新列表，显示当前专辑/歌手
        }
    });
    syncTabVisuals();
}
// 列表界面析构函数（关闭界面时自动调用）
ListWidget::~ListWidget()
{
    if (m_worker != nullptr) {//如果扫描工作者存在，则取消扫描
        m_worker->cancel();
    }
    if (m_scanThread != nullptr) {//如果扫描线程存在，则退出并等待
        m_scanThread->quit();
        m_scanThread->wait();
    }
    delete ui;
}
// 设置音乐扫描的根目录
void ListWidget::setRootPath(const QString &path)
{
    const QDir dir(path);//把传入的路径包装成 QDir 对象
    if (!dir.exists()) {//如果路径不存在，则返回
        return;
    }

    m_rootPath = QDir::cleanPath(dir.absolutePath());//清理并保存【绝对路径】（标准化路径）
    m_currentPath = m_rootPath;//初始化当前路径为根路径
    m_dirStack.clear();//清空目录返回栈（回到最顶层）
    startBackgroundScan();//启动后台扫描
}
// 刷新列表（文件夹/歌曲）
void ListWidget::refreshList()
{
    if (m_currentTab == 3) {// 如果当前是【全部歌曲】页 → 跳转到对应刷新
        refreshAllSongsList();
        return;
    }
    if (m_currentTab != 0) {// 如果当前不是【文件夹浏览】页 → 按当前 Tab 刷新（专辑/歌手）
        refreshGroupList(m_currentTab);
        return;
    }
    // 如果当前路径为空 → 恢复为根目录
    if (m_currentPath.isEmpty()) {
        m_currentPath = m_rootPath;
    }
    if (m_currentPath.isEmpty()) {// 如果当前路径为空 → 清空列表并更新标签
        ui->listWidget_files->clear();
        updateCurrentPathLabel();
        return;
    }
    // 如果扫描未准备好 → 清空列表并更新标签
    if (!m_scanReady) {
        ui->listWidget_files->clear();
        ui->listWidget_files->addItem(QStringLiteral("加载中..."));
        updateCurrentPathLabel();
        return;
    }
    const QString currentDir = QDir::cleanPath(QDir(m_currentPath).absolutePath());
    m_currentSongs = m_dirSongsMap.value(currentDir);
    m_subDirs = m_dirSubdirsMap.value(currentDir);
    /*
    m_subDirs = FileScanner::scanSubDirs(m_currentPath);
    m_currentSongs.clear();
    for (const SongInfo &song : m_allSongs) {
        const QString parentDir = QDir::cleanPath(QFileInfo(song.filePath).dir().absolutePath());
        if (parentDir == currentDir) {
            m_currentSongs.append(song);
        }
    }
    */

    ui->listWidget_files->clear(); //清空列表
    // 显示【当前文件夹内的子目录】
    for (const QString &subDirPath : m_subDirs) {
        const QFileInfo folderInfo(subDirPath); //子目录的文件信息
        auto *item = new QListWidgetItem(QStringLiteral("[%1]").arg(folderInfo.fileName())); //创建一个列表项，显示子目录名称
        item->setData(LibraryList::Role::itemType, static_cast<int>(LibraryList::ItemType::folder)); //设置列表项类型为文件夹
        item->setData(LibraryList::Role::path, subDirPath); //设置列表项路径为子目录路径
        item->setSizeHint(QSize(0, 48)); //设置列表项大小（与 LibraryListDelegate 文件夹行一致）
        ui->listWidget_files->addItem(item); //添加到列表
    }
    // 显示【当前文件夹内的歌曲】
    for (int i = 0; i < m_currentSongs.size(); ++i) { //遍历当前文件夹内的歌曲
        const SongInfo &song = m_currentSongs[i]; //当前歌曲
        const QFileInfo songInfo(song.filePath); //歌曲的文件信息
        QString title = song.title.trimmed(); //歌曲的标题
        if (title.isEmpty()) { //如果标题为空，则使用文件名
            title = songInfo.completeBaseName(); //使用文件名
        }
        const QString artist = song.artist.trimmed(); //歌曲的艺术家
        auto *item = new QListWidgetItem(title);
        item->setToolTip(QStringLiteral("%1\n%2").arg(song.filePath, artist.isEmpty() ? QStringLiteral("—") : artist));
        item->setData(LibraryList::Role::itemType, static_cast<int>(LibraryList::ItemType::file)); //设置列表项类型为歌曲
        item->setData(LibraryList::Role::path, song.filePath); //设置列表项路径为歌曲路径
        item->setData(LibraryList::Role::artist, song.artist);
        item->setData(LibraryList::Role::durationMs, song.duration);
        item->setSizeHint(QSize(0, 68)); //与 LibraryListDelegate 歌曲行高一致
        ui->listWidget_files->addItem(item); //添加到列表
    }

    updateCurrentPathLabel(); //更新当前路径标签
    updatePlayingHighlight(); //更新当前播放歌曲高亮
}
// 更新顶部“当前路径”标签文字
void ListWidget::updateCurrentPathLabel()
{
    if (m_currentTab == 1) {//如果当前在【专辑分类】页
        ui->lbl_currentPath->setText(QStringLiteral("专辑"));
        return;
    }
    if (m_currentTab == 2) {//如果当前在【歌手分类】页
        ui->lbl_currentPath->setText(QStringLiteral("歌手"));
        return;
    }
    if (m_currentTab == 3) {//如果当前在【全部歌曲】页
        ui->lbl_currentPath->setText(QStringLiteral("全部"));
        return;
    }

    if (m_currentPath.isEmpty() || m_rootPath.isEmpty()) {//如果当前路径为空或根路径为空
        ui->lbl_currentPath->setText(QStringLiteral("/"));
        return;
    }
    //标准化路径（去掉多余 ../ ./）
    const QString cleanRoot = QDir::cleanPath(m_rootPath);
    const QString cleanCurrent = QDir::cleanPath(m_currentPath);
    if (cleanCurrent == cleanRoot) {//在根目录 → 显示 /
        ui->lbl_currentPath->setText(QStringLiteral("/"));
        return;
    }
    //计算相对路径（从根目录到当前路径）
    QString relative = cleanCurrent; //相对路径
    if (relative.startsWith(cleanRoot)) {//如果相对路径以根路径开头，则去掉根路径
        relative = relative.mid(cleanRoot.size()); //去掉根路径
    }
    if (relative.isEmpty()) {//如果相对路径为空，则显示 /
        relative = QStringLiteral("/");
    } else if (!relative.startsWith('/')) {//如果相对路径不以 / 开头，则添加 /
        relative.prepend('/');
    }
    ui->lbl_currentPath->setText(relative);
}
// 更新播放高亮：当前播放曲由 LibraryListDelegate 根据 Role::isCurrent 绘制
void ListWidget::updatePlayingHighlight()
{
    for (int i = 0; i < ui->listWidget_files->count(); ++i) {
        QListWidgetItem *item = ui->listWidget_files->item(i);
        const int type = item->data(LibraryList::Role::itemType).toInt();
        const QString path = item->data(LibraryList::Role::path).toString();
        const bool isCur = (type == static_cast<int>(LibraryList::ItemType::file))
            && !m_currentPlayingFilePath.isEmpty() && path == m_currentPlayingFilePath;
        item->setData(LibraryList::Role::isCurrent, isCur);
    }
    ui->listWidget_files->viewport()->update();
}
// 处理列表项点击（文件夹 / 歌曲）
void ListWidget::handleItemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }
    // 取出当前点击项的类型和路径
    const int type = item->data(LibraryList::Role::itemType).toInt();// 拿到当前行类型     
    const QString path = item->data(LibraryList::Role::path).toString();   // 拿到当前行路径

    if (type == static_cast<int>(LibraryList::ItemType::folder)) {//点击的是【文件夹】→ 入栈当前路径，并刷新列表
        m_dirStack.push(m_currentPath);// 把当前路径压入返回栈
        m_currentPath = path;// 进入新文件夹
        refreshList();// 刷新列表
        return;
    }

    if (type != static_cast<int>(LibraryList::ItemType::file)) {//点击的不是【歌曲】→ 直接返回
        return;
    }
    if (m_currentTab == 3) {
        int targetIndex = -1;
        for (int i = 0; i < m_allSongs.size(); ++i) {
            if (m_allSongs[i].filePath == path) {
                targetIndex = i;
                break;
            }
        }
        if (targetIndex >= 0) {
            playFromContext(m_allSongs, targetIndex, PlayContext::All);
        }
        return;
    }
    // 如果当前播放歌曲路径与当前点击路径相同，则直接返回
    if (!m_currentPlayingFilePath.isEmpty() && path == m_currentPlayingFilePath) {
        emit backToPlayerRequested();
        return;
    }
    //点击【文件夹内的歌曲】（普通播放）
    m_controller->setPlayMode(PlayMode::FolderLoop);

    int folderSongIndex = -1;
    for (int i = 0; i < m_currentSongs.size(); ++i) {
        if (m_currentSongs[i].filePath == path) {
            folderSongIndex = i;
            break;
        }
    }
    if (folderSongIndex >= 0) {
        playFromContext(m_currentSongs, folderSongIndex, PlayContext::Folder);
    }
}
// 构建分组映射：按【专辑】和【歌手】分类所有歌曲
void ListWidget::buildGroupMaps()
{
    m_albumMap.clear();// 清空专辑映射
    m_artistMap.clear();// 清空艺人映射
    m_dirSongsMap.clear();
    QSet<QString> visitedPaths;// 已访问路径集合，防止重复歌曲（去重）
    // 遍历所有歌曲
    for (const SongInfo &song : m_allSongs) {
        if (visitedPaths.contains(song.filePath)) {// 如果这首歌已经处理过（重复），跳过
            continue;
        }
        visitedPaths.insert(song.filePath);// 将这首歌的路径加入已访问路径集合

        QString album = song.album.trimmed();// 专辑名称
        if (album.isEmpty()) {// 如果专辑名称为空，则设置为“未知专辑”
            album = QStringLiteral("未知专辑");
        }
        QString artist = song.artist.trimmed();// 歌手名称
        if (artist.isEmpty()) {// 如果歌手名称为空，则设置为“未知歌手”
            artist = QStringLiteral("未知歌手");
        }

        m_albumMap[album].append(song); // 将这首歌加入专辑映射
        m_artistMap[artist].append(song); // 将这首歌加入艺人映射

        const QString dir = QDir::cleanPath(QFileInfo(song.filePath).dir().absolutePath());
        m_dirSongsMap[dir].append(song);
    }
    rebuildDirSubdirsMap();
}

void ListWidget::rebuildDirSubdirsMap()
{
    m_dirSubdirsMap.clear();
    if (m_rootPath.isEmpty()) {
        return;
    }
    QQueue<QString> queue;
    QSet<QString> visited;
    const QString cleanRoot = QDir::cleanPath(m_rootPath);
    queue.enqueue(cleanRoot);
    visited.insert(cleanRoot);
    while (!queue.isEmpty()) {
        const QString dir = queue.dequeue();
        const QStringList subs = FileScanner::scanSubDirs(dir);
        m_dirSubdirsMap.insert(dir, subs);
        for (const QString &sub : subs) {
            const QString cleanSub = QDir::cleanPath(sub);
            if (!visited.contains(cleanSub)) {
                visited.insert(cleanSub);
                queue.enqueue(cleanSub);
            }
        }
    }
}

void ListWidget::playFromPath(const QString &filePath)
{
    for (int i = 0; i < m_allSongs.size(); ++i) {
        if (m_allSongs[i].filePath == filePath) {
            playFromContext(m_allSongs, i, PlayContext::All);
            return;
        }
    }
}

void ListWidget::playFromContext(QList<SongInfo> scope, int indexInScope, PlayContext::Source source)
{
    if (indexInScope < 0 || indexInScope >= scope.size() || m_allSongs.isEmpty()) {
        return;
    }
    const QString songPath = scope.at(indexInScope).filePath;
    int globalIndex = -1;
    for (int i = 0; i < m_allSongs.size(); ++i) {
        if (m_allSongs[i].filePath == songPath) {
            globalIndex = i;
            break;
        }
    }
    if (globalIndex < 0) {
        return;
    }
    PlayContext ctx;
    ctx.scopeList = std::move(scope);
    ctx.scopeIndex = indexInScope;
    ctx.source = source;
    ctx.globalIndex = globalIndex;
    m_controller->setPlaylist(m_allSongs);
    m_controller->setContext(ctx);
    m_controller->playSong(globalIndex);
    emit backToPlayerRequested();
}

// 刷新分组列表（专辑 tab1 / 歌手 tab2）
void ListWidget::refreshGroupList(int tab)
{
    ui->listWidget_files->clear();//清空旧列表

    if (!m_scanReady) {// 如果扫描未准备好，则显示“加载中...”
        ui->listWidget_files->addItem(QStringLiteral("加载中..."));
        return;
    }
    //判断当前是【专辑】还是【歌手】
    const QMap<QString, QList<SongInfo>> &groupMap = (tab == 1) ? m_albumMap : m_artistMap;
    // 遍历所有分组（专辑名 / 歌手名）
    for (auto it = groupMap.cbegin(); it != groupMap.cend(); ++it) {
        const QString groupName = it.key();// 分组名（专辑名/歌手名）
        const QList<SongInfo> &songs = it.value();// 该分组下的歌曲

        auto *groupItem = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(groupName).arg(songs.size()));
        groupItem->setData(LibraryList::Role::itemType, static_cast<int>(LibraryList::ItemType::group)); // 标记类型：分组
        groupItem->setData(LibraryList::Role::groupName, groupName);// 保存分组名
        groupItem->setData(LibraryList::Role::isExpanded, m_expandedGroup == groupName);
        groupItem->setSizeHint(QSize(0, 44));// 行高（与 LibraryListDelegate 分组行一致）
        ui->listWidget_files->addItem(groupItem);
        // 如果当前展开的分组是该分组，则显示该分组下的歌曲
        if (m_expandedGroup == groupName) {
            for (int i = 0; i < songs.size(); ++i) {// 遍历该分组下所有歌曲
                const SongInfo &song = songs[i];// 当前歌曲
                QString title = song.title.trimmed();
                if (title.isEmpty()) {// 如果标题为空，则使用文件名
                    title = QFileInfo(song.filePath).completeBaseName();// 使用文件名
                }
                const QString artist = song.artist.trimmed();// 歌手名称
                auto *songItem = new QListWidgetItem(title);
                songItem->setToolTip(QStringLiteral("%1\n%2").arg(song.filePath, artist.isEmpty() ? QStringLiteral("—") : artist));
                songItem->setData(LibraryList::Role::itemType, static_cast<int>(LibraryList::ItemType::file)); // 标记类型：歌曲
                songItem->setData(LibraryList::Role::path, song.filePath); // 保存歌曲路径
                songItem->setData(LibraryList::Role::groupName, groupName); // 保存分组名
                songItem->setData(LibraryList::Role::artist, song.artist);
                songItem->setData(LibraryList::Role::durationMs, song.duration);
                songItem->setSizeHint(QSize(0, 68)); // 行高（与 LibraryListDelegate 歌曲行一致）
                ui->listWidget_files->addItem(songItem);
            }
        }
    }

    updatePlayingHighlight();//高亮当前播放的歌曲
}
// 刷新【全部歌曲】页面的列表
void ListWidget::refreshAllSongsList()
{
    ui->listWidget_files->clear();//清空旧列表
    // 如果扫描未准备好，则显示“加载中...”
    if (!m_scanReady) {
        ui->listWidget_files->addItem(QStringLiteral("加载中..."));
        return;
    }
    for (int i = 0; i < m_allSongs.size(); ++i) {
        const SongInfo &song = m_allSongs[i];
        QString title = song.title.trimmed();
        if (title.isEmpty()) {// 歌曲标题：为空就用文件名
            title = QFileInfo(song.filePath).completeBaseName();
        }
        const QString artist = song.artist.trimmed();
        auto *item = new QListWidgetItem(title);
        item->setToolTip(QStringLiteral("%1\n%2").arg(song.filePath, artist.isEmpty() ? QStringLiteral("—") : artist));
        item->setData(LibraryList::Role::itemType, static_cast<int>(LibraryList::ItemType::file)); // 标记类型：歌曲
        item->setData(LibraryList::Role::path, song.filePath); // 保存歌曲路径
        item->setData(LibraryList::Role::artist, song.artist);
        item->setData(LibraryList::Role::durationMs, song.duration);
        item->setSizeHint(QSize(0, 68)); // 行高（与 LibraryListDelegate 歌曲行一致）
        ui->listWidget_files->addItem(item);
    }

    updatePlayingHighlight();
}
// 处理分组项点击（专辑 tab1 / 歌手 tab2）
void ListWidget::handleGroupItemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {// 如果当前点击项为空，则直接返回
        return;
    }
    // 取出当前点击项的类型和分组名
    const int type = item->data(LibraryList::Role::itemType).toInt();// 拿到当前行类型
    const QString groupName = item->data(LibraryList::Role::groupName).toString();// 拿到当前行分组名

    if (type == static_cast<int>(LibraryList::ItemType::group)) {//点击的是【分组】→ 展开/折叠分组
        if (m_expandedGroup == groupName) {
            m_expandedGroup.clear();// 如果点的是已经展开的分组 → 收起
        } else {
            m_expandedGroup = groupName;// 如果点的是未展开的分组 → 展开
        }
        refreshGroupList(m_currentTab);// 刷新列表，显示当前专辑/歌手
        return;
    }

    if (type != static_cast<int>(LibraryList::ItemType::file)) {//点击的不是【歌曲】→ 直接返回
        return;
    }
    // 拿到当前分组的所有歌曲（专辑内 / 歌手内）
    const QString filePath = item->data(LibraryList::Role::path).toString();// 拿到当前行歌曲路径
    const QMap<QString, QList<SongInfo>> &groupMap = (m_currentTab == 1) ? m_albumMap : m_artistMap;
    const QList<SongInfo> groupSongs = groupMap.value(groupName);// 拿到当前分组的所有歌曲
    if (groupSongs.isEmpty()) {// 如果当前分组没有歌曲，则直接返回
        return;
    }
    // 如果点击的是【正在播放的歌曲】→ 直接返回播放界面
    if (!m_currentPlayingFilePath.isEmpty() && m_currentPlayingFilePath == filePath) {
        emit backToPlayerRequested();
        return;
    }
    int scopeSongIndex = -1;
    for (int i = 0; i < groupSongs.size(); ++i) {
        if (groupSongs[i].filePath == filePath) {
            scopeSongIndex = i;
            break;
        }
    }
    if (scopeSongIndex >= 0) {
        playFromContext(groupSongs, scopeSongIndex, PlayContext::Group);
    }
}
// 启动后台线程扫描音乐
void ListWidget::startBackgroundScan()
{
    if (m_scanThread->isRunning()) {// 如果扫描线程正在运行，则停止扫描
        if (m_worker != nullptr) {// 如果扫描工作者不为空，则取消扫描
            m_worker->cancel();// 取消扫描
        }
        m_scanThread->quit();// 退出扫描线程
        m_scanThread->wait();// 等待扫描线程退出
    }

    m_scanReady = false;// 设置扫描准备状态为false
    m_allSongs.clear();// 清空全量歌曲列表
    m_dirSongsMap.clear();
    m_dirSubdirsMap.clear();
    ui->listWidget_files->clear();// 清空列表
    ui->listWidget_files->addItem(QStringLiteral("加载中..."));// 显示“加载中...”
    updateCurrentPathLabel();// 更新当前路径标签

    m_worker = new FileScanWorker();// 创建扫描工作者
    m_worker->moveToThread(m_scanThread);// 将扫描工作者移动到扫描线程
    connect(this, &ListWidget::requestScan, m_worker, &FileScanWorker::startScan);// 连接请求扫描信号和扫描工作者开始扫描槽
    connect(m_worker, &FileScanWorker::scanFinished, this, &ListWidget::onScanFinished);// 连接扫描完成信号和扫描完成槽
    connect(m_worker, &FileScanWorker::scanFinished, m_scanThread, &QThread::quit);// 连接扫描完成信号和扫描线程退出槽
    connect(m_scanThread, &QThread::finished, m_worker, &QObject::deleteLater);// 连接扫描线程完成信号和扫描工作者删除槽
    connect(m_scanThread, &QThread::finished, this, [this]() {
        m_worker = nullptr;// 将扫描工作者设置为空
    });// 错误处理：扫描失败时显示错误信息
    connect(m_worker, &FileScanWorker::scanError, this, [this](const QString &message) {
        ui->listWidget_files->clear();// 清空列表
        ui->listWidget_files->addItem(message);// 显示错误信息
    });// 连接扫描错误信号和错误处理槽

    m_scanThread->start();// 启动扫描线程
    emit requestScan(m_rootPath);// 发出请求扫描信号，传入根目录路径
}
// 后台扫描音乐完成 → 处理结果
void ListWidget::onScanFinished(QList<SongInfo> songs)
{
    m_allSongs = songs;//保存扫描到的所有歌曲
    buildGroupMaps();//构建分组映射
    emit searchContextUpdated(m_allSongs, m_artistMap, m_albumMap);//发出搜索上下文更新信号，传入全量歌曲列表、艺人分组、专辑分组
    m_scanReady = true;//设置扫描准备状态为true
    if (m_currentTab == 0) {//如果当前在【文件夹浏览页】
        refreshList();//刷新列表
    } else if (m_currentTab == 3) {//如果当前在【全部歌曲页】
        refreshAllSongsList();//刷新列表
    } else {//如果当前在【专辑分类页】或【歌手分类页】
        refreshGroupList(m_currentTab);//刷新列表
    }
}
void ListWidget::syncTabVisuals()
{
    auto applyTab = [](QPushButton *btn, bool selected) {
        if (btn == nullptr) {
            return;
        }
        if (selected) {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: transparent; color: #f5f5f8; font-weight: 600; border: none; "
                "border-bottom: 3px solid #ff7043; padding: 10px 12px 7px 12px; } "
                "QPushButton:hover { color: #ffab91; }"));
        } else {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: transparent; color: #9090a8; font-weight: normal; border: none; "
                "border-bottom: 3px solid transparent; padding: 10px 12px 7px 12px; } "
                "QPushButton:hover { color: #d0d0e4; }"));
        }
    };
    applyTab(ui->btn_tab_dir, m_currentTab == 0);
    applyTab(ui->btn_tab_album, m_currentTab == 1);
    applyTab(ui->btn_tab_artist, m_currentTab == 2);
    applyTab(ui->btn_tab_all, m_currentTab == 3);
}

// 重绘事件：自己画窗口背景
void ListWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);// 忽略不用的参数

    QPainter painter(this); // 创建一个QPainter，用于绘制背景
    painter.setRenderHint(QPainter::Antialiasing, true);// 设置抗锯齿
    painter.setPen(Qt::NoPen);// 设置画笔为无色
    painter.setBrush(QColor(20, 20, 30, 210));// 设置画刷为深紫色

    const QRectF bgRect = rect().adjusted(1.0, 1.0, -1.0, -1.0); // 缩小1像素，避免边缘被裁剪
    painter.drawRoundedRect(bgRect, 12.0, 12.0);// 绘制圆角矩形
}
