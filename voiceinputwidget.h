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
class QResizeEvent;
class VoiceChatDelegate;
class MicOverlayWidget;

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
    ~VoiceInputWidget() override;

    void setSearchContext(
        QList<SongInfo> allSongs,
        QMap<QString, QList<SongInfo>> artistMap,
        QMap<QString, QList<SongInfo>> albumMap
    );

    /** 父级 PlayWidget 尺寸变化时同步底部抽屉几何（含滑入/收起目标矩形）。 */
    void applyDrawerGeometry(int playWidgetWidth, int playWidgetHeight);

public slots:
    void onRecognizing();
    void onRecognizeFailed(const QString &error);
    void onDispatchResult(bool success, const QString &message);

signals:
    void playRequested(const QString &filePath);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSendClicked();
    void onToggleDrawerClicked();
    void onPillNextClicked();
    void onPillPauseClicked();
    void onPillShuffleClicked();
    void updateMicOverlayGeometry();

private:
    void appendChatMessage(bool isUser, const QString &text);
    QRect computeDrawerGeometry() const;
    void setConversationVisible(bool visible);
    static QIcon makeSendIcon();

    Ui::VoiceInputWidget *ui;
    AiController *m_aiController;
    PlayerController *m_playerController;
    VoiceChatDelegate *m_chatDelegate;
    MicOverlayWidget *m_micOverlay;
    QPropertyAnimation *m_drawerGeomAnim;
    bool m_drawerOpen;
    int m_playW;
    int m_playH;
    QList<SongInfo> m_allSongs;
    QMap<QString, QList<SongInfo>> m_artistMap;
    QMap<QString, QList<SongInfo>> m_albumMap;
};
