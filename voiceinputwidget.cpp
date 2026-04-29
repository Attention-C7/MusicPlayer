#include "voiceinputwidget.h"
#include "ui_voiceinputwidget.h"

#include <QFileInfo>
#include <QPropertyAnimation>

VoiceInputWidget::VoiceInputWidget(
    AiController *aiController,
    PlayerController *playerController,
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap,
    QWidget *parent
)
    : QWidget(parent)
    , ui(new Ui::VoiceInputWidget)
    , m_aiController(aiController)
    , m_playerController(playerController)
    , m_animation(new QPropertyAnimation(this))
    , m_expanded(false)
    , m_allSongs(allSongs)
    , m_artistMap(artistMap)
    , m_albumMap(albumMap)
{
    ui->setupUi(this);

    ui->panel_input->setMaximumHeight(0);
    ui->btn_toggle->setText(QStringLiteral("🎤 语音/文字控制"));
    ui->lbl_hint->setText(QStringLiteral("当前为文字模式，语音录入需接入麦克风SDK"));
    ui->lbl_result->setText(QString());
    if (m_aiController != nullptr) {
        m_aiController->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }

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
        if (error.contains(QStringLiteral("未能及时响应"))) {
            ui->lbl_result->setText(QStringLiteral("未能及时响应，已尝试本地处理"));
            return;
        }
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

void VoiceInputWidget::setSearchContext(
    QList<SongInfo> allSongs,
    QMap<QString, QList<SongInfo>> artistMap,
    QMap<QString, QList<SongInfo>> albumMap
)
{
    m_allSongs = allSongs;
    m_artistMap = artistMap;
    m_albumMap = albumMap;
    if (m_aiController != nullptr) {
        m_aiController->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }
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
        const QString keyword = param.trimmed();
        if (keyword.isEmpty()) {
            ui->lbl_result->setText(QStringLiteral("未找到相关歌曲"));
            return;
        }

        int targetIndex = -1;
        for (int i = 0; i < m_allSongs.size(); ++i) {
            if (m_allSongs[i].filePath == keyword) {
                targetIndex = i;
                break;
            }
        }

        if (targetIndex < 0) {
            for (int i = 0; i < m_allSongs.size(); ++i) {
                const SongInfo &song = m_allSongs[i];
                if (song.title.contains(keyword, Qt::CaseInsensitive)
                    || song.artist.contains(keyword, Qt::CaseInsensitive)) {
                    targetIndex = i;
                    break;
                }
            }
        }

        if (targetIndex < 0) {
            ui->lbl_result->setText(QStringLiteral("未找到相关歌曲：") + keyword);
            return;
        }

        m_playerController->setPlaylist(m_allSongs);
        m_playerController->playSong(targetIndex);
        const QFileInfo info(m_allSongs[targetIndex].filePath);
        const QString title = m_allSongs[targetIndex].title.trimmed().isEmpty()
                                  ? info.completeBaseName()
                                  : m_allSongs[targetIndex].title.trimmed();
        ui->lbl_result->setText(QStringLiteral("已播放：") + title);
        return;
    }

    if (cmd == QStringLiteral("unknown")) {
        ui->lbl_result->setText(QStringLiteral("未识别指令，请重试"));
        return;
    }

    ui->lbl_result->setText(QStringLiteral("未识别指令，请重试"));
}
