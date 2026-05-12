#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDateTime>
#include <QVector>

#include <cmath>

namespace {

/** 将本缓冲区内交错标量采样解码为 float（Float 原样；Int16 /32768）。失败返回 false。 */
bool decodeInterleavedFloats(const QAudioBuffer &buffer, QVector<float> *out)
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

    const int sampleCount = frameCount * channelCount;
    if (sampleCount <= 0) {
        return false;
    }

    out->reserve(sampleCount);

#if QT_VERSION_MAJOR >= 6
    switch (format.sampleFormat()) {
    case QAudioFormat::Float: {
        const float *data = buffer.constData<float>();
        if (data == nullptr) {
            return false;
        }
        for (int i = 0; i < sampleCount; ++i) {
            out->append(data[i]);
        }
        return true;
    }
    case QAudioFormat::Int16: {
        const qint16 *data = buffer.constData<qint16>();
        if (data == nullptr) {
            return false;
        }
        constexpr float kInv = 1.0f / 32768.0f;
        for (int i = 0; i < sampleCount; ++i) {
            out->append(static_cast<float>(data[i]) * kInv);
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
    const char *bytes = reinterpret_cast<const char *>(buffer.constData());
    if (bytes == nullptr) {
        return false;
    }

    switch (format.sampleType()) {
    case QAudioFormat::Float: {
        if (format.sampleSize() / 8 != static_cast<int>(sizeof(float))) {
            return false;
        }
        for (int f = 0; f < frameCount; ++f) {
            const float *frame = reinterpret_cast<const float *>(bytes + f * bytesPerFrame);
            for (int ch = 0; ch < channelCount; ++ch) {
                out->append(frame[ch]);
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
            const qint16 *frame = reinterpret_cast<const qint16 *>(bytes + f * bytesPerFrame);
            for (int ch = 0; ch < channelCount; ++ch) {
                out->append(static_cast<float>(frame[ch]) * kInv);
            }
        }
        return true;
    }
    default:
        return false;
    }
#endif
}

/**
 * 每 DOWNSAMPLE_STRIDE 个采样取算术均值得到一个低频采样；对整帧低频序列求 RMS。
 * 尾部不足一组的标量丢弃。组数为 0 时返回 false。
 */
bool computeDownsampledLowRms(const QVector<float> &samples, int stride, float *outLowRms)
{
    if (stride <= 0 || outLowRms == nullptr) {
        return false;
    }
    const int n = samples.size();
    const int groups = n / stride;
    if (groups <= 0) {
        return false;
    }

    double sumSq = 0.0;
    for (int g = 0; g < groups; ++g) {
        double mean = 0.0;
        const int base = g * stride;
        for (int k = 0; k < stride; ++k) {
            mean += static_cast<double>(samples.at(base + k));
        }
        mean /= static_cast<double>(stride);
        sumSq += mean * mean;
    }

    *outLowRms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(groups)));
    return true;
}

} // namespace

BeatDetector::BeatDetector(QObject *parent)
    : QObject(parent)
{
}

void BeatDetector::feedBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0) {
        return;
    }

    QVector<float> samples;
    if (!decodeInterleavedFloats(buffer, &samples)) {
        return;
    }

    float lowRms = 0.0f;
    if (!computeDownsampledLowRms(samples, DOWNSAMPLE_STRIDE, &lowRms)) {
        return;
    }

    if (!m_hasPrevLowRms) {
        m_prevLowRms = lowRms;
        m_hasPrevLowRms = true;
        return;
    }

    const float diff = lowRms - m_prevLowRms;
    m_prevLowRms = lowRms;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (diff > DIFF_THRESHOLD && (now - m_lastBeatTime) > static_cast<qint64>(MIN_INTERVAL_MS)) {
        m_lastBeatTime = now;
        emit beatDetected();
    }
}
