#include "lrcparser.h"

#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

bool LrcParser::isValid(const QString &lrcPath)
{
    QFile file(lrcPath);
    if (!file.exists()) {
        return false;
    }
    const bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
    if (ok) {
        file.close();
    }
    return ok;
}

QMap<qint64, QString> LrcParser::parse(const QString &lrcPath)
{
    QMap<qint64, QString> result;

    QFile file(lrcPath);
    if (!file.exists()) {
        return result;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    const QRegularExpression timeTagRe(
        QStringLiteral(R"(\[(\d{1,2}):(\d{2})\.(\d{2,3})\])")
    );
    const QRegularExpression infoTagRe(
        QStringLiteral(R"(^\[(ti|ar|al|by|offset):.*\]$)"),
        QRegularExpression::CaseInsensitiveOption
    );

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (infoTagRe.match(line).hasMatch()) {
            continue;
        }

        const QRegularExpressionMatchIterator it = timeTagRe.globalMatch(line);
        QList<qint64> timestamps;
        auto iter = it;
        while (iter.hasNext()) {
            const QRegularExpressionMatch m = iter.next();
            const int mm = m.captured(1).toInt();
            const int ss = m.captured(2).toInt();
            const QString frac = m.captured(3);

            int ms = 0;
            if (frac.size() == 2) {
                ms = frac.toInt() * 10;
            } else {
                ms = frac.toInt();
            }

            const qint64 totalMs = static_cast<qint64>(mm) * 60 * 1000
                                 + static_cast<qint64>(ss) * 1000
                                 + static_cast<qint64>(ms);
            timestamps.append(totalMs);
        }

        if (timestamps.isEmpty()) {
            continue;
        }

        QString lyricText = line;
        lyricText.remove(timeTagRe);
        lyricText = lyricText.trimmed();
        if (lyricText.isEmpty()) {
            continue;
        }

        for (qint64 ts : timestamps) {
            result.insert(ts, lyricText);
        }
    }

    return result;
}
