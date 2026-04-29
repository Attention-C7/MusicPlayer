#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>

class AiController : public QObject
{
    Q_OBJECT

public:
    explicit AiController(QObject *parent = nullptr);
    void recognize(QString userInput);

signals:
    void commandReady(QString command, QString param);
    void recognizeFailed(QString error);

private:
    QString sanitizeJsonText(const QString &text) const;

    QNetworkAccessManager *m_manager;
    QString m_apiKey;
};
