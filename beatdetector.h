#pragma once

#include <QObject>
#include <QVector>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 使用 aubio 节拍跟踪：单声道 float 按 hop 送入 new_aubio_tempo（WIN/HOP 见常量）；
 * 对 aubio 输出做置信度过滤，并按 BPM（>150 折半）推算最小间隔防抖假拍。
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
    qint64 m_lastBeatTime = 0;

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;
    static constexpr uint_t SAMPLE_RATE = 44100;
};
