#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QtDebug>

#include <cmath>

namespace {

bool bufferToMonoFloatSamples(const QAudioBuffer &buffer, QVector<float> *out)
{
    out->clear();
    if (!buffer.isValid()) {
        return false;
    }

    const QAudioFormat format = buffer.format();
    const int channelCount = format.channelCount();
    const int frameCount = buffer.frameCount();
    if (channelCount <= 0 || frameCount <= 0) {
        return false;
    }

    out->reserve(frameCount);

#if QT_VERSION_MAJOR >= 6
    switch (format.sampleFormat()) {
    case QAudioFormat::Float: {
        const float *const data = reinterpret_cast<const float *>(buffer.constData<float>());
        if (data == nullptr) {
            return false;
        }
        for (int f = 0; f < frameCount; ++f) {
            const float *const frame = data + f * channelCount;
            if (channelCount >= 2) {
                out->append(0.5f * (frame[0] + frame[1]));
            } else {
                out->append(frame[0]);
            }
        }
        return true;
    }
    case QAudioFormat::Int16: {
        const qint16 *const data = reinterpret_cast<const qint16 *>(buffer.constData<qint16>());
        if (data == nullptr) {
            return false;
        }
        constexpr float kInv = 1.0f / 32768.0f;
        for (int f = 0; f < frameCount; ++f) {
            const qint16 *const frame = data + f * channelCount;
            if (channelCount >= 2) {
                out->append(0.5f * (static_cast<float>(frame[0]) + static_cast<float>(frame[1])) * kInv);
            } else {
                out->append(static_cast<float>(frame[0]) * kInv);
            }
        }
        return true;
    }
    case QAudioFormat::Int32: {
        const qint32 *const data = reinterpret_cast<const qint32 *>(buffer.constData<qint32>());
        if (data == nullptr) {
            return false;
        }
        constexpr float kInv = 1.0f / 2147483648.0f;
        for (int f = 0; f < frameCount; ++f) {
            const qint32 *const frame = data + f * channelCount;
            if (channelCount >= 2) {
                out->append(0.5f * (static_cast<float>(frame[0]) + static_cast<float>(frame[1])) * kInv);
            } else {
                out->append(static_cast<float>(frame[0]) * kInv);
            }
        }
        return true;
    }
    case QAudioFormat::UInt8: {
        const quint8 *const data = reinterpret_cast<const quint8 *>(buffer.constData<quint8>());
        if (data == nullptr) {
            return false;
        }
        constexpr float kInv = 1.0f / 128.0f;
        for (int f = 0; f < frameCount; ++f) {
            const quint8 *const frame = data + f * channelCount;
            if (channelCount >= 2) {
                out->append(
                    0.5f * ((static_cast<float>(frame[0]) - 128.0f) + (static_cast<float>(frame[1]) - 128.0f))
                            * kInv);
            } else {
                out->append((static_cast<float>(frame[0]) - 128.0f) * kInv);
            }
        }
        return true;
    }
    default:
        return false;
    }
#else
    const int bytesPerFrame = format.bytesPerFrame();
    if (bytesPerFrame <= 0) {
        return false;
    }
    const char *const bytes = reinterpret_cast<const char *>(buffer.constData());
    if (bytes == nullptr) {
        return false;
    }

    switch (format.sampleType()) {
    case QAudioFormat::Float: {
        if (format.sampleSize() / 8 != static_cast<int>(sizeof(float))) {
            return false;
        }
        for (int f = 0; f < frameCount; ++f) {
            const float *const frame = reinterpret_cast<const float *>(bytes + f * bytesPerFrame);
            if (channelCount >= 2) {
                out->append(0.5f * (frame[0] + frame[1]));
            } else {
                out->append(frame[0]);
            }
        }
        return true;
    }
    case QAudioFormat::SignedInt: {
        if (format.sampleSize() != 16) {
            return false;
        }
        constexpr float kInv = 1.0f / 32768.0f;
        for (int f = 0; f < frameCount; ++f) {
            const qint16 *const frame = reinterpret_cast<const qint16 *>(bytes + f * bytesPerFrame);
            if (channelCount >= 2) {
                out->append(0.5f * (static_cast<float>(frame[0]) + static_cast<float>(frame[1])) * kInv);
            } else {
                out->append(static_cast<float>(frame[0]) * kInv);
            }
        }
        return true;
    }
    default:
        return false;
    }
#endif
}

} // namespace

