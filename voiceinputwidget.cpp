#include "voiceinputwidget.h"
#include "ui_voiceinputwidget.h"

#include "micoverlaywidget.h"
#include "voicechatdelegate.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractButton>
#include <QEasingCurve>
#include "commanddispatcher.h"
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyleOptionViewItem>
#include <QToolButton>

namespace {

constexpr int kBottomChromeReserve = 130;
constexpr int kDrawerClosedHeight = 52;
constexpr int kDrawerAnimMs = 280;

} // namespace

int VoiceInputWidget::drawerOpenHeightPx() const
{
    return qBound(248, (m_playH * 46) / 100, 352);
}

static QIcon makeSendIconImpl()
{
    const int s = 24;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor("#FF7043"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor("#FF7043"));
    QPainterPath plane;
    plane.moveTo(3.0, 12.0);
    plane.lineTo(20.0, 5.5);
    plane.lineTo(10.5, 12.0);
    plane.lineTo(20.0, 18.5);
    plane.closeSubpath();
    painter.drawPath(plane);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(8.5, 12.0), 2.0, 2.0);
    return QIcon(pm);
}

QIcon VoiceInputWidget::makeSendIcon()
{
    static const QIcon icon = makeSendIconImpl();
    return icon;
}

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
    , m_chatDelegate(nullptr)
    , m_micOverlay(nullptr)
    , m_drawerGeomAnim(new QPropertyAnimation(this, QByteArrayLiteral("geometry"), this))
    , m_drawerOpen(false)
    , m_playW(800)
    , m_playH(500)
    , m_allSongs(std::move(allSongs))
    , m_artistMap(std::move(artistMap))
    , m_albumMap(std::move(albumMap))
{
    ui->setupUi(this);
    setObjectName(QStringLiteral("VoiceInputWidgetRoot"));

    m_chatDelegate = new VoiceChatDelegate(ui->list_chat);
    ui->list_chat->setItemDelegate(m_chatDelegate);
    ui->list_chat->setSelectionMode(QAbstractItemView::NoSelection);
    ui->list_chat->setFocusPolicy(Qt::NoFocus);
    ui->list_chat->setSpacing(2);
    ui->list_chat->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; color: #e8e8ef; border: none; }"
        "QListWidget::item { background: transparent; border: none; padding: 0; }"));

    ui->lbl_hint->setStyleSheet(QStringLiteral(
        "color: #8888a0; font-size: 12px; background: transparent;"));

    const QString pillStyle = QStringLiteral(
        "QPushButton {"
        "background-color: #333355;"
        "color: #dddddd;"
        "border: 1px solid #444466;"
        "border-radius: 14px;"
        "padding: 4px 14px;"
        "font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "background-color: #3d3d5a;"
        "color: #FF7043;"
        "border-color: #FF7043;"
        "}"
        "QPushButton:pressed { background-color: #2a2a40; }");
    ui->btn_pill_next->setStyleSheet(pillStyle);
    ui->btn_pill_pause->setStyleSheet(pillStyle);
    ui->btn_pill_shuffle->setStyleSheet(pillStyle);
    ui->btn_pill_next->setCursor(Qt::PointingHandCursor);
    ui->btn_pill_pause->setCursor(Qt::PointingHandCursor);
    ui->btn_pill_shuffle->setCursor(Qt::PointingHandCursor);

    ui->lineEdit_input->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "background-color: #2a2a3e;"
        "border: 1px solid #444466;"
        "border-radius: 10px;"
        "padding: 6px 12px;"
        "color: #f0f0f5;"
        "font-size: 14px;"
        "}"));

    auto *sendBtn = qobject_cast<QToolButton *>(ui->btn_send);
    if (sendBtn != nullptr) {
        sendBtn->setIcon(makeSendIcon());
        sendBtn->setIconSize(QSize(22, 22));
        sendBtn->setAutoRaise(true);
        sendBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        sendBtn->setStyleSheet(QStringLiteral(
            "QToolButton { background: transparent; border: none; border-radius: 8px; padding: 4px; }"
            "QToolButton:hover { background-color: rgba(255,112,67,0.18); }"
            "QToolButton:pressed { background-color: rgba(255,112,67,0.28); }"));
        sendBtn->setCursor(Qt::PointingHandCursor);
    }

    ui->btn_toggle->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "text-align: left;"
        "padding: 8px 12px;"
        "background-color: #2a2a3e;"
        "color: #e8e8ef;"
        "border: 1px solid #3a3a55;"
        "border-radius: 10px;"
        "font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #333355; color: #FF7043; border-color: #FF7043; }"));

    setStyleSheet(QStringLiteral(
        "#VoiceInputWidgetRoot {"
        "background-color: rgba(32,32,52,0.94);"
        "border: 1px solid rgba(255,255,255,0.10);"
        "border-bottom: none;"
        "border-top-left-radius: 18px;"
        "border-top-right-radius: 18px;"
        "}"));

    m_micOverlay = new MicOverlayWidget(this);
    m_micOverlay->hide();

    m_drawerGeomAnim->setDuration(kDrawerAnimMs);
    m_drawerGeomAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_drawerGeomAnim, &QPropertyAnimation::finished, this, &VoiceInputWidget::updateMicOverlayGeometry);

    if (m_aiController != nullptr) {
        m_aiController->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }

    connect(ui->btn_toggle, &QPushButton::clicked, this, &VoiceInputWidget::onToggleDrawerClicked);
    connect(ui->btn_send, &QAbstractButton::clicked, this, &VoiceInputWidget::onSendClicked);
    connect(ui->lineEdit_input, &QLineEdit::returnPressed, this, &VoiceInputWidget::onSendClicked);
    connect(ui->btn_pill_next, &QPushButton::clicked, this, &VoiceInputWidget::onPillNextClicked);
    connect(ui->btn_pill_pause, &QPushButton::clicked, this, &VoiceInputWidget::onPillPauseClicked);
    connect(ui->btn_pill_shuffle, &QPushButton::clicked, this, &VoiceInputWidget::onPillShuffleClicked);

    if (m_aiController != nullptr) {
        connect(m_aiController, &AiController::recognizing, this, &VoiceInputWidget::onRecognizing);
        connect(m_aiController, &AiController::recognizeFailed, this, &VoiceInputWidget::onRecognizeFailed);
        CommandDispatcher *dispatcher = m_aiController->dispatcher();
        if (dispatcher != nullptr) {
            connect(dispatcher, &CommandDispatcher::dispatchResult, this, &VoiceInputWidget::onDispatchResult);
        }
    }

    appendChatMessage(
        false,
        QStringLiteral("你好，可以用文字或下方快捷指令控制播放；展开后可查看对话记录。"));
    (void)m_playerController;

    setConversationVisible(false);
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
    m_allSongs = std::move(allSongs);
    m_artistMap = std::move(artistMap);
    m_albumMap = std::move(albumMap);
    if (m_aiController != nullptr) {
        m_aiController->setSearchContext(m_allSongs, m_artistMap, m_albumMap);
    }
}

