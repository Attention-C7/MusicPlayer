#include "voiceinputwidget.h"
#include "ui_voiceinputwidget.h"

#include <QPropertyAnimation>

VoiceInputWidget::VoiceInputWidget(AiController *aiController, PlayerController *playerController, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VoiceInputWidget)
    , m_aiController(aiController)
    , m_playerController(playerController)
    , m_animation(new QPropertyAnimation(this))
    , m_expanded(false)
{
    ui->setupUi(this);

    ui->panel_input->setMaximumHeight(0);
    ui->lbl_result->setText(QString());

    m_animation->setTargetObject(ui->panel_input);
    m_animation->setPropertyName("maximumHeight");
    m_animation->setDuration(220);

    connect(ui->btn_toggle, &QPushButton::clicked, this, &VoiceInputWidget::toggleExpanded);

    connect(ui->btn_send, &QPushButton::clicked, this, [this]() {
        if (m_aiController == nullptr) {
            ui->lbl_result->setText(QStringLiteral("AI控制器未初始化"));
            return;
        }

        const QString input = ui->lineEdit_input->text().trimmed();
        if (input.isEmpty()) {
            return;
        }

        const bool handledByLocal = m_aiController->recognize(input);
        ui->lineEdit_input->clear();
        if (!handledByLocal) {
            ui->lbl_result->setText(QStringLiteral("联网识别中..."));
        }
    });

    connect(m_aiController, &AiController::commandReady, this, &VoiceInputWidget::handleCommand);
    connect(m_aiController, &AiController::recognizeFailed, this, [this](const QString &error) {
        if (error.contains(QStringLiteral("超时"))) {
            ui->lbl_result->setText(QStringLiteral("网络超时，请重试"));
            return;
        }
        ui->lbl_result->setText(QStringLiteral("网络错误：") + error);
    });
}

VoiceInputWidget::~VoiceInputWidget()
{
    delete ui;
}

void VoiceInputWidget::toggleExpanded()
{
    m_expanded = !m_expanded;
    m_animation->stop();
    m_animation->setStartValue(ui->panel_input->maximumHeight());
    m_animation->setEndValue(m_expanded ? 80 : 0);
    m_animation->start();
}

void VoiceInputWidget::handleCommand(const QString &cmd, const QString &param)
{
    if (m_playerController == nullptr) {
        ui->lbl_result->setText(QStringLiteral("播放器未初始化"));
        return;
    }

    if (cmd == QStringLiteral("play") || cmd == QStringLiteral("pause")) {
        m_playerController->playPause();
        ui->lbl_result->setText(cmd == QStringLiteral("play")
                                    ? QStringLiteral("已执行：播放/暂停切换")
                                    : QStringLiteral("已执行：播放/暂停切换"));
        return;
    }

    if (cmd == QStringLiteral("prev")) {
        m_playerController->prev();
        ui->lbl_result->setText(QStringLiteral("已切换：上一首"));
        return;
    }

    if (cmd == QStringLiteral("next")) {
        m_playerController->next();
        ui->lbl_result->setText(QStringLiteral("已切换：下一首"));
        return;
    }

    if (cmd == QStringLiteral("mode")) {
        if (param == QStringLiteral("single")) {
            m_playerController->setPlayMode(PlayMode::SingleLoop);
            ui->lbl_result->setText(QStringLiteral("已设置：单曲循环"));
            return;
        }
        if (param == QStringLiteral("folder")) {
            m_playerController->setPlayMode(PlayMode::FolderLoop);
            ui->lbl_result->setText(QStringLiteral("已设置：文件夹循环"));
            return;
        }
        if (param == QStringLiteral("all")) {
            m_playerController->setPlayMode(PlayMode::AllLoop);
            ui->lbl_result->setText(QStringLiteral("已设置：全部循环"));
            return;
        }
        if (param == QStringLiteral("random")) {
            m_playerController->setPlayMode(PlayMode::RandomPlay);
            ui->lbl_result->setText(QStringLiteral("已设置：随机播放"));
            return;
        }
        ui->lbl_result->setText(QStringLiteral("播放模式参数无效"));
        return;
    }

    if (cmd == QStringLiteral("search")) {
        ui->lbl_result->setText(QStringLiteral("搜索功能待实现"));
        return;
    }

    if (cmd == QStringLiteral("unknown")) {
        ui->lbl_result->setText(QStringLiteral("未识别指令，请重试"));
        return;
    }

    ui->lbl_result->setText(QStringLiteral("未识别指令，请重试"));
}
