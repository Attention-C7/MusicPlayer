#include "localruleengine.h"

QMap<QString, CommandAction> LocalRuleEngine::buildKeywordMap(){
    QMap<QString, CommandAction> map;

    //格式说明：key=正则字符串，value=对应的CommandAction枚举
    map["下一首|下一曲|next|下一个"] = CommandAction::PlaybackNext;
    map["上一首|上一曲|prev|previous|上一个"] = CommandAction::PlaybackPrev;
    map["播放|play|继续|resume"] = CommandAction::PlaybackPlay;
    map["暂停|pause|停止|stop|停一下"] = CommandAction::PlaybackPause;
    map["随机|shuffle|随机播放|乱序播放|随机模式"] = CommandAction::PlaylistShuffle;
    map["音量大|音量调大|大声|louder|音量加"] = CommandAction::VolumeUp;
    map["音量小|音量调小|小声|quieter|音量减"] = CommandAction::VolumeDown;

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

bool LocalRuleEngine::match(const QString &input, Command &cmd){
    //优先尝试基础指令匹配
    if(matchBasic(input, cmd)){
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