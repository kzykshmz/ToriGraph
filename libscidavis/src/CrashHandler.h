#ifndef TORIGRAPH_CRASH_HANDLER_H
#define TORIGRAPH_CRASH_HANDLER_H

#include <QString>

namespace ToriGraphCrash {

// Installs a lightweight process-wide crash dump writer on Windows (MiniDumpWriteDump).
// Automatically skips installation when AddressSanitizer is active or when
// TORIGRAPH_DISABLE_CRASH_DUMP=1 is set in the environment.
// Safe to call multiple times.
void installCrashHandler();

// Returns true if the unhandled exception minidump filter is currently installed.
bool isCrashHandlerInstalled();

// Returns the directory where crash minidumps (.dmp) are written, or an empty string if disabled.
QString crashDumpDirectory();

} // namespace ToriGraphCrash

#endif // TORIGRAPH_CRASH_HANDLER_H
