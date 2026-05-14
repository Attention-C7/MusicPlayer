#pragma once

#include <QPixmap>
#include <QWidget>

/**
 * 语音抽屉打开时的全屏磨砂遮罩：底层为播放区截图轻模糊，再叠半透明色，与主界面分层（类 iOS Sheet / Material scrim）。
 */
class VoiceDrawerScrimWidget final : public QWidget
{
public:
    explicit VoiceDrawerScrimWidget(QWidget *parent = nullptr);

    void clearBackdrop();
    void setBackdropBlurred(const QPixmap &pixmap);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_blur;
};
