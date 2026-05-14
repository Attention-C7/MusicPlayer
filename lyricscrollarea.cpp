#include "lyricscrollarea.h"

#include <QFrame>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWidget>

namespace {

constexpr int kFadeHeight = 50;

class LyricEdgeFadeWidget final : public QWidget
{
public:
    explicit LyricEdgeFadeWidget(bool topEdge, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_topEdge(topEdge)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        QLinearGradient gradient(0, 0, 0, height());
        if (m_topEdge) {
            gradient.setColorAt(0.0, QColor(0, 0, 0, 210));
            gradient.setColorAt(0.55, QColor(0, 0, 0, 90));
            gradient.setColorAt(1.0, QColor(0, 0, 0, 0));
        } else {
            gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
            gradient.setColorAt(0.45, QColor(0, 0, 0, 90));
            gradient.setColorAt(1.0, QColor(0, 0, 0, 210));
        }
        painter.fillRect(rect(), gradient);
    }

private:
    bool m_topEdge;
};

} // namespace

LyricScrollArea::LyricScrollArea(QWidget *parent)
    : QScrollArea(parent)
    , m_topFade(new LyricEdgeFadeWidget(true, this))
    , m_bottomFade(new LyricEdgeFadeWidget(false, this))
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);
    m_topFade->raise();
    m_bottomFade->raise();
}

void LyricScrollArea::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    const int h = height();
    const int fh = qMin(kFadeHeight, qMax(24, h / 3));
    m_topFade->setGeometry(0, 0, width(), fh);
    m_bottomFade->setGeometry(0, h - fh, width(), fh);
    m_topFade->raise();
    m_bottomFade->raise();
}
