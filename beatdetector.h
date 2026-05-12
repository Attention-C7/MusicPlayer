#pragma once

#include <QList>
#include <QObject>

#include <QtGlobal>

class QAudioBuffer;

/**
 * 短时 RMS 能量 onset 检测：每帧 QAudioBuffer 算 RMS，与最近 WINDOW 帧均值比较；
 * 超过阈值且满足最小间隔则发 beatDetected。仅支持 Float / SInt16 PCM（见 cpp）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject *parent = nullptr);

    /** 送入一帧解码 PCM；内部更新滑动能量窗并可能触发 beatDetected。
     *  调试：Debug 构建下会节流输出 qDebug「[BeatDetector] feedBuffer…」以确认是否被调用；
     *  Release 下可在 CMake 对目标定义 MUSICPLAYER_BEATDETECTOR_TRACE 开启同样日志。
     */
    void feedBuffer(const QAudioBuffer &buffer);

signals:
    /** 检测到疑似节拍（能量相对滑动均值突增且通过防抖间隔）。 */
    void beatDetected();

private:
    /** 最近若干帧的 RMS 能量，长度至多为 WINDOW；用于滑动平均。 */
    QList<float> m_energyHistory;
    /** 上次 emit beatDetected 的墙钟时间（ms）；与 MIN_INTERVAL 配合防抖。 */
    qint64 m_lastBeatTime = 0;

    static constexpr int WINDOW = 20;             // 滑动窗口帧数（冷启动未满不判定）
    static constexpr float THRESHOLD = 1.5f;      // 当前 RMS 相对均值倍数阈值
    static constexpr int MIN_INTERVAL = 200;    // 两次触发最小间隔（ms）
};
