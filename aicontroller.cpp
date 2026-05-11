#include "aicontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVariant>

AiController::AiController(PlayerController *controller, QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    ,m_dispatcher(new CommandDispatcher(controller, this))
    ,m_apiKey(QString::fromUtf8(qgetenv("DASHSCOPE_API_KEY")))
{
    //网络响应统一走onNetworkReply
    connect(m_manager, &QNetworkAccessManager::finished, this, &AiController::onNetworkReply);
}
//CommandDispatcher在这里创建，持有controller引用
//qgetenv从环境变量中读Key,不写死在代码里
//finished信号在网络请求完成时触发

CommandDispatcher *AiController::dispatcher() const
{
    return m_dispatcher;
}

void AiController::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap)
{
    //转发给dispatcher
    m_dispatcher->setSearchContext(allSongs, artistMap, albumMap);
}
//AiController不关心上下文，只负责识别和调度,直接透传给dispatcher

bool AiController::recognize(const QString &input){
    if (input.trimmed().isEmpty()){
        return false;
    }

    //step1:本地规则匹配
    Command cmd;
    if (LocalRuleEngine::match(input, cmd)){
        //本地命中，走验证+调度
        processCommand(cmd);
        return true;
    }

    //step2:联网识别
    m_pendingInput = input;
    sendToLLM(input);
    emit recognizing();
    return false;
}
//流程极简：本地匹配，成功则processCommand,否则sendToLLM
//m_pendingInput保存输入，网络超时后可以二次本地匹配
//emit recognizing通知UI显示“联网识别中...”

void AiController::processCommand(const Command &cmd){
    CommandValidator::Result result = CommandValidator::validate(cmd);
    if (!result.valid){
        emit recognizeFailed(result.reason);
        return;
    }

    m_dispatcher->dispatch(cmd);
}
//验证+调度的统一入口
//本地命中和网络返回都走这一个方法
//保证行为一致性

void AiController::sendToLLM(const QString &input){
    if (m_apiKey.isEmpty()){
        emit recognizeFailed(QStringLiteral("未配置DASHSCOPE_API_KEY"));
        return;
    }

    //构建请求
    QNetworkRequest request(QUrl(QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());

    //system prompt:告诉LLM只返回结构化JSON
    const QString systemPrompt = QStringLiteral(
        "你是音乐播放器语音控制助手。"
        "用户输入自然语言,你返回JSON指令。\n"
        "支持的action:\n"
        "playback.next / playback.prev / playback.play / playback.pause\n"
        "playback.seek（params.position 为毫秒绝对位置，或 params.offsetMs 为相对当前进度的毫秒偏移）\n"
        "music.play / music.search（需 target：type 为 artist / album / title 之一，keyword 为关键词）\n"
        "volume.up / volume.down / volume.set（params.volume 为 0-100 整数）\n"
        "playlist.shuffle / playlist.loop_single / playlist.loop_all / playlist.loop_folder\n"
        "ui.show_list（打开侧栏列表，等同点「列表」）/ ui.hide_list（返回全屏播放，等同列表内返回）\n"
        "格式示例：{\"version\":\"1.0\",\"action\":\"volume.set\","
        "\"params\":{\"volume\":50},\"source\":\"llm\",\"confidence\":0.9}\n"
        "无对应字段可省略 target/params。只返回JSON，不要解释。"
    );

    //构建请求Body
    QJsonObject body;
    body["model"] = QStringLiteral("qwen-turbo");
    body["max_tokens"] = 200;

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg["role"] = QStringLiteral("system");
    sysMsg["content"] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = QStringLiteral("user");
    userMsg["content"] = input;
    messages.append(userMsg);

    body["messages"] = messages;

    //发送请求
    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    //10s超时
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setProperty("llmReply", QVariant::fromValue(static_cast<QObject *>(reply)));
    timer->start(10000);

    connect(timer, &QTimer::timeout, this, &AiController::onLlmRequestTimeout);

    // reply完成时停止timer
    connect(reply, &QNetworkReply::finished, timer, &QTimer::stop);
}
//QNetworkRequest 构建HTTP请求头
//QJsonDocument(body).toJson() 序列化为JSON字节流
//QTimer::singleShot 10秒后触发超时处理
//超时时先尝试二次本地匹配，再才报错

void AiController::onNetworkReply(QNetworkReply *reply){
    reply->deleteLater();

    //检查网络错误
    if (reply->error() != QNetworkReply::NoError){
        //abort触发的error不再重复报错
        if (reply->error() != QNetworkReply::OperationCanceledError){
            emit recognizeFailed(QStringLiteral("网络错误：") + reply->errorString());
        }
        return;
    }

    //读取响应
    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError){
        emit recognizeFailed(QStringLiteral("响应解析失败"));
        return;
    }

    //提取content文本
    //格式：choices[0].message.content
    QJsonObject root = doc.object();
    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()){
        emit recognizeFailed(QStringLiteral("响应格式异常"));
        return;
    }

    QString content = choices[0].toObject()["message"].toObject()["content"].toString();

    //清理markdown标记
    content = sanitizeJson(content);

    //解析Command JSON
    QJsonDocument cmdDoc = QJsonDocument::fromJson(content.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError){
        emit recognizeFailed(QStringLiteral("指令解析失败"));
        return;
    }

    //构建Command对象
    Command cmd = parseCommandFromJson(cmdDoc.object());

    //走统一验证+调度流程
    processCommand(cmd);
}

