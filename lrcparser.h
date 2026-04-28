#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QtGlobal>

class LrcParser : public QObject
{
public:
    static QMap<qint64, QString> parse(const QString &lrcPath);
    static bool isValid(const QString &lrcPath);
};
