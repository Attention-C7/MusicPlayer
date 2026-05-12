#pragma once

#include <QList>
#include <QObject>

#include <QtGlobal>

class QAudioBuffer;

/**
 * 短时 RMS onset：每帧算 RMS，保留最近 WINDOW 帧；用窗内最小 RMS 作 baseline，
 * 当前 peak 明显高于 baseline（且高于静音门限）时发 beatDetected，并做时间防抖。
 * PCM 仅支持 Float / SInt16（见 cpp）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject *parent = nullptr);

    /** 送入一帧解码 PCM；更新能量窗并可能触发 beatDetected。
     *  调试：MUSICPLAYER_BEATDETECTOR_TRACE 或 QT_DEBUG 下可打 feedBuffer 到达日志（见 cpp）。
     */
    void feedBuffer(const QAudioBuffer &buffer);

signals:
    /** 检测到疑似节拍（peak 相对窗内 baseline 突增且通过防抖）。 */
    void beatDetected();

private:
    /** 最近 WINDOW 帧的 RMS，用于取 baseline = min(窗)。 */
    QList<float> m_energyHistory;
    /** 上次 emit beatDetected 的墙钟时间（ms）。 */
    qint64 m_lastBeatTime = 0;

    static constexpr int WINDOW = 8;              // 窗越短 baseline 跟得越紧，易反映真实起伏
    static constexpr float THRESHOLD = 1.2f;      // peak 需超过 baseline 的倍数
    static constexpr float MIN_PEAK_RMS = 0.05f;  // 当前帧 RMS 下限，抑制纯静音/底噪误触
    static constexpr int MIN_INTERVAL = 200;      // 两次触发最小间隔（ms）
};
