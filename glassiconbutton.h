#pragma once

#include <QColor>
#include <QEvent>
#include <QPixmap>
#include <QPushButton>

class QMouseEvent;
class QPaintEvent;
class QVariantAnimation;

/**
 * 毛玻璃风格工具按钮：SVG 着色（QSvgRenderer）、主/副尺寸、悬停底、点击 0.9→1.0 缩放、可选激活指示点。
 */
class GlassIconButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal clickScale READ clickScale WRITE setClickScale)

public:
    enum class ChipRole {
        Secondary40,
        MainPlay56
    };

    explicit GlassIconButton(QWidget *parent = nullptr);

    void setChipRole(ChipRole role);
    void setSvgResourcePath(const QString &qrcPath);
    void setGlyphTint(const QColor &color);
    /** 激活态：图标改橙色并在底部绘制 2px 指示点（如循环/随机已选模式）。 */
    void setAccentActive(bool active);
    /** 仅节拍按钮为 true：按 level 绘制无歌词/可进全屏/已开启三态描边。 */
    void setBeatVisualEnabled(bool enabled);

    qreal clickScale() const { return m_clickScale; }
    void setClickScale(qreal scale);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onClickAnimFinished();

private:
    void rebuildGlyphPixmap();
    QRect iconDrawRect() const;

    QString m_svgPath;
    QColor m_glyphTint;
    bool m_accentActive;
    int m_beatChrome;
    bool m_beatVisual;
    ChipRole m_role;
    qreal m_clickScale;
    bool m_pressed;
    QPixmap m_glyphPixmap;
    QVariantAnimation *m_clickAnim;
};
