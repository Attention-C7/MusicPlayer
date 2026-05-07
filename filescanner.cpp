#include "filescanner.h"

#include <QDir> //列目录、判断存在、过滤文件
#include <QDirIterator> //递归遍历子目录
#include <QFileInfo> //文件信息。拿绝对路径、基名、存在性，符合「路径用 QDir/QFileInfo」的用法。
#include <QFileInfoList>    //文件列表

#include <taglib/fileref.h> //读音频内嵌标签（标题/艺人/专辑等），与 CMake 里链接的 TagLib 对应
#include <taglib/tag.h> //TagLib 标签结构，用于读取 ID3 元数据。

namespace  //音频扩展名过滤，供 scanFiles/scanAllFiles/scanSubDirs 使用。
{  //匿名命名空间：audioNameFilters() 只在本编译单元使用，避免与其它 .cpp 里同名函数链接冲突；返回 QStringList 给 entryInfoList 做后缀过滤。
QStringList audioNameFilters()
{
    return {"*.mp3", "*.wav", "*.wma", "*.MP3", "*.WAV", "*.WMA"};
}
}

QList<SongInfo> FileScanner::scanFiles(const QString &dirPath)
{
    QList<SongInfo> songs;  //定义一个空的歌曲列表，用来存放扫描到的歌曲
    //用 QDir(dirPath) 检查存在性；目录不存在则返回空列表，避免无效 IO。
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return songs;
    }
    //用 entryInfoList 列出目录下所有文件，符合音频扩展名的文件会被过滤出来。
    const QFileInfoList fileList = dir.entryInfoList(
        audioNameFilters(), //只取普通文件、跳过 symlink、要求可读；格式限定在 mp3/wav/wma。
        QDir::Files | QDir::NoSymLinks | QDir::Readable //QDir::Files：只列文件；QDir::NoSymLinks：不列符号链接；QDir::Readable：只列可读文件。
    );
    //用 reserve 预分配空间，避免多次扩容；fileList.size() 是文件数量，作为 capacity 传入。
    songs.reserve(fileList.size());
    for (const QFileInfo &fileInfo : fileList) {
        SongInfo song;  //新建 SongInfo：filePath 用 absoluteFilePath()；lrcPath 先置空。
        song.filePath = fileInfo.absoluteFilePath();
        song.lrcPath = QString();
        //同目录、同「基名」的 .lrc：存在则写入 lrcPath，供 PlayerController 里歌词加载使用（与 songinfo.h 字段一致）。
        const QString lrcFilePath = fileInfo.dir().absoluteFilePath(fileInfo.completeBaseName() + QStringLiteral(".lrc"));
        if (QFileInfo::exists(lrcFilePath)) {
            song.lrcPath = lrcFilePath;
        }
        //文件名启发式：默认 title = 无扩展名文件名；若文件名含 " - " 或单个 -，则拆成 艺人 - 标题。TagLib 为空时 UI 仍可有像样展示。
        // ID3 metadata will be parsed in PlayerController.
        const QString baseName = fileInfo.completeBaseName();   // 获取文件【不含后缀】的文件名
        song.title = baseName;  //默认标题 = 完整文件名
        song.artist = QString();  // 默认艺术家 = 空字符串
        
        int sepPos = baseName.indexOf(QStringLiteral(" - "));
        int sepLen = 3;
        if (sepPos < 0) {
            sepPos = baseName.indexOf(QLatin1Char('-'));
            sepLen = 1;
        }
        if (sepPos > 0 && sepPos < baseName.size() - sepLen) {
            const QString artist = baseName.left(sepPos).trimmed();
            const QString title = baseName.mid(sepPos + sepLen).trimmed();
            if (!artist.isEmpty() && !title.isEmpty()) {
                song.artist = artist;
                song.title = title;
            }
        }

        song.album = QString(); //album 先空，后面由 TagLib 可能覆盖

        TagLib::FileRef f(song.filePath.toLocal8Bit().constData());
        if (!f.isNull() && f.tag() != nullptr) {//判断文件是否打开成功、标签是否存在
            TagLib::Tag *tag = f.tag(); // 获取标签对象 从标签读取 标题、歌手、专辑，并转成 UTF-8 字符串（解决乱码）
            const QString tagTitle = QString::fromUtf8(tag->title().toCString(true)).trimmed();
            const QString tagArtist = QString::fromUtf8(tag->artist().toCString(true)).trimmed();
            const QString tagAlbum = QString::fromUtf8(tag->album().toCString(true)).trimmed();
            // 标签不为空，就覆盖之前从文件名拆分的信息（优先级：标签 > 文件名）
            if (!tagTitle.isEmpty()) {
                song.title = tagTitle;
            }
            if (!tagArtist.isEmpty()) {
                song.artist = tagArtist;
            }
            if (!tagAlbum.isEmpty()) {
                song.album = tagAlbum;
            }
        }

        song.duration = 0; //扫描阶段不从TagLib读时长；真正时长一般由QMediaPlayer在播放/加载媒体后更新（与 PlayerController 的职责划分一致，也避免扫描时解码开销）

        songs.append(song); //追加到结果列表
    }

    return songs;
}

