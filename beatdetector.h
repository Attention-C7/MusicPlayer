#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲实际采样率懒初始化 aubio。
 * - tempo：强节奏 / 流行（置信度 + BPM 软间隔）
 * - onset specflux：弱鼓时起音；高置信 tempo 时本 hop 不再并入 onset，减轻 POP 过密
 * - 仅 onset 路径使用略长硬间隔，减轻民谣「一字一音」快于真拍
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject *parent = nullptr);
    ~BeatDetector() override;

    void feedBuffer(const QAudioBuffer &buffer);

    /** onset 灵敏度，建议 0.22～0.35；过小易噪，过大民谣偏弱。 */
    void setOnsetThreshold(float value);
    /** aubio 静音门限（dB），如 -70；整体音量偏低时可略调高（如 -75）。 */
    void setOnsetSilenceDb(float value);

signals:
    void beatDetected();

private:
    void releaseAnalysisEngine();
    bool ensureAnalysisEngine(int sampleRate);

    aubio_tempo_t *m_tempo = nullptr;
    aubio_onset_t *m_onset = nullptr;
    fvec_t *m_inputBuf = nullptr;
    fvec_t *m_outputBuf = nullptr;
    QVector<float> m_pending;
    qint64 m_lastBeatTime = 0;

    uint_t m_sampleRate = 0;
    float m_onsetThreshold = 0.3f;
    float m_onsetSilenceDb = -70.0f;

    quint64 m_statRawTempo = 0;
    quint64 m_statRawOnset = 0;
    quint64 m_statEmit = 0;
    quint64 m_statSuppressed = 0;
    qint64 m_lastStatsLogMs = 0;

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;
};
