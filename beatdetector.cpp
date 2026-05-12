#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>

#include <cstring>

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

void BeatDetector::setOnsetThreshold(float value)
{
    if (value < 0.05f) {
        value = 0.05f;
    } else if (value > 0.95f) {
        value = 0.95f;
    }
    m_onsetThreshold = value;
    if (m_aubioOnset != nullptr) {
        aubio_onset_set_threshold(m_aubioOnset, static_cast<smpl_t>(m_onsetThreshold));
    }
}

void BeatDetector::setOnsetSilenceDb(float value)
{
    if (value > -20.0f) {
        value = -20.0f;
    } else if (value < -120.0f) {
        value = -120.0f;
    }
    m_onsetSilenceDb = value;
    if (m_aubioOnset != nullptr) {
        aubio_onset_set_silence(m_aubioOnset, static_cast<smpl_t>(m_onsetSilenceDb));
    }
}

void BeatDetector::releaseAnalysisEngine()
{
    if (m_aubioTempo != nullptr) {
        del_aubio_tempo(m_aubioTempo);
        m_aubioTempo = nullptr;
    }
    if (m_aubioOnset != nullptr) {
        del_aubio_onset(m_aubioOnset);
        m_aubioOnset = nullptr;
    }
    if (m_inputBuf != nullptr) {
        del_fvec(m_inputBuf);
        m_inputBuf = nullptr;
    }
    if (m_outputBuf != nullptr) {
        del_fvec(m_outputBuf);
        m_outputBuf = nullptr;
    }
    if (m_onsetOut != nullptr) {
        del_fvec(m_onsetOut);
        m_onsetOut = nullptr;
    }
    m_sampleRate = 0;
    m_pending.clear();
    m_hasLastBeat = false;
}

bool BeatDetector::ensureAnalysisEngine(int sampleRate)
{
    if (sampleRate <= 0) {
        return false;
    }
    const uint_t sr = static_cast<uint_t>(sampleRate);
    if (m_aubioTempo != nullptr && m_inputBuf != nullptr && m_outputBuf != nullptr && m_onsetOut != nullptr
        && m_sampleRate == sr) {
        return true;
    }

    releaseAnalysisEngine();

    m_inputBuf = new_fvec(HOP_SIZE);
    m_outputBuf = new_fvec(2);
    m_onsetOut = new_fvec(1);
    m_aubioTempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, sr);
    m_aubioOnset = new_aubio_onset("specflux", WIN_SIZE, HOP_SIZE, sr);

    if (m_inputBuf == nullptr || m_outputBuf == nullptr || m_onsetOut == nullptr || m_aubioTempo == nullptr
        || m_aubioOnset == nullptr) {
        releaseAnalysisEngine();
        return false;
    }

    aubio_onset_set_threshold(m_aubioOnset, static_cast<smpl_t>(m_onsetThreshold));
    aubio_onset_set_silence(m_aubioOnset, static_cast<smpl_t>(m_onsetSilenceDb));

    m_sampleRate = sr;
    m_hasLastBeat = false;
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
        std::memcpy(
            m_inputBuf->data,
            m_pending.constData(),
            static_cast<size_t>(hop) * sizeof(smpl_t));

        aubio_tempo_do(m_aubioTempo, m_inputBuf, m_outputBuf);
        const bool tempoHit = (m_outputBuf->data[0] != static_cast<smpl_t>(0));
        const float tempoConf = aubio_tempo_get_confidence(m_aubioTempo);
        const bool strongTempo = tempoHit && (tempoConf >= kTempoConfTrigger);

        float onsetValue = 0.0f;
        if (m_aubioOnset != nullptr && m_onsetOut != nullptr) {
            aubio_onset_do(m_aubioOnset, m_inputBuf, m_onsetOut);
            onsetValue = static_cast<float>(aubio_onset_get_descriptor(m_aubioOnset));
        }

        const bool intervalOk = !m_hasLastBeat || (m_lastBeatTime.elapsed() >= kHardMinIntervalMs);

        if (strongTempo && intervalOk) {
            emit beatDetected(kTempoIntensity);
            m_lastBeatTime.restart();
            m_hasLastBeat = true;
        } else if (!strongTempo && intervalOk && onsetValue > m_onsetThreshold) {
            emit beatDetected(kOnsetIntensity);
            m_lastBeatTime.restart();
            m_hasLastBeat = true;
        }

        m_pending.remove(0, hop);
    }
}