BeatDetector::BeatDetector(QObject *parent)
    : QObject(parent)
{
    setObjectName(QStringLiteral("BeatDetector"));
}

BeatDetector::~BeatDetector()
{
    releaseAnalysisEngine();
}

void BeatDetector::releaseAnalysisEngine()
{
    if (m_aubioTempo != nullptr) {
        del_aubio_tempo(m_aubioTempo);
        m_aubioTempo = nullptr;
    }
    if (m_inputBuf != nullptr) {
        del_fvec(m_inputBuf);
        m_inputBuf = nullptr;
    }
    if (m_outputBuf != nullptr) {
        del_fvec(m_outputBuf);
        m_outputBuf = nullptr;
    }
    m_sampleRate = 0;
    m_pending.clear();
    m_lastBeatWallMs = 0;
}

bool BeatDetector::ensureAnalysisEngine(int sampleRate)
{
    if (sampleRate <= 0) {
        return false;
    }
    const uint_t sr = static_cast<uint_t>(sampleRate);
    if (m_aubioTempo != nullptr && m_inputBuf != nullptr && m_outputBuf != nullptr && m_sampleRate == sr) {
        return true;
    }

    releaseAnalysisEngine();

    /** default + 2048/256 与曾跑通配置一致；采样率用缓冲实际值（勿硬编码 44100，免 48k 等错位）。 */
    m_inputBuf = new_fvec(HOP_SIZE);
    m_outputBuf = new_fvec(2);
    m_aubioTempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, sr);

    if (m_inputBuf == nullptr || m_outputBuf == nullptr || m_aubioTempo == nullptr) {
        releaseAnalysisEngine();
        return false;
    }

    m_sampleRate = sr;
    return true;
}

void BeatDetector::feedBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0) {
        return;
    }

    const int sr = buffer.format().sampleRate();
    if (!ensureAnalysisEngine(sr)) {
        return;
    }

    QVector<float> chunk;
    if (!bufferToMonoFloatSamples(buffer, &chunk)) {
        return;
    }

    m_pending += chunk;

    const int hop = static_cast<int>(HOP_SIZE);
    while (m_pending.size() >= hop) {
        const float *const samples = m_pending.constData();
        for (int i = 0; i < hop; ++i) {
            m_inputBuf->data[i] = static_cast<smpl_t>(samples[i]);
        }

        aubio_tempo_do(m_aubioTempo, m_inputBuf, m_outputBuf);

        const bool beatPickHit = (m_outputBuf->data[0] != static_cast<smpl_t>(0));
        const float bpmRaw = static_cast<float>(aubio_tempo_get_bpm(m_aubioTempo));
        const float conf = static_cast<float>(aubio_tempo_get_confidence(m_aubioTempo));
        float rmsAcc = 0.0f;
        for (int i = 0; i < hop; ++i) {
            const float s = static_cast<float>(m_inputBuf->data[i]);
            rmsAcc += s * s;
        }
        const float rms = std::sqrt(rmsAcc / static_cast<float>(hop));
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "[Beat]"
                 << "hit=" << beatPickHit << "bpm=" << bpmRaw << "conf=" << conf << "rms=" << rms
                 << "lastInterval=" << (now - m_lastBeatWallMs);

        const bool hit = beatPickHit && (conf >= kTempoConfTrigger);

        float bpm = bpmRaw;
        if (bpm > 150.0f) {
            bpm /= 2.0f;
        }
        if (!std::isfinite(bpm) || bpm < 1.0f) {
            bpm = 120.0f;
        }
        /** 理论拍间距 60000/bpm，乘 0.8 防抖。 */
        static constexpr float kBeatIntervalTighten = 0.8f;
        const int minInterval = static_cast<int>(60000.0f / bpm * kBeatIntervalTighten);

        if (hit && (m_lastBeatWallMs == 0 || now - m_lastBeatWallMs >= minInterval)) {
            m_lastBeatWallMs = now;
            emit beatDetected(kTempoIntensity);
        }

        m_pending.remove(0, hop);
    }
}
