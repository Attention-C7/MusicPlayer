#include "beatanalyzer.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QUrl>
#include <QtGlobal>

BeatAnalyzer::BeatAnalyzer(QObject *parent)
    : QObject(parent)
    , m_decoder(new QAudioDecoder(this))
    , m_binMs(80)
    , m_ready(false)
    , m_analyzing(false)
    , m_channelCount(0)
    , m_sampleRate(0)
    , m_targetSamplesPerBin(0)
    , m_energySum(0.0f)
    , m_energyCount(0)
{
    connect(m_decoder, &QAudioDecoder::bufferReady, this, [this]() {
        if (!m_decoder->bufferAvailable()) {
            return;
        }
        processBuffer(m_decoder->read());
    });

    connect(m_decoder, &QAudioDecoder::finished, this, [this]() {
        if (!m_analyzing) {
            return;
        }
        finalizeAnalysis();
    });

    connect(m_decoder, &QAudioDecoder::error, this, [this](QAudioDecoder::Error error) {
        if (error == QAudioDecoder::NoError) {
            return;
        }
        m_analyzing = false;
        m_ready = false;
        m_bins.clear();
        emit analyzeFailed(m_decoder->errorString());
    });
}

void BeatAnalyzer::analyze(QString filePath)
{
    const QString path = filePath.trimmed();
    if (path.isEmpty()) {
        emit analyzeFailed(QStringLiteral("音频路径为空"));
        return;
    }

    if (m_analyzing) {
        m_decoder->stop();
    }

    resetState();
    m_currentFilePath = path;
    m_analyzing = true;
    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
}

float BeatAnalyzer::intensityAt(qint64 ms) const
{
    if (!m_ready || m_bins.isEmpty() || ms < 0 || m_binMs <= 0) {
        return 0.0f;
    }

    int index = static_cast<int>(ms / m_binMs);
    if (index < 0) {
        index = 0;
    }
    if (index >= m_bins.size()) {
        index = m_bins.size() - 1;
    }
    return m_bins.at(index);
}

bool BeatAnalyzer::isReady() const
{
    return m_ready;
}

bool BeatAnalyzer::isAnalyzing() const
{
    return m_analyzing;
}

void BeatAnalyzer::resetState()
{
    m_bins.clear();
    m_ready = false;
    m_channelCount = 0;
    m_sampleRate = 0;
    m_targetSamplesPerBin = 0;
    m_energySum = 0.0f;
    m_energyCount = 0;
}

void BeatAnalyzer::processBuffer(const QAudioBuffer &buffer)
{
    const QAudioFormat format = buffer.format();
    if (!format.isValid()) {
        return;
    }

    if (m_channelCount <= 0) {
        m_channelCount = format.channelCount();
    }
    if (m_sampleRate <= 0) {
        m_sampleRate = format.sampleRate();
    }
    if (m_targetSamplesPerBin <= 0) {
        const qint64 samplesPerChannel = static_cast<qint64>(m_sampleRate) * m_binMs / 1000;
        m_targetSamplesPerBin = qMax<qint64>(1, samplesPerChannel * qMax(1, m_channelCount));
    }

    const int sampleCount = buffer.sampleCount();
    if (sampleCount <= 0) {
        return;
    }

    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        const quint8 *data = buffer.constData<quint8>();
        for (int i = 0; i < sampleCount; ++i) {
            m_energySum += normalizeSample(data[i]);
            ++m_energyCount;
            if (m_energyCount >= m_targetSamplesPerBin) {
                m_bins.append(m_energySum / static_cast<float>(m_energyCount));
                m_energySum = 0.0f;
                m_energyCount = 0;
            }
        }
        break;
    }
    case QAudioFormat::Int16: {
        const qint16 *data = buffer.constData<qint16>();
        for (int i = 0; i < sampleCount; ++i) {
            m_energySum += normalizeSample(data[i]);
            ++m_energyCount;
            if (m_energyCount >= m_targetSamplesPerBin) {
                m_bins.append(m_energySum / static_cast<float>(m_energyCount));
                m_energySum = 0.0f;
                m_energyCount = 0;
            }
        }
        break;
    }
    case QAudioFormat::Int32: {
        const qint32 *data = buffer.constData<qint32>();
        for (int i = 0; i < sampleCount; ++i) {
            m_energySum += normalizeSample(data[i]);
            ++m_energyCount;
            if (m_energyCount >= m_targetSamplesPerBin) {
                m_bins.append(m_energySum / static_cast<float>(m_energyCount));
                m_energySum = 0.0f;
                m_energyCount = 0;
            }
        }
        break;
    }
    case QAudioFormat::Float: {
        const float *data = buffer.constData<float>();
        for (int i = 0; i < sampleCount; ++i) {
            m_energySum += normalizeSample(data[i]);
            ++m_energyCount;
            if (m_energyCount >= m_targetSamplesPerBin) {
                m_bins.append(m_energySum / static_cast<float>(m_energyCount));
                m_energySum = 0.0f;
                m_energyCount = 0;
            }
        }
        break;
    }
    default:
        break;
    }
}

void BeatAnalyzer::finalizeAnalysis()
{
    if (m_energyCount > 0) {
        m_bins.append(m_energySum / static_cast<float>(m_energyCount));
    }

    if (m_bins.isEmpty()) {
        m_analyzing = false;
        m_ready = false;
        emit analyzeFailed(QStringLiteral("未能提取节奏强度"));
        return;
    }

    float maxValue = 0.0f;
    for (float value : m_bins) {
        if (value > maxValue) {
            maxValue = value;
        }
    }

    if (maxValue > 0.0f) {
        for (float &value : m_bins) {
            value = qBound(0.0f, value / maxValue, 1.0f);
        }
    } else {
        for (float &value : m_bins) {
            value = 0.0f;
        }
    }

    QVector<float> smoothed = m_bins;
    for (int i = 0; i < m_bins.size(); ++i) {
        float sum = m_bins[i];
        int count = 1;
        if (i > 0) {
            sum += m_bins[i - 1];
            ++count;
        }
        if (i + 1 < m_bins.size()) {
            sum += m_bins[i + 1];
            ++count;
        }
        smoothed[i] = sum / static_cast<float>(count);
    }
    m_bins = smoothed;

    m_analyzing = false;
    m_ready = true;
    emit analyzeFinished();
}

float BeatAnalyzer::normalizeSample(qint16 value) const
{
    return qAbs(static_cast<float>(value) / 32768.0f);
}

float BeatAnalyzer::normalizeSample(qint32 value) const
{
    return qAbs(static_cast<float>(value) / 2147483648.0f);
}

float BeatAnalyzer::normalizeSample(float value) const
{
    return qBound(0.0f, qAbs(value), 1.0f);
}

float BeatAnalyzer::normalizeSample(quint8 value) const
{
    const float centered = (static_cast<float>(value) - 128.0f) / 128.0f;
    return qAbs(centered);
}
