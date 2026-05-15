#pragma once

#include <QResizeEvent>
#include <QScrollArea>

class QWidget;

/**
 * 歌词滚动区：隐藏竖向滚动条，顶/底约 50px 半透明黑色渐变遮罩（上下渐隐）。
 */
class LyricScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit LyricScrollArea(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *m_topFade;
    QWidget *m_bottomFade;
};
