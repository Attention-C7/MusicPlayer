#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MusicPlayer;
}
QT_END_NAMESPACE

class MusicPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer() override;

private:
    Ui::MusicPlayer *ui;
};
#endif // MUSICPLAYER_H