void AiController::onLlmRequestTimeout()
{
    auto *timer = qobject_cast<QTimer *>(sender());
    if (timer == nullptr) {
        return;
    }

    auto *reply = qobject_cast<QNetworkReply*>(
        timer->property("llmReply")
              .value<QObject*>());
    timer->deleteLater();

    if (reply == nullptr) {
        return;
    }

    if (reply->isRunning()) {
        reply->abort();
        Command fallback;
        if (LocalRuleEngine::match(m_pendingInput, fallback)) {
            processCommand(fallback);
        } else {
            emit recognizeFailed(QStringLiteral("网络超时，已尝试本地处理"));
        }
    }
}

Command AiController::parseCommandFromJson(const QJsonObject &obj){
    Command cmd;

    //解析action
    cmd.action = actionStringToEnum(obj["action"].toString());

    //解析target，可能为空
    if (obj.contains("target")){
        QJsonObject t = obj["target"].toObject();
        cmd.target.type = t["type"].toString();
        cmd.target.keyword = t["keyword"].toString();
    }

    //解析params
    if (obj.contains("params")){
        cmd.params = obj["params"].toObject().toVariantMap();
    }

    //解析元信息
    cmd.source = obj["source"].toString("LLM");
    cmd.confidence = static_cast<float>(obj["confidence"].toDouble(0.8));
    cmd.valid = true;

    return cmd;
}
//obj.contains() 先判断key存在再取值，防止崩溃
//toObject().toVariantMap() QJsonObject转QVariantMap
//toDouble(0.8) 第二个参数是默认值

CommandAction AiController::actionStringToEnum(const QString &actionStr){
    //LLM返回的action字符串映射到枚举
    static const QMap<QString, CommandAction> actionMap = {
        {"playback.next", CommandAction::PlaybackNext},
        {"playback.prev", CommandAction::PlaybackPrev},
        {"playback.play", CommandAction::PlaybackPlay},
        {"playback.pause", CommandAction::PlaybackPause},
        {"playback.seek", CommandAction::PlaybackSeek},
        {"music.play", CommandAction::MusicPlay},
        {"music.search", CommandAction::MusicSearch},
        {"volume.up", CommandAction::VolumeUp},
        {"volume.down", CommandAction::VolumeDown},
        {"volume.set", CommandAction::VolumeSet},
        {"playlist.shuffle", CommandAction::PlaylistShuffle},
        {"playlist.loop_single", CommandAction::PlaylistLoopSingle},
        {"playlist.loop_all", CommandAction::PlaylistLoopAll},
        {"playlist.loop_folder", CommandAction::PlaylistLoopFolder},
        {"ui.show_list", CommandAction::UiShowList},
        {"ui.hide_list", CommandAction::UiHideList},
    };

    return actionMap.value(actionStr.toLower(), CommandAction::Unknown);
}
//static const 只初始化一次，复用
//C++11初始化列表语法构建QMap
//QMap::value(key, defaultValue) 找不到时返回Unknown

