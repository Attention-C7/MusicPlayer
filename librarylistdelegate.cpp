#include "librarylistdelegate.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#include <QStringList>
#include <QStyle>

namespace {

constexpr int kThumbSide = 48;
constexpr int kRowPadH = 10;
constexpr int kRowPadV = 8;
constexpr int kFileRowHeight = 68;
constexpr int kFolderRowHeight = 48;
constexpr int kGroupRowHeight = 44;

QString formatDurationMs(qint64 ms)
{
    if (ms <= 0) {
        return QStringLiteral("--:--");
    }
    const qint64 totalSec = ms / 1000;
    const qint64 m = totalSec / 60;
    const qint64 s = totalSec % 60;
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

static QHash<QString, QPixmap> g_coverThumbCache;
static QStringList g_coverThumbOrder;

static QPixmap pixmapForCoverCacheFile(const QString &cachePath)
{
    if (cachePath.isEmpty() || !QFile::exists(cachePath)) {
        return QPixmap();
    }
    auto it = g_coverThumbCache.find(cachePath);
    if (it != g_coverThumbCache.end()) {
        return it.value();
    }
    QPixmap raw;
    if (!raw.load(cachePath)) {
        return QPixmap();
    }
    const QPixmap scaled = raw.scaled(kThumbSide, kThumbSide, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    g_coverThumbCache.insert(cachePath, scaled);
    g_coverThumbOrder.append(cachePath);
    while (g_coverThumbOrder.size() > 96) {
        const QString old = g_coverThumbOrder.takeFirst();
        g_coverThumbCache.remove(old);
    }
    return scaled;
}

QColor thumbBackgroundForPath(const QString &path)
{
    const uint h = static_cast<uint>(qHash(path)) % 360u;
    return QColor::fromHsl(static_cast<int>(h), 72, 42);
}

void drawPlaceholderThumb(QPainter *p, const QRect &r, const QString &path, const QString &title)
{
    const QColor base = thumbBackgroundForPath(path);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.0, base.lighter(115));
    g.setColorAt(1.0, base.darker(115));
    QPainterPath pathShape;
    pathShape.addRoundedRect(QRectF(r), 6.0, 6.0);
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillPath(pathShape, g);
    QString ch = title.trimmed();
    if (ch.isEmpty()) {
        ch = QStringLiteral("♪");
    } else {
        ch = ch.left(1).toUpper();
    }
    QFont f = p->font();
    f.setBold(true);
    f.setPointSize(qBound(11, f.pointSize() + 5, 18));
    p->setFont(f);
    p->setPen(QColor(255, 255, 255, 230));
    p->drawText(r, Qt::AlignCenter, ch);
    p->restore();
}

void drawFolderRow(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index)
{
    const QRect r = opt.rect.adjusted(kRowPadH, kRowPadV, -kRowPadH, -kRowPadV);
    const QString text = index.data(Qt::DisplayRole).toString();
    if (opt.state.testFlag(QStyle::State_Selected)) {
        p->fillRect(opt.rect, QColor(255, 112, 67, 36));
    }
    p->save();
    p->setPen(QColor("#c8c8dc"));
    QFont f = opt.font;
    f.setPointSize(qMax(10, f.pointSize()));
    p->setFont(f);
    const int iconW = 28;
    const QRect iconR(r.left(), r.center().y() - iconW / 2, iconW, iconW);
    if (const QStyle *style = opt.widget != nullptr ? opt.widget->style() : QApplication::style()) {
        const QIcon ico = style->standardIcon(QStyle::SP_DirIcon);
        ico.paint(p, iconR, Qt::AlignCenter);
    }
    const QRect textR(iconR.right() + 8, r.top(), r.right() - iconR.right() - 8, r.height());
    p->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, text);
    p->restore();
}

void drawGroupRow(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index)
{
    const QRect r = opt.rect.adjusted(kRowPadH, kRowPadV, -kRowPadH, -kRowPadV);
    const QString text = index.data(Qt::DisplayRole).toString();
    const bool expanded = index.data(LibraryList::Role::isExpanded).toBool();
    if (opt.state.testFlag(QStyle::State_Selected)) {
        p->fillRect(opt.rect, QColor(255, 112, 67, 28));
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QColor("#e8e8ef"));
    QFont f = opt.font;
    f.setBold(true);
    f.setPointSize(qMax(10, f.pointSize()));
    p->setFont(f);
    const QPoint c(r.left() + 8, r.center().y());
    QPolygon tri;
    if (expanded) {
        tri << QPoint(c.x() - 4, c.y() - 3) << QPoint(c.x() + 4, c.y() - 3) << QPoint(c.x(), c.y() + 4);
    } else {
        tri << QPoint(c.x() - 3, c.y() - 5) << QPoint(c.x() - 3, c.y() + 5) << QPoint(c.x() + 5, c.y());
    }
    p->setBrush(QColor("#ff7043"));
    p->drawPolygon(tri);
    const QRect textR(r.left() + 22, r.top(), r.width() - 22, r.height());
    p->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, text);
    p->restore();
}

