#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QDebug> // feedBuffer 临时能量日志；与下方节流宏配合，调完阈值可删

#include <cmath>

namespace {

/**
 * 将本缓冲区内所有采样转为等效 float（Float 原样；Int16 /32768），
 * 再算整段 RMS = sqrt(mean(sample^2))。不支持格式返回 0。
 */
float computeFrameRmsAsFloat(const QAudioBuffer &buffer)
{
    if (!buffer.isValid()) {
        return 0.0f;
    }

    const QAudioFormat format = buffer.format();
    const int channelCount = format.channelCount();
    const int frameCount = buffer.frameCount();
    if (channelCount <= 0 || frameCount <= 0) {
        return 0.0f;
    }

    // 交错 PCM：总标量采样数 = 帧数 × 声道数
    const int sampleCount = frameCount * channelCount;
    if (sampleCount <= 0) {
        return 0.0f;
    }

    double sumSq = 0.0;

#if QT_VERSION_MAJOR >= 6
    // Qt 6.11+：仅模板 constData<T>() 为公开 API；勿调用无模板 constData()（已 private）。
    switch (format.sampleFormat()) {
    case QAudioFormat::Float: {
        const float *data = buffer.constData<float>();
        if (data == nullptr) {
            return 0.0f;
        }
        for (int i = 0; i < sampleCount; ++i) {
            const float s = data[i];
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }
        break;
    }
    case QAudioFormat::Int16: {
        const qint16 *data = buffer.constData<qint16>();
        if (data == nullptr) {
            return 0.0f;
        }
        constexpr float kInv = 1.0f / 32768.0f;
        for (int i = 0; i < sampleCount; ++i) {
            const float s = static_cast<float>(data[i]) * kInv;
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }
        break;
    }
    default:
        return 0.0f;
    }
#else
    const int bytesPerFrame = format.bytesPerFrame();
    if (bytesPerFrame <= 0) {
        return 0.0f;
    }
    const char *bytes = reinterpret_cast<const char *>(buffer.constData());
    if (bytes == nullptr) {
        return 0.0f;
    }

    switch (format.sampleType()) {
    case QAudioFormat::Float: {
        if (format.sampleSize() / 8 != static_cast<int>(sizeof(float))) {
            return 0.0f;
        }
        for (int f = 0; f < frameCount; ++f) {
            const float *frame = reinterpret_cast<const float *>(bytes + f * bytesPerFrame);
            for (int ch = 0; ch < channelCount; ++ch) {
                const float s = frame[ch];
                sumSq += static_cast<double>(s) * static_cast<double>(s);
            }
        }
        break;
    }
    case QAudioFormat::SignedInt: {
        if (format.sampleSize() != 16) {
            return 0.0f;
        }
        constexpr float kInv = 1.0f / 32768.0f;
        for (int f = 0; f < frameCount; ++f) {
            const qint16 *frame = reinterpret_cast<const qint16 *>(bytes + f * bytesPerFrame);
            for (int ch = 0; ch < channelCount; ++ch) {
                const float s = static_cast<float>(frame[ch]) * kInv;
                sumSq += static_cast<double>(s) * static_cast<double>(s);
            }
        }
        break;
    }
    default:
        return 0.0f;
    }
#endif

    const double meanSq = sumSq / static_cast<double>(sampleCount);
    if (meanSq <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(std::sqrt(meanSq));
}

} // namespace

BeatDetector::BeatDetector(QObject *parent)
    : QObject(parent)
    , m_energyHistory()
{
    m_energyHistory.reserve(WINDOW);
}

void BeatDetector::feedBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0) {
        return;
    }

    // 运行期确认 PCM 是否到达：默认仅 Debug 输出；Release 需跟踪时在 CMake 加 MUSICPLAYER_BEATDETECTOR_TRACE
#if defined(MUSICPLAYER_BEATDETECTOR_TRACE) || defined(QT_DEBUG)
    static int s_feedLogCount = 0;
    ++s_feedLogCount;
    if (s_feedLogCount <= 12 || (s_feedLogCount % 128) == 0) {
        qDebug() << QStringLiteral("[BeatDetector] feedBuffer call#%1 frames=%2")
                        .arg(s_feedLogCount)
                        .arg(buffer.frameCount());
    }
#endif

    // 1) PCM → float 等效采样并算本帧 RMS
    const float rms = computeFrameRmsAsFloat(buffer);

    // 2) 写入滑动窗，超长则丢最旧
    m_energyHistory.append(rms);
    while (m_energyHistory.size() > WINDOW) {
        m_energyHistory.removeFirst();
    }

    // 3) 冷启动：未满 WINDOW 帧不参与阈值判定
    if (m_energyHistory.size() < WINDOW) {
        return;
    }

    // 4) 当前窗内（含本帧）能量的算术平均
    double sum = 0.0;
    for (float e : m_energyHistory) {
        sum += static_cast<double>(e);
    }
    const float avg = static_cast<float>(sum / static_cast<double>(WINDOW));

    // 临时：观察 rms / avg / 判定阈值（含本帧的 avg）；调阈值或 RMS 后删除整段
    static int s_energyLogCount = 0;
    if (++s_energyLogCount <= 15 || (s_energyLogCount % 128) == 0) {
        qDebug() << "[Beat] rms=" << rms << "avg=" << avg << "threshold=" << (avg * THRESHOLD);
    }

    // 5) 相对均值突增 + 防抖：距上次触发须超过 MIN_INTERVAL ms
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (rms > avg * THRESHOLD && (now - m_lastBeatTime) > static_cast<qint64>(MIN_INTERVAL)) {
        m_lastBeatTime = now;
        emit beatDetected();
    }
}