QString AiController::sanitizeJson(const QString &raw){
    QString cleaned = raw.trimmed();

    if (cleaned.startsWith(QStringLiteral("```"))) {
        cleaned = cleaned.mid(3).trimmed();
    }else if (cleaned.endsWith(QStringLiteral("```json"))){
        cleaned = cleaned.mid(7).trimmed();
    }
    if (cleaned.endsWith(QStringLiteral("```"))){
        cleaned.chop(3);
        cleaned = cleaned.trimmed();
    }
    return cleaned;
}

/*bool AiController::recognize(const QString &userInput)
{
    const QString trimmedInput = userInput.trimmed();
    if (trimmedInput.isEmpty()) {
        emit recognizeFailed(QStringLiteral("输入为空"));
        return true;
    }

    QString localCmd;
    QString localParam;
    if (matchLocalBasicCommand(trimmedInput, localCmd, localParam)) {
        emit commandReady(localCmd, localParam);
        return true;
    }

    auto findSongByKeyword = [this](const QString &keyword) -> QString {
        const QString k = keyword.trimmed();
        if (k.isEmpty()) {
            return QString();
        }

        for (const SongInfo &song : m_allSongs) {
            if (song.title.contains(k, Qt::CaseInsensitive)) {
                return song.filePath;
            }
        }

        for (auto it = m_artistMap.cbegin(); it != m_artistMap.cend(); ++it) {
            if (it.key().contains(k, Qt::CaseInsensitive) && !it.value().isEmpty()) {
                return it.value().first().filePath;
            }
        }

        return QString();
    };

    auto findByArtistKeyword = [this](const QString &keyword) -> QString {
        const QString k = keyword.trimmed();
        if (k.isEmpty()) {
            return QString();
        }
        for (auto it = m_artistMap.cbegin(); it != m_artistMap.cend(); ++it) {
            if (it.key().contains(k, Qt::CaseInsensitive) && !it.value().isEmpty()) {
                return it.value().first().filePath;
            }
        }
        return QString();
    };

    {
        const QRegularExpression rePlayArtist(QStringLiteral("^播放(.+)的歌$"));
        const QRegularExpressionMatch match = rePlayArtist.match(trimmedInput);
        if (match.hasMatch()) {
            const QString keyword = match.captured(1).trimmed();
            const QString filePath = findByArtistKeyword(keyword);
            if (!filePath.isEmpty()) {
                emit commandReady(QStringLiteral("search"), filePath);
                return true;
            }
        }
    }

    {
        const QRegularExpression rePlayAny(QStringLiteral("^播放(.+)$"));
        const QRegularExpressionMatch match = rePlayAny.match(trimmedInput);
        if (match.hasMatch()) {
            const QString keyword = match.captured(1).trimmed();
            if (!keyword.isEmpty()) {
                const QString filePath = findSongByKeyword(keyword);
                if (!filePath.isEmpty()) {
                    emit commandReady(QStringLiteral("search"), filePath);
                    return true;
                }
            }
        }
    }

    {
        const QRegularExpression reSearchAny(QStringLiteral("^搜索(.+)$"));
        const QRegularExpressionMatch match = reSearchAny.match(trimmedInput);
        if (match.hasMatch()) {
            const QString keyword = match.captured(1).trimmed();
            if (!keyword.isEmpty()) {
                const QString filePath = findSongByKeyword(keyword);
                if (!filePath.isEmpty()) {
                    emit commandReady(QStringLiteral("search"), filePath);
                    return true;
                }
            }
        }
    }

    if (m_apiKey.isEmpty()) {
        emit recognizeFailed(QStringLiteral("未配置 DASHSCOPE_API_KEY"));
        return false;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), QStringLiteral("qwen-turbo"));
    payload.insert(QStringLiteral("max_tokens"), 100);

    QJsonArray messages;
    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMessage.insert(
        QStringLiteral("content"),
        QStringLiteral(
            "你是音乐播放器助手，用户会用自然语言描述他想要的操作，你需要理解意图并返回播放器能执行的操作指令JSON。"
            "可用指令："
            "播放/暂停：{\"cmd\":\"play\"} {\"cmd\":\"pause\"}；"
            "切歌：{\"cmd\":\"next\"} {\"cmd\":\"prev\"}；"
            "模式：{\"cmd\":\"mode\",\"param\":\"single/folder/all/random\"}；"
            "搜索：{\"cmd\":\"search\",\"param\":\"搜索关键词\"}；"
            "无法执行：{\"cmd\":\"unknown\",\"reason\":\"原因说明\"}。"
            "请根据用户意图选择最合适的指令返回。不要解释，只返回JSON。"
        )
    );
    messages.append(systemMessage);

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), userInput);
    messages.append(userMessage);

    payload.insert(QStringLiteral("messages"), messages);

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->start(10000);
    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, timeoutTimer, trimmedInput]() {
        if (reply->isRunning()) {
            reply->setProperty("timeout_aborted", true);
            reply->abort();
        }
        timeoutTimer->deleteLater();

        QString cmd;
        QString param;
        const bool matched = matchLocalBasicCommand(trimmedInput, cmd, param);
        emit recognizeFailed(QStringLiteral("未能及时响应，已尝试本地处理"));
        if (matched) {
            emit commandReady(cmd, param);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->property("timeout_aborted").toBool()) {
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            emit recognizeFailed(reply->errorString());
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDoc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
            emit recognizeFailed(QStringLiteral("响应JSON解析失败"));
            return;
        }

        const QJsonArray choices = responseDoc.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty() || !choices.first().isObject()) {
            emit recognizeFailed(QStringLiteral("响应内容为空"));
            return;
        }

        const QJsonObject messageObj = choices.first().toObject().value(QStringLiteral("message")).toObject();
        const QString text = messageObj.value(QStringLiteral("content")).toString();
        const QString cleaned = sanitizeJsonText(text);
        if (cleaned.isEmpty()) {
            emit recognizeFailed(QStringLiteral("响应指令为空"));
            return;
        }

        QJsonParseError cmdParseError;
        const QJsonDocument cmdDoc = QJsonDocument::fromJson(cleaned.toUtf8(), &cmdParseError);
        if (cmdParseError.error != QJsonParseError::NoError || !cmdDoc.isObject()) {
            emit recognizeFailed(QStringLiteral("指令JSON解析失败"));
            return;
        }

        const QJsonObject cmdObj = cmdDoc.object();
        const QString cmd = cmdObj.value(QStringLiteral("cmd")).toString();
        const QString param = cmdObj.value(QStringLiteral("param")).toString();

        if (cmd.isEmpty()) {
            emit recognizeFailed(QStringLiteral("指令缺少 cmd 字段"));
            return;
        }

        emit commandReady(cmd, param);
    });

    connect(reply, &QNetworkReply::finished, this, [timeoutTimer]() {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
    });

    return false;
}*/

/*QString AiController::sanitizeJsonText(const QString &text) const
{
    QString cleaned = text.trimmed();
    if (cleaned.startsWith(QStringLiteral("```"))) {
        const int firstLineEnd = cleaned.indexOf('\n');
        if (firstLineEnd >= 0) {
            cleaned = cleaned.mid(firstLineEnd + 1).trimmed();
        } else {
            cleaned.clear();
        }
    }
    if (cleaned.endsWith(QStringLiteral("```"))) {
        cleaned.chop(3);
        cleaned = cleaned.trimmed();
    }
    return cleaned;
}*/
