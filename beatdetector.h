#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲采样率懒初始化 aubio tempo（无 onset）。
 * 单声道 float 块送入 aubio_tempo；使用 aubio 默认 silence/threshold（不显式 set_*）。
 * 有 hit 且置信度 ≥ kTempoConfTrigger；防抖间隔按 BPM 折半后估算（60000/bpm*0.8 ms，墙钟）。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    /** aubio_tempo_get_confidence 下限（与曾跑通配置对齐）。 */
    static constexpr float kTempoConfTrigger = 0.15f;
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

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;
};
