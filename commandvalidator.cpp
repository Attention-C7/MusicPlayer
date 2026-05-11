#include "commandvalidator.h"

bool CommandValidator::validateAction(const Command &cmd){
    // Unknown是兜底值，说明解析失败
    return cmd.action != CommandAction::Unknown;
}
//只要不是Unknown就合法

bool CommandValidator::validateTarget(const Command &cmd, QString &reason)
{
    // 只有音乐类指令才需要target
    if (!cmd.isMusicAction()) {
        return true;
    }

    // MusicPlay/MusicSearch必须有keyword
    if (cmd.target.isEmpty()) {
        reason = QStringLiteral("音乐指令缺少搜索关键词");
        return false;
    }

    // keyword长度合理性检查
    if (cmd.target.keyword.length() > 50) {
        reason = QStringLiteral("搜索关键词过长");
        return false;
    }

    return true;
}
//先判断是否需要验证，不需要直接返回true
//isEmpty() 是在CommandTarget里写的便捷方法
//长度检查防止异常输入

bool CommandValidator::validateParams(const Command &cmd, QString &reason)
{
    // VolumeSet需要有volume参数
    if (cmd.action == CommandAction::VolumeSet) {
        if (!cmd.params.contains(QStringLiteral("volume"))) {
            reason = QStringLiteral("音量设置指令缺少volume参数");
            return false;
        }

        // volume值范围检查 0-100
        int vol = cmd.params
                     .value(QStringLiteral("volume"))
                     .toInt();
        if (vol < 0 || vol > 100) {
            reason = QStringLiteral("音量值超出范围(0-100)");
            return false;
        }
    }

    // PlaybackSeek需要有position参数
    if (cmd.action == CommandAction::PlaybackSeek) {
        if (!cmd.params.contains(
                QStringLiteral("position"))) {
            reason = QStringLiteral("跳转指令缺少position参数");
            return false;
        }
    }

    return true;
}
//QVariantMap::contains() 检查key是否存在
//QVariantMap::value().toInt() 取值并转类型
//目前只验证两种有参数要求的指令，后续扩展加case即可

CommandValidator::Result CommandValidator::validate(const Command &cmd)
{
    Result result;

    // action合法性
    if (!validateAction(cmd)) {
        result.valid  = false;
        result.reason = QStringLiteral("无法识别的指令类型");
        return result;
    }

    // target完整性
    if (!validateTarget(cmd, result.reason)) {
        result.valid = false;
        return result;
    }

    // params合理性
    if (!validateParams(cmd, result.reason)) {
        result.valid = false;
        return result;
    }

    // 全部通过
    result.valid  = true;
    result.reason = QString();
    return result;
}
//链式验证，任一失败立刻return
//result.reason 由私有方法填充，主入口不需要知道细节
//最后成功时显式清空reason