QList<SongInfo> FileScanner::scanAllFiles(const QString &rootPath)
{
    QList<SongInfo> allSongs;

    const QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        return allSongs;
    }
    //先把根目录自身里的音频扫进去（与迭代器「子目录」互补，避免漏掉根上的文件）。
    allSongs.append(scanFiles(rootDir.absolutePath()));
    //深度优先遍历所有可读子目录路径。用 QDirIterator 遍历所有子目录，对每个目录调用 scanFiles，合并成一个大列表。
    QDirIterator it(
        rootDir.absolutePath(),
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDirIterator::Subdirectories
    );
    //每个子目录再调用 scanFiles 合并进 allSongs
    while (it.hasNext()) {
        const QString dirPath = it.next();
        allSongs.append(scanFiles(dirPath));
    }

    return allSongs;
}
    //只处理直接子文件夹：对每个子目录路径调用 hasAudioFiles，为真才加入列表。供 listwidget.cpp 刷新 m_subDirs，让用户在目录树里只看到「自己或子孙目录里至少有一个音频文件」的项。
QStringList FileScanner::scanSubDirs(const QString &dirPath)
{
    QStringList validSubDirs;    // 存放最终有效的目录

    const QDir rootDir(dirPath);
    if (!rootDir.exists()) {
        return validSubDirs;
    }
    // 获取【直接子文件夹】（不递归）
    const QFileInfoList subDirList = rootDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable
    );
    // 遍历每个子目录，调用 hasAudioFiles 检查是否包含音频文件。若为真，则加入 validSubDirs。
    for (const QFileInfo &subDirInfo : subDirList) {
        const QString subDirPath = subDirInfo.absoluteFilePath();
        if (hasAudioFiles(subDirPath)) {
            validSubDirs.append(subDirPath);
        }
    }

    return validSubDirs;
}

bool FileScanner::hasAudioFiles(const QString &dirPath)
{   //目录不存在 → 无音频。
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return false;
    }
    //列出目录下所有文件，符合音频扩展名的文件会被过滤出来。
    const QFileInfoList audioFiles = dir.entryInfoList(
        audioNameFilters(),
        QDir::Files | QDir::NoSymLinks | QDir::Readable
    );
    if (!audioFiles.isEmpty()) {
        return true;
    }
    //递归检查子目录：对每个子目录调用 hasAudioFiles，为真就返回 true。
    const QFileInfoList subDirList = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable
    );
    //递归！只要有一个子目录包含音乐，就返回 true
    for (const QFileInfo &subDirInfo : subDirList) {
        if (hasAudioFiles(subDirInfo.absoluteFilePath())) {
            return true;
        }
    }

    return false;
}