void drawFileRow(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index)
{
    const QString path = index.data(LibraryList::Role::path).toString();
    const QString title = index.data(Qt::DisplayRole).toString();
    const QString artist = index.data(LibraryList::Role::artist).toString().trimmed();
    const qint64 dur = index.data(LibraryList::Role::durationMs).toLongLong();
    const bool isCurrent = index.data(LibraryList::Role::isCurrent).toBool();

    const QRect full = opt.rect;
    if (isCurrent) {
        p->fillRect(full, QColor(255, 112, 67, 28));
        p->fillRect(QRect(full.left(), full.top(), 3, full.height()), QColor("#ff7043"));
    } else if (opt.state.testFlag(QStyle::State_Selected)) {
        p->fillRect(full, QColor(255, 255, 255, 14));
    }

    const QRect inner = full.adjusted(kRowPadH, kRowPadV, -kRowPadH, -kRowPadV);
    const QRect thumbR(inner.left(), inner.top() + (inner.height() - kThumbSide) / 2, kThumbSide, kThumbSide);
    const QString coverCache = index.data(LibraryList::Role::coverPath).toString();
    const QPixmap coverPm = pixmapForCoverCacheFile(coverCache);
    if (!coverPm.isNull()) {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(thumbR), 6.0, 6.0);
        p->setClipPath(clip);
        const int x = thumbR.center().x() - coverPm.width() / 2;
        const int y = thumbR.center().y() - coverPm.height() / 2;
        p->drawPixmap(x, y, coverPm);
        p->restore();
    } else {
        drawPlaceholderThumb(p, thumbR, path, title);
    }

    const int textLeft = thumbR.right() + 12;
    const QString durStr = formatDurationMs(dur);
    QFontMetrics fmDur(opt.font);
    const int durW = qMax(56, fmDur.horizontalAdvance(durStr) + 14);
    QFont titleFont = opt.font;
    titleFont.setBold(true);
    const QFontMetrics fmTitle(titleFont);
    const int half = inner.height() / 2;
    const QRect titleR(textLeft, inner.top(), inner.right() - textLeft - durW, half);
    const QRect subR(textLeft, inner.top() + half, inner.right() - textLeft - durW, inner.height() - half);
    const QRect durR(inner.right() - durW, inner.top(), durW, inner.height());

    p->save();
    p->setFont(titleFont);
    p->setPen(isCurrent ? QColor("#ff9040") : QColor("#f0f0f5"));
    const QString elidedTitle = fmTitle.elidedText(title.isEmpty() ? QFileInfo(path).completeBaseName() : title,
        Qt::ElideRight, titleR.width());
    p->drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    QFont subFont = opt.font;
    subFont.setPointSize(qMax(8, opt.font.pointSize() - 1));
    p->setFont(subFont);
    p->setPen(QColor("#9898b0"));
    const QFontMetrics fmSub(subFont);
    const QString art = artist.isEmpty() ? QStringLiteral("—") : artist;
    p->drawText(subR, Qt::AlignLeft | Qt::AlignVCenter, fmSub.elidedText(art, Qt::ElideRight, subR.width()));

    p->setFont(opt.font);
    p->setPen(QColor("#b0b0c8"));
    p->drawText(durR, Qt::AlignRight | Qt::AlignVCenter, durStr);
    p->restore();
}

} // namespace

LibraryListDelegate::LibraryListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void LibraryListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid() || painter == nullptr) {
        return;
    }
    const QVariant typeVar = index.data(LibraryList::Role::itemType);
    if (!typeVar.isValid()) {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QStyledItemDelegate::paint(painter, opt, index);
        return;
    }
    const int t = typeVar.toInt();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (t == static_cast<int>(LibraryList::ItemType::folder)) {
        drawFolderRow(painter, option, index);
    } else if (t == static_cast<int>(LibraryList::ItemType::group)) {
        drawGroupRow(painter, option, index);
    } else if (t == static_cast<int>(LibraryList::ItemType::file)) {
        drawFileRow(painter, option, index);
    } else {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QStyledItemDelegate::paint(painter, opt, index);
    }
    painter->restore();
}

QSize LibraryListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QStyledItemDelegate::sizeHint(option, index);
    }
    const QVariant typeVar = index.data(LibraryList::Role::itemType);
    if (!typeVar.isValid()) {
        return QStyledItemDelegate::sizeHint(option, index);
    }
    const int t = typeVar.toInt();
    int w = 320;
    if (option.widget != nullptr) {
        w = qMax(200, option.widget->width());
    }
    if (t == static_cast<int>(LibraryList::ItemType::folder)) {
        return QSize(w, kFolderRowHeight);
    }
    if (t == static_cast<int>(LibraryList::ItemType::group)) {
        return QSize(w, kGroupRowHeight);
    }
    if (t == static_cast<int>(LibraryList::ItemType::file)) {
        return QSize(w, kFileRowHeight);
    }
    return QStyledItemDelegate::sizeHint(option, index);
}
