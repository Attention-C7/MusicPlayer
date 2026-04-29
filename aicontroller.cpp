#include "aicontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {
bool matchExactWithTone(const QString &normalized, const QString &corePattern)
{
    const QRegularExpression re(
        QStringLiteral("^(?:%1)(?:一下|[啊吧呀呢嘛啦])?$").arg(corePattern)
    );
    return re.match(normalized).hasMatch();
}

bool matchLocalBasicCommand(const QString &input, QString &cmd, QString &param)
{
    const QString normalized = input.trimmed().toLower();
    if (normalized.isEmpty()) {
        return false;
    }

    if (matchExactWithTone(normalized, QStringLiteral("下一首|下一曲|next|下一个"))) {
        cmd = QStringLiteral("next");
        param.clear();
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("上一首|上一曲|prev|上一个|previous"))) {
        cmd = QStringLiteral("prev");
        param.clear();
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("播放|play|继续|resume"))) {
        cmd = QStringLiteral("play");
        param.clear();
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("暂停|pause|停止|stop|停一下"))) {
        cmd = QStringLiteral("pause");
        param.clear();
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("随机|shuffle|随机播放|乱序播放|随机模式"))) {
        cmd = QStringLiteral("mode");
        param = QStringLiteral("random");
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("单曲循环|单曲|repeat.?one|循环单曲"))) {
        cmd = QStringLiteral("mode");
        param = QStringLiteral("single");
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("目录循环|文件夹循环|列表循环|当前目录循环"))) {
        cmd = QStringLiteral("mode");
        param = QStringLiteral("folder");
        return true;
    }
    if (matchExactWithTone(normalized, QStringLiteral("全部循环|循环全部|全循环|repeat.?all"))) {
        cmd = QStringLiteral("mode");
        param = QStringLiteral("all");
        return true;
    }

    return false;
}
}

AiController::AiController(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_apiKey(QString::fromUtf8(qgetenv("DASHSCOPE_API_KEY")).trimmed())
{
}

void AiController::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap
)
{
    m_allSongs = allSongs;
    m_artistMap = artistMap;
    m_albumMap = albumMap;
}

bool AiController::recognize(QString userInput)
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
