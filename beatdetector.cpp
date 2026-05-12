#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QDebug>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

/** 硬下限：未锁相时最小发射间隔（毫秒），对应样本域防抖基线。 */
constexpr qint64 kHardMinIntervalMs = 260;
/** tempo 拍：aubio 二值 + 置信度门限（成员 m_tempoConfTrigger，随灵敏度预设变化）。 */
constexpr float kTempoBpmHalvingAbove = 150.0f;

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

float BeatDetector::calculateBeatIntensity(bool onsetHit, float onsetValue, float tempoConf, bool isPredicted) const
{
    Q_UNUSED(onsetHit);
    if (isPredicted) {
        return 0.15f;
    }
    const float onsetNorm = std::min(1.0f, onsetValue / (m_onsetPeakEstimate + 0.001f));
    const float conf = tempoConf > 0.01f ? tempoConf : 0.2f;
    return std::clamp(0.3f * conf + 0.7f * onsetNorm, 0.2f, 1.0f);
}

float BeatDetector::calculatePredictedIntensity() const
{
    return 0.15f;
}

void BeatDetector::resetPllState()
{
    m_totalSamples = 0;
    m_lastBeatSample = -1;
    m_nextBeatSample = -1;
    if (m_sampleRate > 0) {
        m_beatPeriodSamples = static_cast<float>(m_sampleRate) * 60.0f / 120.0f;
    } else {
        m_beatPeriodSamples = 44100.0f * 60.0f / 120.0f;
    }
    m_phaseLocked = false;
    m_strongBeatCount = 0;
    m_predictBeatCount = 0;
    notifyBpmUpdated();
}

void BeatDetector::notifyBpmUpdated()
{
    if (m_sampleRate <= 0 || m_beatPeriodSamples <= 1e-6f) {
        return;
    }
    const float bpm = static_cast<float>(m_sampleRate) * 60.0f / m_beatPeriodSamples;
    emit bpmUpdated(bpm);
}

BeatDetector::BeatDetector(QObject *parent)
    : QObject(parent)
{
    setObjectName(QStringLiteral("BeatDetector"));
    setSensitivity(Normal);
}

void BeatDetector::setSensitivity(Sensitivity s)
{
    m_sensitivity = s;
    float th = 0.3f;
    float tconf = 0.1f;
    float pscale = 0.5f;
    float fl = 0.08f;
    switch (s) {
    case Normal:
        th = 0.30f;
        tconf = 0.10f;
        pscale = 0.50f;
        fl = 0.08f;
        break;
    case BoostWeak:
        th = 0.24f;
        tconf = 0.07f;
        pscale = 0.40f;
        fl = 0.06f;
        break;
    case ReduceFalse:
        th = 0.36f;
        tconf = 0.14f;
        pscale = 0.60f;
        fl = 0.10f;
        break;
    default:
        th = 0.30f;
        tconf = 0.10f;
        pscale = 0.50f;
        fl = 0.08f;
        m_sensitivity = Normal;
        break;
    }
    m_tempoConfTrigger = tconf;
    m_dynPercentileScale = pscale;
    m_onsetFloorSoft = fl;
    if (th < 0.05f) {
        th = 0.05f;
    } else if (th > 0.95f) {
        th = 0.95f;
    }
    m_onsetThreshold = th;
    if (m_onset != nullptr) {
        aubio_onset_set_threshold(m_onset, static_cast<smpl_t>(m_onsetThreshold));
    }
    resetOnsetDynamics();
}

BeatDetector::~BeatDetector()
{
    releaseAnalysisEngine();
}

void BeatDetector::resetOnsetDynamics()
{
    m_onsetHistory.clear();
    m_dynamicOnsetThreshold = m_onsetThreshold;
    m_onsetPeakEstimate = m_onsetThreshold;
    m_onsetMaxRecent = 0.1f;
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
    if (m_onsetOut != nullptr) {
        del_fvec(m_onsetOut);
        m_onsetOut = nullptr;
    }
    m_sampleRate = 0;
    m_pending.clear();
    resetOnsetDynamics();
}

