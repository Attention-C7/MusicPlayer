#pragma once

#include <QStyledItemDelegate>

/**
 * QListWidget 气泡绘制：用户消息靠右（强调色），系统反馈靠左（深底浅字），圆角仿 iMessage。
 */
class VoiceChatDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit VoiceChatDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
