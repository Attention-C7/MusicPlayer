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
    //短语类指令：一张 keyword→Action 表（含播放模式、随机、音量加减等）
    static bool matchBasic(const QString &input, Command &cmd);
    //搜索指令匹配：播放周杰伦的歌等（带捕获组，单独规则）
    static bool matchSearch(const QString &input, Command &cmd);
    //进度跳转：跳到某秒、快进/快退若干秒
    static bool matchSeek(const QString &input, Command &cmd);
    //音量设为具体数值
    static bool matchVolumeSet(const QString &input, Command &cmd);
    //构建短语关键词→Action 映射表（仅调用处 static 缓存）
    static QMap<QString, CommandAction> buildKeywordMap();
};
//全部static方法，无需实例化，直接LocalRuleEngine::match(input, cmd)调用
//const QString &引用传入，不拷贝字符串，效率更高
//Command &cmd非const引用，函数内部可以修改它
//private静态方法，封装实现，对外透明