#include "aicontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

AiController::AiController(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_apiKey(QString::fromUtf8(qgetenv("DASHSCOPE_API_KEY")).trimmed())
{
}

bool AiController::recognize(QString userInput)
{
    const QString normalized = userInput.trimmed().toLower();
    if (normalized.isEmpty()) {
        emit recognizeFailed(QStringLiteral("输入为空"));
        return true;
    }

    auto containsAny = [&normalized](const QStringList &keywords) {
        for (const QString &keyword : keywords) {
            if (normalized.contains(keyword)) {
                return true;
            }
        }
        return false;
    };

    if (containsAny({QStringLiteral("下一首"), QStringLiteral("下一曲"), QStringLiteral("next")})) {
        emit commandReady(QStringLiteral("next"), QString());
        return true;
    }
    if (containsAny({QStringLiteral("上一首"), QStringLiteral("上一曲"), QStringLiteral("prev"), QStringLiteral("previous")})) {
        emit commandReady(QStringLiteral("prev"), QString());
        return true;
    }
    if (containsAny({QStringLiteral("播放"), QStringLiteral("play"), QStringLiteral("继续")})) {
        emit commandReady(QStringLiteral("play"), QString());
        return true;
    }
    if (containsAny({QStringLiteral("暂停"), QStringLiteral("pause"), QStringLiteral("停")})) {
        emit commandReady(QStringLiteral("pause"), QString());
        return true;
    }
    if (containsAny({QStringLiteral("随机"), QStringLiteral("shuffle"), QStringLiteral("乱序")})) {
        emit commandReady(QStringLiteral("mode"), QStringLiteral("random"));
        return true;
    }
    if (containsAny({QStringLiteral("单曲循环"), QStringLiteral("repeat one")})) {
        emit commandReady(QStringLiteral("mode"), QStringLiteral("single"));
        return true;
    }
    if (containsAny({QStringLiteral("目录循环"), QStringLiteral("列表循环")})) {
        emit commandReady(QStringLiteral("mode"), QStringLiteral("folder"));
        return true;
    }
    if (containsAny({QStringLiteral("全部循环"), QStringLiteral("循环全部")})) {
        emit commandReady(QStringLiteral("mode"), QStringLiteral("all"));
        return true;
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
        QStringLiteral("你是音乐播放器的语音控制助手，用户会输入自然语言指令，你只能返回以下JSON格式之一，不要返回任何其他内容："
                       "{\"cmd\":\"play\"}"
                       "{\"cmd\":\"pause\"}"
                       "{\"cmd\":\"prev\"}"
                       "{\"cmd\":\"next\"}"
                       "{\"cmd\":\"mode\",\"param\":\"single\"}"
                       "{\"cmd\":\"mode\",\"param\":\"folder\"}"
                       "{\"cmd\":\"mode\",\"param\":\"all\"}"
                       "{\"cmd\":\"mode\",\"param\":\"random\"}"
                       "{\"cmd\":\"search\",\"param\":\"歌曲名\"}"
                       "{\"cmd\":\"unknown\"}")
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
    timeoutTimer->start(8000);
    connect(timeoutTimer, &QTimer::timeout, this, [this, reply, timeoutTimer]() {
        if (reply->isRunning()) {
            reply->setProperty("timeout_aborted", true);
            reply->abort();
        }
        timeoutTimer->deleteLater();
        emit recognizeFailed(QStringLiteral("请求超时，请重试"));
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
}

QString AiController::sanitizeJsonText(const QString &text) const
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
}
