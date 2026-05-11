#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

#include "aicontroller.h"
#include "playercontroller.h"
#include "songinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class VoiceInputWidget;
}
QT_END_NAMESPACE

class QPropertyAnimation;

class VoiceInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VoiceInputWidget(
        AiController *aiController,
        PlayerController *playerController,
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap,
        QWidget *parent = nullptr
    );
    ~VoiceInputWidget();
    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );
public slots:
    void onRecognizing();
    void onRecognizeFailed(const QString &error);
    void onDispatchResult(bool success, const QString &message);

signals:
    void playRequested(const QString &filePath);  //search 命中后请求按绝对路径播放；由 MusicPlayer 接到 ListWidget::playFromPath

private slots:
    void onSendClicked();

private:
    void toggleExpanded();
    void handleCommand(const QString &cmd, const QString &param);

    Ui::VoiceInputWidget *ui;
    AiController *m_aiController;
    PlayerController *m_playerController;
    QPropertyAnimation *m_animation;
    bool m_expanded;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
};
