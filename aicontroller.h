#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QList>
#include <QMap>

#include "songinfo.h"

class AiController : public QObject
{
    Q_OBJECT

public:
    explicit AiController(QObject *parent = nullptr);
    bool recognize(QString userInput);
    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );

signals:
    void commandReady(QString command, QString param);
    void recognizeFailed(QString error);

private:
    QString sanitizeJsonText(const QString &text) const;

    QNetworkAccessManager *m_manager;
    QString m_apiKey;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
};
