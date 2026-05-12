#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QDebug>

#include <cstring>

namespace {

/** 硬下限：任意 emit 之间至少间隔（onset 密、弱 tempo 时主导）。 */
constexpr qint64 kHardMinIntervalMs = 220;
/** tempo 置信度达到此值才启用 BPM 软间隔（略低于触发门限，避免卡在边缘）。 */
constexpr float kTempoConfForSoftGap = 0.22f;
constexpr float kTempoConfTrigger = 0.15f;
constexpr float kTempoBpmHalvingAbove = 150.0f;
/** 软间隔 = 60000/bpm * kSoftGapBeatFraction，与硬下限取 max。 */
constexpr float kSoftGapBeatFraction = 0.45f;
constexpr float kBpmMin = 40.0f;
constexpr float kBpmMax = 220.0f;

constexpr int kStatsLogIntervalMs = 30000;

/** 立体声时 (L+R)/2，单声道取第 0 路；Float / Int16（Qt6）；Qt5 SignedInt/Float。 */
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
    if (m_onset != nullptr) {
        aubio_onset_set_threshold(m_onset, static_cast<smpl_t>(m_onsetThreshold));
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
    if (m_onset != nullptr) {
        aubio_onset_set_silence(m_onset, static_cast<smpl_t>(m_onsetSilenceDb));
    }
}

void BeatDetector::releaseAnalysisEngine()
{
    if (m_tempo != nullptr) {
        del_aubio_tempo(m_tempo);
        m_tempo = nullptr;
    }
    if (m_onset != nullptr) {
        del_aubio_onset(m_onset);
        m_onset = nullptr;
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
}

bool BeatDetector::ensureAnalysisEngine(int sampleRate)
{
    if (sampleRate <= 0) {
        return false;
    }
    const uint_t sr = static_cast<uint_t>(sampleRate);
    if (m_tempo != nullptr && m_inputBuf != nullptr && m_outputBuf != nullptr && m_sampleRate == sr) {
        return true;
    }

    releaseAnalysisEngine();

    m_inputBuf = new_fvec(HOP_SIZE);
    m_outputBuf = new_fvec(2);
    m_tempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, sr);
    m_onset = new_aubio_onset("specflux", WIN_SIZE, HOP_SIZE, sr);

    if (m_inputBuf == nullptr || m_outputBuf == nullptr || m_tempo == nullptr) {
        releaseAnalysisEngine();
        return false;
    }

    if (m_onset != nullptr) {
        aubio_onset_set_threshold(m_onset, static_cast<smpl_t>(m_onsetThreshold));
        aubio_onset_set_silence(m_onset, static_cast<smpl_t>(m_onsetSilenceDb));
    }

    m_sampleRate = sr;
    m_lastBeatTime = 0;
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

        aubio_tempo_do(m_tempo, m_inputBuf, m_outputBuf);
        const bool tempoHit = (m_outputBuf->data[0] != static_cast<smpl_t>(0))
            && (aubio_tempo_get_confidence(m_tempo) >= kTempoConfTrigger);
        const float tempoConf = tempoHit ? aubio_tempo_get_confidence(m_tempo) : 0.0f;
        float tempoBpm = tempoHit ? aubio_tempo_get_bpm(m_tempo) : 0.0f;
        if (tempoHit && tempoBpm > kTempoBpmHalvingAbove) {
            tempoBpm *= 0.5f;
        }

        bool onsetHit = false;
        if (m_onset != nullptr) {
            aubio_onset_do(m_onset, m_inputBuf, m_outputBuf);
            onsetHit = (m_outputBuf->data[0] != static_cast<smpl_t>(0));
        }

        if (tempoHit) {
            ++m_statRawTempo;
        }
        if (onsetHit) {
            ++m_statRawOnset;
        }

        if (tempoHit || onsetHit) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();

            qint64 minGapMs = kHardMinIntervalMs;
            if (tempoHit && tempoConf >= kTempoConfForSoftGap && tempoBpm > kBpmMin && tempoBpm < kBpmMax) {
                const qint64 tempoGap = static_cast<qint64>(60000.0 / static_cast<double>(tempoBpm) * static_cast<double>(kSoftGapBeatFraction));
                if (tempoGap > minGapMs) {
                    minGapMs = tempoGap;
                }
            }

            if (m_lastBeatTime != 0 && (now - m_lastBeatTime) < minGapMs) {
                ++m_statSuppressed;
            } else {
                m_lastBeatTime = now;
                ++m_statEmit;
                emit beatDetected();
            }
        }

        m_pending.remove(0, hop);
    }

    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    if (m_lastStatsLogMs == 0) {
        m_lastStatsLogMs = wall;
    } else if (wall - m_lastStatsLogMs >= kStatsLogIntervalMs) {
        qDebug() << "[BeatDetector] ~30s sr=" << static_cast<int>(m_sampleRate)
                 << "rawTempo=" << m_statRawTempo << "rawOnset=" << m_statRawOnset << "emit=" << m_statEmit
                 << "suppressed=" << m_statSuppressed;
        m_statRawTempo = 0;
        m_statRawOnset = 0;
        m_statEmit = 0;
        m_statSuppressed = 0;
        m_lastStatsLogMs = wall;
    }
}
