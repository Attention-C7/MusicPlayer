#pragma once

#include <QObject>
#include <QVector>
#include <QString>

class QAudioDecoder;
class QAudioBuffer;

class BeatAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit BeatAnalyzer(QObject *parent = nullptr);

    void analyze(QString filePath);
    float intensityAt(qint64 ms) const;
    bool isReady() const;
    bool isAnalyzing() const;

signals:
    void analyzeFinished();
    void analyzeFailed(QString error);

private:
    void resetState();
    void processBuffer(const QAudioBuffer &buffer);
    void finalizeAnalysis();
    float normalizeSample(qint16 value) const;
    float normalizeSample(qint32 value) const;
    float normalizeSample(float value) const;
    float normalizeSample(quint8 value) const;

    QAudioDecoder *m_decoder;
    QVector<float> m_bins;
    qint64 m_binMs;
    QString m_currentFilePath;
    bool m_ready;
    bool m_analyzing;
    int m_channelCount;
    int m_sampleRate;
    qint64 m_targetSamplesPerBin;
    float m_energySum;
    qint64 m_energyCount;
};
