#pragma once

#include <QObject>

#include <QtGlobal>

class QAudioBuffer;

/**
 * 节拍 onset：将 PCM 交错采样按每 8 点取均值降采样，再对低频序列算 RMS（lowRms）；
 * 用相邻缓冲的 lowRms 差分突增 + 时间防抖触发 beatDetected。
 * PCM 仅支持 Float / SInt16（见 cpp）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject *parent = nullptr);

    /** 送入一帧解码 PCM；内部更新 lowRms 差分并可能触发 beatDetected。 */
    void feedBuffer(const QAudioBuffer &buffer);

signals:
    /** 检测到疑似节拍（lowRms 相对上一帧突增且通过防抖）。 */
    void beatDetected();

private:
    float m_prevLowRms = 0.0f;
    bool m_hasPrevLowRms = false;
    qint64 m_lastBeatTime = 0;

    static constexpr int DOWNSAMPLE_STRIDE = 8;   // 每 STRIDE 个标量采样取均值作为一个低频点
    static constexpr float DIFF_THRESHOLD = 0.03f; // lowRms 相对上一帧增量门限（可调）
    static constexpr int MIN_INTERVAL_MS = 250;   // 两次触发最小间隔（ms）
};
