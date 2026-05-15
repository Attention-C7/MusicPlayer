#include "localruleengine.h"

#include <QList>

QMap<QString, CommandAction> LocalRuleEngine::buildKeywordMap(){
    QMap<QString, CommandAction> map;

    //格式说明：key=正则字符串，value=对应的CommandAction枚举
    map["下一首|下一曲|next|下一个"] = CommandAction::PlaybackNext;
    map["上一首|上一曲|prev|previous|上一个"] = CommandAction::PlaybackPrev;
    map["播放|play|继续|resume"] = CommandAction::PlaybackPlay;
    map["暂停|pause|停止|stop|停一下"] = CommandAction::PlaybackPause;
    map["随机|shuffle|随机播放|乱序播放|随机模式"] = CommandAction::PlaylistShuffle;
    map["单曲循环|单循|单曲重复|single loop|repeat one"] = CommandAction::PlaylistLoopSingle;
    map["列表循环|全部循环|顺序播放|循环列表|all loop|loop all"] = CommandAction::PlaylistLoopAll;
    map["文件夹循环|目录循环|folder loop"] = CommandAction::PlaylistLoopFolder;
    map["列表|打开列表|显示列表|侧边列表|歌曲列表|音乐列表|open list|show list"] = CommandAction::UiShowList;
    map["返回播放|关闭列表|回到播放|收起列表|隐藏列表|退出列表|返回播放器|close list|hide list|back to player"] = CommandAction::UiHideList;
    map["音量大|音量调大|大声|louder|音量加"] = CommandAction::VolumeUp;
    map["音量小|音量调小|小声|quieter|音量减"] = CommandAction::VolumeDown;
    map["静音|关掉声音|关闭声音|无声|mute"] = CommandAction::VolumeMute;
    map["取消静音|打开声音|恢复声音|解除静音|unmute"] = CommandAction::VolumeUnmute;

    return map;
}
//key是候选词用|分隔，后面matchBasic()用它构建正则
//每次调用buildKeywordMap()都会重建map,后面会用static局部变量优化

bool LocalRuleEngine::matchBasic(const QString &input, Command &cmd){
    //预处理输出：去空格、转小写
    QString s = input.trimmed().toLower();

    //语气词后缀正则，允许末尾带一个语气词
    const QString suffix = QStringLiteral("(一下|[啊吧呀呢嘛啦])?");

    //获取关键词表（static保证只构建一次）
    static const QMap<QString, CommandAction> keywordMap = buildKeywordMap();

    //遍历关键词表
    for(auto it = keywordMap.constBegin(); it != keywordMap.constEnd(); ++it){
        //构建完整正则：^（关键词）（语气词）$
        QString pattern =
            QStringLiteral("^(") +
            it.key() +
            QStringLiteral(")") +
            suffix +
            QStringLiteral("$");

        QRegularExpression re(pattern);
        QRegularExpressionMatch m = re.match(s);

        if(m.hasMatch()){
            //命中,填充cmd
            cmd.action = it.value();
            cmd.source = QStringLiteral("local_rule");
            cmd.confidence = 1.0f;
            cmd.valid = true;
            return true;
        }
    }

    return false;
}
//static const 局部变量：函数第一次调用初始化，之后复用，避免每次重建map
//constBegin/constEnd:只读遍历，效率更高
//QStringLiteral编译期构建字符串，比直接写字符串更高效

bool LocalRuleEngine::matchSearch(const QString &input, Command &cmd){
    QString s = input.trimmed();
    //rule1:播放XX的歌 → 按歌手搜索
    {
        QRegularExpression re(QStringLiteral("^播放(.+)的歌$"));
        QRegularExpressionMatch m = re.match(s);
        if(m.hasMatch()){
            cmd.action = CommandAction::MusicPlay;
            cmd.target.type = QStringLiteral("artist");
            cmd.target.keyword = m.captured(1).trimmed();
            cmd.source = QStringLiteral("local_rule");
            cmd.confidence = 1.0f;
            cmd.valid = true;
            return true;
        }
    }
    //rule1b:播放XX专辑 → 按专辑
    {
        QRegularExpression re(QStringLiteral("^播放(.+)专辑$"));
        QRegularExpressionMatch m = re.match(s);
        if(m.hasMatch()){
            cmd.action = CommandAction::MusicPlay;
            cmd.target.type = QStringLiteral("album");
            cmd.target.keyword = m.captured(1).trimmed();
            cmd.source = QStringLiteral("local_rule");
            cmd.confidence = 1.0f;
            cmd.valid = true;
            return true;
        }
    }
    //rule2:播放XX → 按歌曲名搜索
    {
        QRegularExpression re(QStringLiteral("^播放(.+)$"));
        QRegularExpressionMatch m = re.match(s);
        if(m.hasMatch()){
            cmd.action = CommandAction::MusicPlay;
            cmd.target.type = QStringLiteral("title");
            cmd.target.keyword = m.captured(1).trimmed();
            cmd.source = QStringLiteral("local_rule");
            cmd.confidence = 0.9f;
            cmd.valid = true;
            return true;
        }
    }
    //rule3:搜索XX
    {
        QRegularExpression re(QStringLiteral("^搜索(.+)$"));
        QRegularExpressionMatch m = re.match(s);
        if(m.hasMatch()){
            cmd.action = CommandAction::MusicSearch;
            cmd.target.type = QStringLiteral("title");
            cmd.target.keyword = m.captured(1).trimmed();
            cmd.source = QStringLiteral("local_rule");
            cmd.confidence = 1.0f;
            cmd.valid = true;
            return true;
        }
    }
    return false;
}
//m.captured(1):获取第一个捕获组（括号内内容）
//rule1置信度1，rule2置信度0.9，因为歌曲匹配歧义更大
//大括号隔开每条规则，限制变量作用域，避免命名冲突

