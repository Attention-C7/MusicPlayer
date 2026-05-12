#include "beatdetector.h"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QDebug>

#include <cstring>

namespace {

/** 取第 0 声道 mono float：Float 直接按交错指针步进 channelCount；Int16 除 32768。 */
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
            out->append(data[f * channelCount]);
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
            out->append(static_cast<float>(data[f * channelCount]) * kInv);
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
            out->append(frame[0]);
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
            out->append(static_cast<float>(frame[0]) * kInv);
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
    m_inputBuf = new_fvec(HOP_SIZE);
    m_outputBuf = new_fvec(2);
    m_tempo = new_aubio_tempo("default", WIN_SIZE, HOP_SIZE, SAMPLE_RATE);
    if (m_inputBuf == nullptr || m_outputBuf == nullptr || m_tempo == nullptr) {
        if (m_tempo != nullptr) {
            del_aubio_tempo(m_tempo);
            m_tempo = nullptr;
        }
        if (m_outputBuf != nullptr) {
            del_fvec(m_outputBuf);
            m_outputBuf = nullptr;
        }
        if (m_inputBuf != nullptr) {
            del_fvec(m_inputBuf);
            m_inputBuf = nullptr;
        }
    }
}

BeatDetector::~BeatDetector()
{
    if (m_tempo != nullptr) {
        del_aubio_tempo(m_tempo);
        m_tempo = nullptr;
    }
    if (m_inputBuf != nullptr) {
        del_fvec(m_inputBuf);
        m_inputBuf = nullptr;
    }
    if (m_outputBuf != nullptr) {
        del_fvec(m_outputBuf);
        m_outputBuf = nullptr;
    }
}

void BeatDetector::feedBuffer(const QAudioBuffer &buffer)
{
    if (m_tempo == nullptr || m_inputBuf == nullptr || m_outputBuf == nullptr) {
        return;
    }
    if (!buffer.isValid() || buffer.frameCount() <= 0) {
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

        if (m_outputBuf->data[0] != static_cast<smpl_t>(0)) {
            qDebug() << "[aubio] beat! BPM=" << aubio_tempo_get_bpm(m_tempo);
            emit beatDetected();
        }

        m_pending.remove(0, hop);
    }
}