bool BeatDetector::ensureAnalysisEngine(int sampleRate)
{
    if (sampleRate <= 0) {
        return false;
    }
    const uint_t sr = static_cast<uint_t>(sampleRate);
    if (m_tempo != nullptr && m_inputBuf != nullptr && m_outputBuf != nullptr && m_onsetOut != nullptr
        && m_sampleRate == sr) {
        return true;
    }

    releaseAnalysisEngine();

    m_inputBuf = new_fvec(HOP_SIZE);
    m_outputBuf = new_fvec(2);
    m_onsetOut = new_fvec(1);
    m_tempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, sr);
    m_onset = new_aubio_onset("specflux", WIN_SIZE, HOP_SIZE, sr);

    if (m_inputBuf == nullptr || m_outputBuf == nullptr || m_onsetOut == nullptr || m_tempo == nullptr) {
        releaseAnalysisEngine();
        return false;
    }

    if (m_onset != nullptr) {
        aubio_onset_set_threshold(m_onset, static_cast<smpl_t>(m_onsetThreshold));
        aubio_onset_set_silence(m_onset, static_cast<smpl_t>(m_onsetSilenceDb));
    }

    m_sampleRate = sr;
    resetPllState();
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
        const qint64 hopEndSample = m_totalSamples + hop;

        std::memcpy(
            m_inputBuf->data,
            m_pending.constData(),
            static_cast<size_t>(hop) * sizeof(smpl_t));

        aubio_tempo_do(m_tempo, m_inputBuf, m_outputBuf);
        const bool tempoHit = (m_outputBuf->data[0] != static_cast<smpl_t>(0))
            && (aubio_tempo_get_confidence(m_tempo) >= m_tempoConfTrigger);
        const float tempoConf = tempoHit ? aubio_tempo_get_confidence(m_tempo) : 0.0f;
        float tempoBpm = tempoHit ? aubio_tempo_get_bpm(m_tempo) : 0.0f;
        if (tempoHit && tempoBpm > kTempoBpmHalvingAbove) {
            tempoBpm *= 0.5f;
        }

        float onsetValue = 0.0f;
        float onsetNorm = 0.0f;
        bool onsetHit = false;
        if (m_onset != nullptr && m_onsetOut != nullptr) {
            aubio_onset_do(m_onset, m_inputBuf, m_onsetOut);
            onsetValue = static_cast<float>(aubio_onset_get_descriptor(m_onset));

            m_onsetMaxRecent = std::max(onsetValue, m_onsetMaxRecent * 0.995f);
            if (m_onsetMaxRecent < 0.1f) {
                m_onsetMaxRecent = 0.1f;
            }
            onsetNorm = onsetValue / m_onsetMaxRecent;
            if (onsetNorm > 1.0f) {
                onsetNorm = 1.0f;
            }

            m_onsetHistory.push_back(onsetNorm);
            if (static_cast<int>(m_onsetHistory.size()) > kOnsetHistoryMax) {
                m_onsetHistory.pop_front();
            }

            if (!m_onsetHistory.empty()) {
                std::vector<float> sorted(m_onsetHistory.begin(), m_onsetHistory.end());
                std::sort(sorted.begin(), sorted.end());
                const size_t n = sorted.size();
                const size_t mid = n / 2u;
                const float median = sorted.at(mid);
                m_onsetPeakEstimate = m_onsetPeakEstimate * 0.99f
                    + (std::max(m_onsetPeakEstimate, median) * 0.01f);
                const float medianScaled = median * 1.2f;
                const float medianThresh = std::min(1.0f, medianScaled);
                m_dynamicOnsetThreshold = std::max(
                    {m_onsetThreshold, m_onsetFloorSoft, medianThresh});
            }

            m_statDynThresholdSum += static_cast<double>(m_dynamicOnsetThreshold);
            ++m_statDynThresholdCount;

            onsetHit = (onsetNorm >= m_dynamicOnsetThreshold);
        }

        if (tempoHit) {
            ++m_statRawTempo;
        }
        if (onsetHit) {
            ++m_statRawOnset;
        }

        const bool beatCandidate = tempoHit || onsetHit;

        float dynamicMinGapMs = static_cast<float>(kHardMinIntervalMs);
        if (m_phaseLocked && m_strongBeatCount >= 8) {
            const float periodMs = (m_beatPeriodSamples / static_cast<float>(m_sampleRate)) * 1000.0f;
            dynamicMinGapMs = std::max(180.0f, periodMs * 0.5f);
        }
        const qint64 minGapSamples = static_cast<qint64>(
            static_cast<float>(m_sampleRate) * dynamicMinGapMs / 1000.0f);
        const bool beatTooSoon = (m_lastBeatSample >= 0 && (m_totalSamples - m_lastBeatSample) < minGapSamples);

        bool emittedReal = false;
        if (beatCandidate) {
            if (beatTooSoon) {
                ++m_statSuppressed;
            } else {
                ++m_statEmit;
                emittedReal = true;
                emit beatDetected(calculateBeatIntensity(onsetHit, onsetNorm, tempoConf, false));

                if (m_lastBeatSample >= 0) {
                    if (tempoConf > 0.25f || onsetNorm > 0.5f) {
                        const float intervalSamples = static_cast<float>(hopEndSample - m_lastBeatSample);
                        m_beatPeriodSamples = 0.3f * intervalSamples + 0.7f * m_beatPeriodSamples;
                        float bpm = static_cast<float>(m_sampleRate) * 60.0f / m_beatPeriodSamples;
                        bpm = std::clamp(bpm, 60.0f, 180.0f);
                        m_beatPeriodSamples = static_cast<float>(m_sampleRate) * 60.0f / bpm;
                        notifyBpmUpdated();
                    }
                }
                m_lastBeatSample = hopEndSample;
                ++m_strongBeatCount;
                if (m_strongBeatCount >= 4) {
                    m_phaseLocked = true;
                }
                m_predictBeatCount = 0;
                m_nextBeatSample = m_lastBeatSample + static_cast<qint64>(m_beatPeriodSamples + 0.5f);
            }
        }

        if (!emittedReal && !beatTooSoon && m_phaseLocked && m_predictBeatCount < 2) {
            const float currentBpm = (m_sampleRate > 0 && m_beatPeriodSamples > 1e-6f)
                ? (static_cast<float>(m_sampleRate) * 60.0f / m_beatPeriodSamples)
                : 0.0f;
            if (currentBpm <= 180.0f) {
                const qint64 earlyMargin = static_cast<qint64>(m_beatPeriodSamples * 0.2f + 0.5f);
                if (hopEndSample >= m_nextBeatSample - earlyMargin) {
                    ++m_statEmit;
                    ++m_statEmitPredicted;
                    emit beatDetected(calculatePredictedIntensity());
                    m_lastBeatSample = hopEndSample;
                    m_nextBeatSample += static_cast<qint64>(m_beatPeriodSamples + 0.5f);
                    ++m_predictBeatCount;
                    if (m_predictBeatCount >= 2) {
                        m_phaseLocked = false;
                        m_strongBeatCount = 0;
                        m_predictBeatCount = 0;
                    }
                }
            }
        }

        m_totalSamples = hopEndSample;
        m_pending.remove(0, hop);
    }

    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    if (m_lastStatsLogMs == 0) {
        m_lastStatsLogMs = wall;
    } else if (wall - m_lastStatsLogMs >= kStatsLogIntervalMs) {
        const double avgDynThresh = (m_statDynThresholdCount > 0)
            ? (m_statDynThresholdSum / static_cast<double>(m_statDynThresholdCount))
            : static_cast<double>(m_dynamicOnsetThreshold);
        const double predictRatio = (m_statEmit > 0)
            ? (static_cast<double>(m_statEmitPredicted) / static_cast<double>(m_statEmit))
            : 0.0;
        float curBpm = 0.0f;
        if (m_sampleRate > 0 && m_beatPeriodSamples > 1e-6f) {
            curBpm = static_cast<float>(m_sampleRate) * 60.0f / m_beatPeriodSamples;
        }

        qDebug().nospace()
            << "[BeatDetector] " << (kStatsLogIntervalMs / 1000) << "s | "
            << "sr=" << static_cast<int>(m_sampleRate)
            << " | rawTempo=" << m_statRawTempo
            << " | rawOnset=" << m_statRawOnset
            << " | emit=" << m_statEmit
            << " (predict=" << m_statEmitPredicted
            << ", ratio=" << QString::number(predictRatio, 'f', 3) << ")"
            << " | suppressed=" << m_statSuppressed
            << " | avgDynThresh=" << QString::number(avgDynThresh, 'f', 4)
            << " | curBpm=" << QString::number(static_cast<double>(curBpm), 'f', 1)
            << " | phaseLocked=" << (m_phaseLocked ? "yes" : "no");

        m_statRawTempo = 0;
        m_statRawOnset = 0;
        m_statEmit = 0;
        m_statEmitPredicted = 0;
        m_statSuppressed = 0;
        m_statDynThresholdSum = 0.0;
        m_statDynThresholdCount = 0;
        m_lastStatsLogMs = wall;
    }
}
