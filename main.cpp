#include "musicplayer.h"

#include <QApplication>

int main(int argc, char *argv[])    //标准C++入口：argc/argv交给Qt解析
{
    QApplication app(argc, argv);   //构造Qt应用程序对象，初始化事件循环、字体、样式等
    app.setStyleSheet(R"(
    QWidget {
        background-color: #1a1a2e;
        color: #ffffff;
        font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
        font-size: 14px;
    }

    QLabel {
        background-color: transparent;
    }

    QSlider {
        background-color: transparent;
    }

    PlayProgressSlider {
        background-color: transparent;
    }

    GlassIconButton {
        border: none;
        padding: 0;
        outline: none;
        background-color: transparent;
    }

    GlassIconButton:hover,
    GlassIconButton:pressed {
        background-color: transparent;
    }

    QLabel#lbl_title {
        font-size: 26px;
        font-weight: bold;
        color: #ffffff;
    }

    QLabel#lbl_artist {
        font-size: 13px;
        color: #9696a8;
    }

    QLabel#lbl_album {
        font-size: 12px;
        color: #7e7e92;
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

    QLabel#lbl_currentTime, QLabel#lbl_totalTime {
        font-size: 12px;
        color: #888888;
        min-width: 40px;
    }

    QPushButton {
        background-color: transparent;
        border: none;
        color: #cccccc;
        font-size: 22px;
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
        font-size: 22px;
        color: #aaaaaa;
        background-color: transparent;
        border: 1px solid #333355;
        border-radius: 6px;
        padding: 4px 12px;
    }

    QPushButton#btn_back:hover {
        color: #ffffff;
        border-color: #55557a;
        background-color: #2a2a3e;
    }

    QLabel#lbl_currentPath {
        font-size: 12px;
        color: #888888;
    }
)");
    MusicPlayer w;  //创建音乐播放器窗口
    w.show();   //显示音乐播放器窗口
    return app.exec();  //进入事件循环，等待用户操作
}
