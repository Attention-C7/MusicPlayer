#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲采样率懒初始化 aubio tempo（无 onset）。
 * 单声道 float 块送入 aubio_tempo；使用 aubio 默认 silence/threshold（不显式 set_*）。
 * 有 hit 且置信度 ≥ kTempoConfTrigger；防抖间隔按 BPM（可选折半）估算（60000/bpm*0.75 ms，墙钟）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    /** aubio_tempo_get_confidence 下限；误触靠防抖间隔抑制。 */
    static constexpr float kTempoConfTrigger = 0.05f;
    static constexpr float kTempoIntensity = 1.0f;

    explicit BeatDetector(QObject *parent = nullptr);
    ~BeatDetector() override;

    void feedBuffer(const QAudioBuffer &buffer);

signals:
    void beatDetected(float intensity);

private:
    void releaseAnalysisEngine();
    bool ensureAnalysisEngine(int sampleRate);

    aubio_tempo_t *m_aubioTempo = nullptr;
    fvec_t *m_inputBuf = nullptr;
    fvec_t *m_outputBuf = nullptr;
    QVector<float> m_pending;

    qint64 m_lastBeatWallMs = 0;

    uint_t m_sampleRate = 0;
    /** aubio 引擎有效 hop 计数；重建后用于前若干 hop 仅预热、不参与 hit 判定。 */
    int m_tempoWarmupHopCount = 0;

    /** aubio 窗长（FFT）；须与 new_aubio_tempo 第二参一致。 */
    static constexpr uint_t WIN_SIZE = 2048;
    /**
     * 每 hop 样本数：须与 new_fvec(HOP_SIZE)、new_aubio_tempo 第三参 hop_size、feedBuffer 内写入帧长完全一致。
     * 常见错误：new_fvec(512) 与 HOP_SIZE=256 混用 → 填充越界或 tempo 与缓冲错位。
     */
    static constexpr uint_t HOP_SIZE = 256;
    static_assert(WIN_SIZE == 2048u);
    static_assert(HOP_SIZE == 256u);
};
