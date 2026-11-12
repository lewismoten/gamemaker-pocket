// When settings.txt may be rewritten. An existing file that could not be opened, or was read only in part, is kept as it is
// (the settings in memory are defaults until the next boot reads it whole); a file that is simply not there may be created.
#pragma once
namespace pk {
inline bool keepSettingsFile(bool existed, bool opened, long readTo, long size) { return existed && (!opened || readTo != size); }
}
