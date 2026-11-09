// Bug reports (0.28.0 Options "Make a bug report"; 0.43.3 also made by themselves after a crash).
#pragma once
#include <string>

namespace bugreport {
// Copies ti4.log, ti4.prev.log, the save, the settings and launch_log.txt into "bug-report-<date>-<time><suffix>"
// next to the game; with gameLog also the saved game's own log (it replays the save). Returns the folder's name,
// or "" when there was nothing to copy.
std::string make(const char* suffix, bool gameLog);
// At start-up: if the last run crashed (its log, now ti4.prev.log, ends with a CRASH line), make a report and
// remember its folder for the title screen. Returns true if it did.
bool afterCrash();
// The folder made by afterCrash ("" if none) - the title screen tells the player about it.
const std::string& crashFolder();
}  // namespace bugreport