void VoiceInputWidget::applyDrawerGeometry(int playWidgetWidth, int playWidgetHeight)
{
    m_playW = qMax(1, playWidgetWidth);
    m_playH = qMax(1, playWidgetHeight);
    if (m_drawerGeomAnim->state() == QAbstractAnimation::Running) {
        m_drawerGeomAnim->setEndValue(computeDrawerGeometry());
    } else {
        setGeometry(computeDrawerGeometry());
    }
    updateMicOverlayGeometry();
}

QRect VoiceInputWidget::computeDrawerGeometry() const
{
    const int h = m_drawerOpen ? drawerOpenHeightPx() : kDrawerClosedHeight;
    const int y = qMax(0, m_playH - kBottomChromeReserve - h);
    return QRect(0, y, m_playW, h);
}

void VoiceInputWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateMicOverlayGeometry();
}

void VoiceInputWidget::updateMicOverlayGeometry()
{
    if (m_micOverlay == nullptr) {
        return;
    }
    if (m_micOverlay->isListening()) {
        m_micOverlay->setGeometry(rect());
    } else if (ui->list_chat != nullptr && ui->list_chat->isVisible()) {
        m_micOverlay->setGeometry(ui->list_chat->geometry());
    } else {
        m_micOverlay->setGeometry(QRect());
    }
    m_micOverlay->raise();
}

