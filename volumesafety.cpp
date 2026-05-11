#include "volumesafety.h"

#include <QMessageBox>
#include <QWidget>

bool VolumeSafety::confirmHighVolumeIfNeeded(
    int requestedPercent,
    int currentAppliedPercent,
    QWidget *parent)
{
    if (requestedPercent < kWarningThresholdPercent) {
        return true;
    }
    if (currentAppliedPercent >= kWarningThresholdPercent) {
        return true;
    }

    const QMessageBox::StandardButton btn = QMessageBox::warning(
        parent,
        QStringLiteral("音量提示"),
        QStringLiteral("音量过大可能损伤听力"),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);

    return btn == QMessageBox::Ok;
}
