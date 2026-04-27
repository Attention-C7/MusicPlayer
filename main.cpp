#include "musicplayer.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(R"(
    QWidget {
        background-color: #1a1a2e;
        color: #ffffff;
        font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
        font-size: 14px;
    }

    QLabel#lbl_title {
        font-size: 20px;
        font-weight: bold;
        color: #ffffff;
    }

    QLabel#lbl_artist {
        font-size: 14px;
        color: #aaaaaa;
    }

    QLabel#lbl_album {
        font-size: 12px;
        color: #888888;
    }

    QLabel#lbl_index {
        font-size: 12px;
        color: #666666;
    }

    QLabel#lbl_albumArt {
        background-color: #2a2a3e;
        border-radius: 12px;
        font-size: 48px;
        color: #444466;
    }

    QSlider#slider_progress::groove:horizontal {
        height: 4px;
        background: #333355;
        border-radius: 2px;
    }

    QSlider#slider_progress::sub-page:horizontal {
        background: #ff6900;
        border-radius: 2px;
    }

    QSlider#slider_progress::handle:horizontal {
        width: 12px;
        height: 12px;
        margin: -4px 0;
        border-radius: 6px;
        background: #ff6900;
    }

    QLabel#lbl_currentTime, QLabel#lbl_totalTime {
        font-size: 12px;
        color: #888888;
        min-width: 40px;
    }

    QPushButton {
        background-color: transparent;
        border: none;
        color: #cccccc;
        font-size: 18px;
        padding: 8px 12px;
        border-radius: 6px;
    }

    QPushButton:hover {
        background-color: #2a2a3e;
        color: #ffffff;
    }

    QPushButton:pressed {
        background-color: #333355;
    }

    QPushButton#btn_playPause {
        font-size: 24px;
        color: #ffffff;
        background-color: #ff6900;
        border-radius: 20px;
        padding: 10px 16px;
        min-width: 48px;
        min-height: 48px;
    }

    QPushButton#btn_playPause:hover {
        background-color: #ff8533;
    }

    QPushButton#btn_playPause:pressed {
        background-color: #cc5500;
    }

    QListWidget {
        background-color: transparent;
        border: none;
        color: #ffffff;
        font-size: 14px;
    }

    QListWidget::item {
        padding: 10px 16px;
        border-bottom: 1px solid rgba(255,255,255,0.05);
    }

    QListWidget::item:hover {
        background-color: rgba(255,255,255,0.08);
        border-radius: 6px;
    }

    QListWidget::item:selected {
        background-color: rgba(255,105,0,0.2);
        color: #ff6900;
    }

    QPushButton#btn_back {
        font-size: 14px;
        color: #aaaaaa;
        background-color: transparent;
        border: 1px solid #333355;
        border-radius: 6px;
        padding: 4px 12px;
    }

    QPushButton#btn_back:hover {
        color: #ffffff;
        border-color: #ff6900;
    }

    QLabel#lbl_currentPath {
        font-size: 12px;
        color: #888888;
    }
)");
    MusicPlayer w;
    w.show();
    return app.exec();
}
