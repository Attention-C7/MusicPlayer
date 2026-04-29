#pragma once

#include <QWidget>

#include "aicontroller.h"
#include "playercontroller.h"

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
    explicit VoiceInputWidget(AiController *aiController, PlayerController *playerController, QWidget *parent = nullptr);
    ~VoiceInputWidget();

private:
    void toggleExpanded();
    void handleCommand(const QString &cmd, const QString &param);

    Ui::VoiceInputWidget *ui;
    AiController *m_aiController;
    PlayerController *m_playerController;
    QPropertyAnimation *m_animation;
    bool m_expanded;
};
