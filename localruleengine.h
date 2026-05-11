#pragma once

#include <QString>
#include <QMap>  //存关键词→Action映射表
#include <QRegularExpression>  //正则表达式，用于匹配关键词
#include "command.h"

class LocalRuleEngine {
public:
    //唯一对外接口
    //input:用户输入的原始字符串
    //cmd:引用传出，命中时填充
    static bool match(const QString &input, Command &cmd);
private:
    //基础指令匹配：下一首/上一首/播放/暂停/随机/单曲循环
    static bool matchBasic(const QString &input, Command &cmd);
    //搜索指令匹配：播放周杰伦的歌等
    static bool matchSearch(const QString &input, Command &cmd);
    //构建关键词→Action映射表
    //只调用一次，结果复用
    static QMap<QString, CommandAction> buildKeywordMap();
};
//全部static方法，无需实例化，直接LocalRuleEngine::match(input, cmd)调用
//const QString &引用传入，不拷贝字符串，效率更高
//Command &cmd非const引用，函数内部可以修改它
//private静态方法，封装实现，对外透明