#include "voicechatdelegate.h"

#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kBubbleRadius = 16;
constexpr int kBubblePadH = 12;
constexpr int kBubblePadV = 8;
constexpr int kSideMargin = 12;
constexpr qreal kMaxBubbleWidthRatio = 0.72;

} // namespace

VoiceChatDelegate::VoiceChatDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize VoiceChatDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QString text = index.data(Qt::DisplayRole).toString();
    const int listW = option.widget != nullptr ? option.widget->width() : 400;
    const int maxBubble = qMax(120, static_cast<int>(listW * kMaxBubbleWidthRatio));
    QFont font = option.font;
    if (index.data(Qt::UserRole).toBool()) {
        font.setPointSize(qMax(14, font.pointSize()));
        font.setBold(true);
    } else {
        font.setPointSize(qMax(13, font.pointSize()));
        font.setBold(false);
    }
    const QFontMetrics fm(font);
    const QRect br = fm.boundingRect(QRect(0, 0, maxBubble - 2 * kBubblePadH, 2000), Qt::TextWordWrap, text);
    const int h = br.height() + 2 * kBubblePadV + 8;
    return QSize(listW, qMax(36, h));
}

void VoiceChatDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QString text = index.data(Qt::DisplayRole).toString();
    const bool isUser = index.data(Qt::UserRole).toBool();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QFont font = option.font;
    if (isUser) {
        font.setPointSize(qMax(14, font.pointSize()));
        font.setBold(true);
    } else {
        font.setPointSize(qMax(13, font.pointSize()));
        font.setBold(false);
    }
    painter->setFont(font);
    const QFontMetrics fm(font);

    const int listW = option.rect.width();
    const int maxBubble = qMax(120, static_cast<int>(listW * kMaxBubbleWidthRatio));
    const QRect textRect = fm.boundingRect(QRect(0, 0, maxBubble - 2 * kBubblePadH, 2000), Qt::TextWordWrap, text);
    const int bubbleW = qMin(maxBubble, textRect.width() + 2 * kBubblePadH + 4);
    const int bubbleH = textRect.height() + 2 * kBubblePadV;

    QRect bubbleRect;
    if (isUser) {
        bubbleRect = QRect(
            option.rect.right() - kSideMargin - bubbleW,
            option.rect.top() + 4,
            bubbleW,
            bubbleH);
        QLinearGradient grad(bubbleRect.topLeft(), bubbleRect.bottomRight());
        grad.setColorAt(0.0, QColor("#FF7043"));
        grad.setColorAt(1.0, QColor("#FF8A65"));
        painter->setPen(QPen(QColor(255, 255, 255, 40), 1.0));
        painter->setBrush(grad);
    } else {
        bubbleRect = QRect(
            option.rect.left() + kSideMargin,
            option.rect.top() + 4,
            bubbleW,
            bubbleH);
        painter->setPen(QPen(QColor(80, 80, 100, 180), 1.0));
        painter->setBrush(QColor(42, 42, 62, 240));
    }

    QPainterPath path;
    path.addRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
    painter->drawPath(path);

    const QRect inner = bubbleRect.adjusted(kBubblePadH, kBubblePadV, -kBubblePadH, -kBubblePadV);
    painter->setPen(isUser ? QColor("#ffffff") : QColor("#e8e8ef"));
    painter->drawText(inner, Qt::TextWordWrap, text);

    painter->restore();
}
