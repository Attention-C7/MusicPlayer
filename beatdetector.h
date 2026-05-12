#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 使用 aubio 节拍跟踪：解码 PCM 为 float，按 hop 送入 new_aubio_tempo；
 * 与 QMediaPlayer 实际采样率不一致时（固定 44100）节拍精度会受影响，部署前宜对齐或重采样。
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    explicit BeatDetector(QObject *parent = nullptr);
    ~BeatDetector() override;

    void feedBuffer(const QAudioBuffer &buffer);

signals:
    void beatDetected();

private:
    aubio_tempo_t *m_tempo = nullptr;
    fvec_t *m_inputBuf = nullptr;
    fvec_t *m_outputBuf = nullptr;
    QVector<float> m_pending;

    static constexpr uint_t HOP_SIZE = 512;
    static constexpr uint_t WIN_SIZE = 1024;
    static constexpr uint_t SAMPLE_RATE = 44100;
};
