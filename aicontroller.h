#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include "command.h"
#include "commandvalidator.h"
#include "commanddispatcher.h"
#include "localruleengine.h"
#include "songinfo.h"

class PlayerController;

class AiController : public QObject
{
    Q_OBJECT

public:
    explicit AiController(PlayerController *controller, QObject *parent = nullptr);
    // 唯一对外接口
    // 返回true=本地命中 false=走网络bool recognize(QString userInput);
    bool recognize(const QString &input);

    // 暴露dispatcher供外部连接信号
    CommandDispatcher *dispatcher() const;

public slots:
    // 接收搜索上下文
    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap);

signals:
    // 保留给VoiceInputWidget显示状态
    void recognizing();           // 开始联网识别
    void recognizeFailed(QString error);

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onLlmRequestTimeout();

private:
    // 发送HTTP请求
    void sendToLLM(const QString &input);

    // 解析LLM返回的JSON → Command
    Command parseCommandFromJson(const QJsonObject &obj);

    // action字符串 → CommandAction枚举
    CommandAction actionStringToEnum(const QString &actionStr);

    // 处理解析后的Command（验证+调度）
    void processCommand(const Command &cmd);

    // 清理JSON中的markdown标记
    QString sanitizeJson(const QString &raw);

    QNetworkAccessManager *m_manager;
    CommandDispatcher *m_dispatcher;
    QString m_apiKey;
    
    //记录当前网络请求的输入,用于超时后二次本地匹配
    QString m_pendingInput;
};
