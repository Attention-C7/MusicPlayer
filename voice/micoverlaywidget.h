#pragma once

#include <QWidget>

class QTimer;

/**
 * 语音识别中：中央麦克风图标 + 5 根竖条随相位/伪音量起伏（QTimer 驱动 update）。
 */
class MicOverlayWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MicOverlayWidget(QWidget *parent = nullptr);

    void setListening(bool listening);

    bool isListening() const { return m_listening; }

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTick();

private:
    QTimer *m_timer;
    bool m_listening;
    qreal m_phase;
    qreal m_levels[5];
};