void VoiceInputWidget::appendChatMessage(bool isUser, const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    auto *item = new QListWidgetItem();
    item->setData(Qt::DisplayRole, text);
    item->setData(Qt::UserRole, isUser);
    item->setFlags(Qt::ItemIsEnabled);

    ui->list_chat->addItem(item);

    QStyleOptionViewItem opt;
    opt.initFrom(ui->list_chat);
    const int vw = qMax(220, ui->list_chat->viewport()->width());
    opt.rect = QRect(0, 0, vw, 400);
    opt.font = ui->list_chat->font();
    const int row = ui->list_chat->count() - 1;
    const QModelIndex idx = ui->list_chat->model()->index(row, 0);
    item->setSizeHint(m_chatDelegate->sizeHint(opt, idx));

    ui->list_chat->scrollToBottom();
}

void VoiceInputWidget::setConversationVisible(bool visible)
{
    ui->list_chat->setVisible(visible);
    ui->lbl_hint->setVisible(visible);
    ui->btn_pill_next->setVisible(visible);
    ui->btn_pill_pause->setVisible(visible);
    ui->btn_pill_shuffle->setVisible(visible);
    ui->lineEdit_input->setVisible(visible);
    ui->btn_send->setVisible(visible);
}

void VoiceInputWidget::onToggleDrawerClicked()
{
    m_drawerOpen = !m_drawerOpen;
    setConversationVisible(m_drawerOpen);
    ui->btn_toggle->setText(m_drawerOpen
        ? QStringLiteral("▼ 收起语音与指令")
        : QStringLiteral("▲ 语音与指令"));

    emit drawerOpenChanged(m_drawerOpen);

    m_drawerGeomAnim->stop();
    m_drawerGeomAnim->setStartValue(geometry());
    m_drawerGeomAnim->setEndValue(computeDrawerGeometry());
    m_drawerGeomAnim->start();
}

void VoiceInputWidget::onSendClicked()
{
    const QString input = ui->lineEdit_input->text().trimmed();
    if (input.isEmpty()) {
        return;
    }
    appendChatMessage(true, input);
    ui->lineEdit_input->clear();
    if (m_aiController != nullptr) {
        m_aiController->recognize(input);
    }
}

void VoiceInputWidget::onPillNextClicked()
{
    appendChatMessage(true, QStringLiteral("下一首"));
    if (m_aiController != nullptr) {
        m_aiController->recognize(QStringLiteral("下一首"));
    }
}

void VoiceInputWidget::onPillPauseClicked()
{
    appendChatMessage(true, QStringLiteral("暂停"));
    if (m_aiController != nullptr) {
        m_aiController->recognize(QStringLiteral("暂停"));
    }
}

void VoiceInputWidget::onPillShuffleClicked()
{
    appendChatMessage(true, QStringLiteral("随机播放"));
    if (m_aiController != nullptr) {
        m_aiController->recognize(QStringLiteral("随机播放"));
    }
}

void VoiceInputWidget::onRecognizing()
{
    m_micOverlay->setListening(true);
    m_micOverlay->show();
    updateMicOverlayGeometry();
}

void VoiceInputWidget::onRecognizeFailed(const QString &error)
{
    m_micOverlay->setListening(false);
    m_micOverlay->hide();
    updateMicOverlayGeometry();
    appendChatMessage(false, error.trimmed().isEmpty() ? QStringLiteral("识别失败") : error);
}

void VoiceInputWidget::onDispatchResult(bool success, const QString &message)
{
    Q_UNUSED(success);
    m_micOverlay->setListening(false);
    m_micOverlay->hide();
    updateMicOverlayGeometry();
    appendChatMessage(false, message.isEmpty() ? QStringLiteral("完成") : message);
}
