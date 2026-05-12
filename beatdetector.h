#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲采样率懒初始化 aubio tempo + onset。
 * - 主拍：aubio_tempo 有 hit 且置信度 ≥ kTempoConfTrigger → beatDetected(kTempoIntensity)，墙钟最小间隔 kHardMinIntervalMs。
 * - 辅拍：同一 hop 无主拍时，onset 描述子 > m_onsetThreshold 且满足间隔 → beatDetected(kOnsetIntensity)（UI 可结合 kMinBeatIntensity 忽略）。
 * 无动态阈值、无 BPM 平滑/预测/归一化。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    static constexpr float kTempoConfTrigger = 0.5f;
    static constexpr float kTempoIntensity = 1.0f;
    static constexpr float kOnsetIntensity = 0.3f;
    /** 检测器侧「强主拍」语义：与 aubio_tempo 置信度比较；非 UI 闪光门限（UI 应接受 kOnsetIntensity 辅拍）。 */
    static constexpr float kMinBeatIntensity = 0.6f;

    explicit BeatDetector(QObject *parent = nullptr);
    ~BeatDetector() override;

    void feedBuffer(const QAudioBuffer &buffer);

    /** onset 固定比较门限，与 aubio_onset_set_threshold 同步；默认 0.5。 */
    void setOnsetThreshold(float value);
    /** aubio 静音门限（dB），如 -70。 */
    void setOnsetSilenceDb(float value);

signals:
    void beatDetected(float intensity);

private:
    void releaseAnalysisEngine();
    bool ensureAnalysisEngine(int sampleRate);

    aubio_tempo_t *m_aubioTempo = nullptr;
    aubio_onset_t *m_aubioOnset = nullptr;
    fvec_t *m_inputBuf = nullptr;
    fvec_t *m_outputBuf = nullptr;
    fvec_t *m_onsetOut = nullptr;
    QVector<float> m_pending;

    QElapsedTimer m_lastBeatTime;
    bool m_hasLastBeat = false;

    uint_t m_sampleRate = 0;
    float m_onsetThreshold = 0.5f;
    float m_onsetSilenceDb = -70.0f;

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;
    static constexpr qint64 kHardMinIntervalMs = 260;
};
