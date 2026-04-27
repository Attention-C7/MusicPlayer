#pragma once

#include <QtGlobal>
#include <QString>

struct SongInfo
{
    QString filePath; // Full file path
    QString title;    // Song title (ID3)
    QString artist;   // Artist (ID3)
    QString album;    // Album (ID3)
    qint64 duration = 0; // Duration in milliseconds
};

enum class PlayMode
{
    SingleLoop, // Single song loop
    FolderLoop, // Loop current folder
    AllLoop,    // Loop all songs
    RandomPlay  // Random play
};
