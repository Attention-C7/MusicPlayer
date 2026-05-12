#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲采样率懒初始化 aubio tempo（无 onset）。
 * 流行音乐底鼓主导节奏；单声道 float 后经一阶 IIR 低通（约 200Hz）再送入 aubio_tempo（specflux），再配合 silence/threshold 调灵敏。
 * 有 hit 且置信度 ≥ kTempoConfTrigger；防抖间隔按 BPM 折半后估算（60000/bpm*0.85 ms，墙钟）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    /** aubio_tempo_get_confidence 下限；流行音乐略抬高以减少误触（相对 0.15）。 */
    static constexpr float kTempoConfTrigger = 0.20f;
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
    /** 一阶低通 y += alpha*(x-y)，alpha≈2π*fc/fs；跨 hop 连续。 */
    float m_lowPassPrev = 0.0f;
    float m_lowPassAlpha = 0.0f;

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;
};
