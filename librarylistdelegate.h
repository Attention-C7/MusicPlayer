#pragma once

#include <QStyledItemDelegate>

namespace LibraryList {

enum class ItemType
{
    folder = 1,
    file = 2,
    group = 3
};

namespace Role
{
inline constexpr int itemType = Qt::UserRole;
inline constexpr int path = Qt::UserRole + 1;
inline constexpr int groupName = Qt::UserRole + 2;
inline constexpr int durationMs = Qt::UserRole + 3;
inline constexpr int isCurrent = Qt::UserRole + 4;
inline constexpr int isExpanded = Qt::UserRole + 5;
inline constexpr int artist = Qt::UserRole + 6;
inline constexpr int coverPath = Qt::UserRole + 7;
}

} // namespace LibraryList

class LibraryListDelegate final : public QStyledItemDelegate
{
public:
    explicit LibraryListDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
