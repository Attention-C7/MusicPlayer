#pragma once

class QWidget;

namespace VolumeSafety {
// 达到或超过该阈值需二次确认（触控松手 / 语音调音量）
constexpr int kWarningThresholdPercent = 60;

// requestedPercent：目标音量；currentAppliedPercent：当前已写入输出的音量。
// 若从低于阈值拉到不低于阈值，弹窗；确定返回 true，取消返回 false（保持 current）。
bool confirmHighVolumeIfNeeded(int requestedPercent, int currentAppliedPercent, QWidget *parent);
}
