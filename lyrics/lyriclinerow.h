#pragma once

#include <QWidget>
#include <QtGlobal>

class QLabel;
class QToolButton;

/**
 * 单行歌词：默认仅居中显示文案；鼠标悬停时在左侧显示播放图标与时间(mm:ss)，点击行或图标跳转。
 */
class LyricLineRow : public QWidget
{
    Q_OBJECT

public:
    explicit LyricLineRow(qint64 timeMs, const QString &timeText, const QString &lyricText, QWidget *parent = nullptr);

    void setActiveLine(bool active);

    qint64 timeMs() const { return m_timeMs; }

signals:
    void seekRequested(qint64 timeMs);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    qint64 m_timeMs;
    QWidget *m_hoverChrome;
    QToolButton *m_playBtn;
    QLabel *m_timeLabel;
    QLabel *m_textLabel;
};
