#pragma once

#include <QString>

#include "command.h"

class CommandValidator{
public:
    //验证结果结构体
    struct Result{
        bool valid = false;
        QString reason; //失败原因
    };

    //唯一对外接口
    static Result validate(const Command &cmd);
private:
    //验证action是否合法
    static bool validateAction(const Command &cmd);
    //验证target是否合法
    static bool validateTarget(const Command &cmd, QString &reason);
    //验证params是否合法
    static bool validateParams(const Command &cmd, QString &reason);
};
//嵌套struct Result让返回值更清晰
//比返回bool多了reason说明失败原因
//QString &reason引用传出错误信息