#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QDebug> // 能量行日志；调参稳定后可删或改条件编译

#include <algorithm>
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

#if defined(MUSICPLAYER_BEATDETECTOR_TRACE) || defined(QT_DEBUG)
    static int s_feedLogCount = 0;
    ++s_feedLogCount;
    if (s_feedLogCount <= 12 || (s_feedLogCount % 128) == 0) {
        qDebug() << QStringLiteral("[BeatDetector] feedBuffer call#%1 frames=%2")
                        .arg(s_feedLogCount)
                        .arg(buffer.frameCount());
    }
#endif

    // 1) 本帧 RMS（peak）
    const float rms = computeFrameRmsAsFloat(buffer);
    const float peak = rms;

    // 2) 写入滑动窗
    m_energyHistory.append(rms);
    while (m_energyHistory.size() > WINDOW) {
        m_energyHistory.removeFirst();
    }

    // 3) 冷启动：未满 WINDOW 帧不判定
    if (m_energyHistory.size() < WINDOW) {
        return;
    }

    // 4) baseline = 窗内最小 RMS：安静段低，强拍相对比值大（渐进音乐下比「对平均」更敏感）
    const float baseline = *std::min_element(m_energyHistory.cbegin(), m_energyHistory.cend());

    // 临时：观察 peak / baseline / 判定门限
    static int s_energyLogCount = 0;
    if (++s_energyLogCount <= 15 || (s_energyLogCount % 128) == 0) {
        qDebug() << "[Beat] peak=" << peak << "baseline=" << baseline << "threshold=" << (baseline * THRESHOLD);
    }

    // 5) peak 相对 baseline 突增 + 绝对能量门限 + 防抖
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (peak > baseline * THRESHOLD
        && peak > MIN_PEAK_RMS
        && (now - m_lastBeatTime) > static_cast<qint64>(MIN_INTERVAL)) {
        m_lastBeatTime = now;
        emit beatDetected();
    }
}
