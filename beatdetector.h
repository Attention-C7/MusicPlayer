#pragma once

#include <QObject>
#include <QVector>

#include <deque>

#include <QtGlobal>

#include <aubio/aubio.h>

class QAudioBuffer;

/**
 * 节拍检测：按缓冲实际采样率懒初始化 aubio。
 * - tempo：aubio 二值 + 置信度门限；高 BPM 半速估计
 * - onset：ODF 连续值，按近期峰值归一化后入窗，滑动中位数×1.2 动态阈值
 * - 融合：beatCandidate = tempoHit || onsetHit；最小间隔在样本域仅在锁相且强拍≥8 后随周期动态收紧
 * - 样本域时间轴 + PLL：连续 4 个真实拍后锁相，漏拍时最多 2 次低强度预测插补
 * - 灵敏度预设：Normal / BoostWeak / ReduceFalse，调节 tempo 门限、onset 基础值与动态分位数权重
 * - bpmUpdated：周期估计变化时发当前 BPM，供 UI 呼吸兜底
 */
class BeatDetector : public QObject
{
    Q_OBJECT

public:
    /** 节拍检测灵敏度预设（与 UI 下拉框顺序一致：标准 / 弱节奏增强 / 减误触）。 */
    enum Sensitivity {
        Normal = 0,
        BoostWeak = 1,
        ReduceFalse = 2
    };
    Q_ENUM(Sensitivity)

    explicit BeatDetector(QObject *parent = nullptr);
    ~BeatDetector() override;

    void feedBuffer(const QAudioBuffer &buffer);

    /** 切换预设：调整 tempo 置信门限、onset 基础阈值与软下限；会重置 onset 滑动窗口。 */
    void setSensitivity(Sensitivity s);
    Sensitivity sensitivity() const { return m_sensitivity; }

    /**
     * onset 灵敏度，建议 0.22～0.35；过小易噪，过大民谣偏弱。
     * 同时作为动态阈值下限（与滑动分位数取 max），并同步到 aubio peakpicker。
     * 调用后保留用户数值；若需恢复预设整体参数请再调 setSensitivity。
     */
    void setOnsetThreshold(float value);
    /** aubio 静音门限（dB），如 -70；整体音量偏低时可略调高（如 -75）。 */
    void setOnsetSilenceDb(float value);

signals:
    /** intensity ∈ [0,1]，强拍接近 1，弱拍/插补较低；UI 用于峰值与时长。 */
    void beatDetected(float intensity);
    /** 估计 BPM（由 m_beatPeriodSamples 与采样率推算），在周期更新后发出。 */
    void bpmUpdated(float bpm);

private:
    void releaseAnalysisEngine();
    bool ensureAnalysisEngine(int sampleRate);

    /**
     * 真实拍强度：onset 相对 m_onsetPeakEstimate 归一化 + tempo 置信度加权。
     * 预测拍不调用本函数，请用 calculatePredictedIntensity()。
     */
    float calculateBeatIntensity(bool onsetHit, float onsetValue, float tempoConf, bool isPredicted) const;
    /** 预测插补拍固定低强度（与 calculateBeatIntensity 分离）。 */
    float calculatePredictedIntensity() const;

    void resetPllState();
    /** 根据当前 m_beatPeriodSamples 发射 bpmUpdated（需 m_sampleRate > 0）。 */
    void notifyBpmUpdated();

    aubio_tempo_t *m_tempo = nullptr;
    aubio_onset_t *m_onset = nullptr;
    fvec_t *m_inputBuf = nullptr;
    fvec_t *m_outputBuf = nullptr;
    /** onset 专用输出缓冲，避免与 tempo 共用 m_outputBuf。 */
    fvec_t *m_onsetOut = nullptr;
    QVector<float> m_pending;

    /** 已处理样本累计（每 hop 末尾对齐）。 */
    qint64 m_totalSamples = 0;
    qint64 m_lastBeatSample = -1;
    qint64 m_nextBeatSample = -1;
    /** 估计拍周期（样本）；默认 120 BPM @44.1k，引擎就绪后按采样率重写。 */
    float m_beatPeriodSamples = 22050.0f;
    bool m_phaseLocked = false;
    int m_strongBeatCount = 0;
    int m_predictBeatCount = 0;

    uint_t m_sampleRate = 0;
    float m_onsetThreshold = 0.3f;
    float m_onsetSilenceDb = -70.0f;

    /** 动态 onset 检测阈值；引擎重置时与 m_onsetThreshold 对齐。 */
    float m_dynamicOnsetThreshold = 0.3f;
    /** 最近若干 hop 的归一化 onset（0～1 量级），用于中位数动态阈值。 */
    std::deque<float> m_onsetHistory;
    static constexpr int kOnsetHistoryMax = 86;
    /** 归一化分母：近期 ODF 峰值（带衰减），与 onset 原始值同量纲；重置为 0.1f。 */
    float m_onsetMaxRecent = 0.1f;
    /** 归一化参考峰值（缓慢跟踪分位水平）。 */
    float m_onsetPeakEstimate = 0.3f;

    quint64 m_statRawTempo = 0;
    quint64 m_statRawOnset = 0;
    quint64 m_statEmit = 0;
    quint64 m_statEmitPredicted = 0;
    quint64 m_statSuppressed = 0;
    /** 窗口内动态 onset 阈值采样累加（用于平均）。 */
    double m_statDynThresholdSum = 0.0;
    quint64 m_statDynThresholdCount = 0;
    qint64 m_lastStatsLogMs = 0;

    static constexpr uint_t HOP_SIZE = 256;
    static constexpr uint_t WIN_SIZE = 2048;

    void resetOnsetDynamics();

    Sensitivity m_sensitivity = Normal;
    /** tempo 二值后置信度下限，随 Sensitivity 变化。 */
    float m_tempoConfTrigger = 0.1f;
    /** 灵敏度预设同步（动态阈值为滑动中位数×1.2，本系数暂不参与计算）。 */
    float m_dynPercentileScale = 0.5f;
    /** 动态阈值中与 m_onsetThreshold 并列的软下限。 */
    float m_onsetFloorSoft = 0.08f;
};