bool LocalRuleEngine::matchSeek(const QString &input, Command &cmd)
{
    const QString s = input.trimmed();

    auto fillSeek = [&cmd](qint64 positionMs, qint64 offsetMs, bool usePosition) {
        cmd.action = CommandAction::PlaybackSeek;
        cmd.params.clear();
        if (usePosition) {
            cmd.params.insert(QStringLiteral("position"), positionMs);
        } else {
            cmd.params.insert(QStringLiteral("offsetMs"), offsetMs);
        }
        cmd.source = QStringLiteral("local_rule");
        cmd.confidence = 1.0f;
        cmd.valid = true;
    };

    {
        const QRegularExpression re(QStringLiteral("^跳到(\\d+)秒$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 sec = m.captured(1).toLongLong(&ok);
            if (!ok || sec < 0) {
                return false;
            }
            fillSeek(sec * 1000, 0, true);
            return true;
        }
    }
    {
        const QRegularExpression re(QStringLiteral("^跳到(\\d+)分(\\d+)秒$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok1 = false;
            bool ok2 = false;
            const qint64 min = m.captured(1).toLongLong(&ok1); 
            const qint64 sec = m.captured(2).toLongLong(&ok2);
            if (!ok1 || !ok2 || min < 0 || sec < 0 || sec >= 60) {
                return false;
            }
            fillSeek((min * 60 + sec) * 1000, 0, true);
            return true;
        }
    }
    {
        const QRegularExpression re(QStringLiteral("^跳到(\\d+)分钟$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 min = m.captured(1).toLongLong(&ok);
            if (!ok || min < 0) {
                return false;
            }
            fillSeek(min * 60 * 1000, 0, true);
            return true;
        }
    }
    {
        const QRegularExpression re(QStringLiteral("^快进(\\d+)秒$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 sec = m.captured(1).toLongLong(&ok);
            if (!ok || sec < 0) {
                return false;
            }
            fillSeek(0, sec * 1000, false);
            return true;
        }
    }
    {
        const QRegularExpression re(QStringLiteral("^快退(\\d+)秒$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 sec = m.captured(1).toLongLong(&ok);
            if (!ok || sec < 0) {
                return false;
            }
            fillSeek(0, -sec * 1000, false);
            return true;
        }
    }
    {
        const QRegularExpression re(QStringLiteral("^后退(\\d+)秒$"));
        const QRegularExpressionMatch m = re.match(s);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 sec = m.captured(1).toLongLong(&ok);
            if (!ok || sec < 0) {
                return false;
            }
            fillSeek(0, -sec * 1000, false);
            return true;
        }
    }
    return false;
}

bool LocalRuleEngine::matchVolumeSet(const QString &input, Command &cmd)
{
    const QString s = input.trimmed();
    const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("^音量(\\d{1,3})%?$")),
        QRegularExpression(QStringLiteral("^音量调到(\\d{1,3})%?$")),
        QRegularExpression(QStringLiteral("^音量设置为(\\d{1,3})%?$")),
    };
    for (const QRegularExpression &re : patterns) {
        const QRegularExpressionMatch m = re.match(s);
        if (!m.hasMatch()) {
            continue;
        }
        bool ok = false;
        const int vol = m.captured(1).toInt(&ok);
        if (!ok || vol < 0 || vol > 100) {
            return false;
        }
        cmd.action = CommandAction::VolumeSet;
        cmd.params.clear();
        cmd.params.insert(QStringLiteral("volume"), vol);
        cmd.source = QStringLiteral("local_rule");
        cmd.confidence = 1.0f;
        cmd.valid = true;
        return true;
    }
    return false;
}

bool LocalRuleEngine::match(const QString &input, Command &cmd){
    //短语类指令（含播放模式、随机等）
    if(matchBasic(input, cmd)){
        return true;
    }
    if (matchSeek(input, cmd)) {
        return true;
    }
    if (matchVolumeSet(input, cmd)) {
        return true;
    }
    //再尝试搜索指令匹配
    if(matchSearch(input, cmd)){
        return true;
    }
    //都失败
    return false;
}
//入口函数只做分发，逻辑清晰
//顺序：basic优先，避免“播放”被Search规则误匹配