#include "CrashHandler.h"
#include "globals.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStandardPaths>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <cwchar>
#endif

namespace ToriGraphCrash {

namespace {
bool g_installed = false;
QString g_dumpDirString;

#ifdef _WIN32
wchar_t g_dumpDirWide[MAX_PATH] = { 0 };
wchar_t g_versionWide[64] = L"unknown";
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

using MiniDumpWriteDumpFn = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                           PMINIDUMP_EXCEPTION_INFORMATION,
                                           PMINIDUMP_USER_STREAM_INFORMATION,
                                           PMINIDUMP_CALLBACK_INFORMATION);

static bool isAddressSanitizerLoaded()
{
#if defined(__SANITIZE_ADDRESS__)
    return true;
#else
    return GetModuleHandleW(L"clang_rt.asan_dynamic-x86_64.dll") != nullptr
            || GetModuleHandleW(L"clang_rt.asan_dbg_dynamic-x86_64.dll") != nullptr;
#endif
}

// Allocation-free crash callback so corrupted heaps never deadlock inside the handler.
static LONG WINAPI unhandledSehFilter(EXCEPTION_POINTERS *exceptionPointers)
{
    if (g_dumpDirWide[0] != L'\0') {
        HMODULE dbgHelp = LoadLibraryW(L"dbghelp.dll");
        if (dbgHelp) {
            auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(
                    GetProcAddress(dbgHelp, "MiniDumpWriteDump"));
            if (writeDump) {
                SYSTEMTIME st {};
                GetSystemTime(&st);
                const DWORD pid = GetCurrentProcessId();

                wchar_t dumpPath[MAX_PATH * 2] = { 0 };
                std::swprintf(dumpPath, sizeof(dumpPath) / sizeof(wchar_t),
                              L"%ls\\ToriGraph-%ls-%04u%02u%02uT%02u%02u%02uZ-pid%lu.dmp",
                              g_dumpDirWide, g_versionWide, unsigned(st.wYear), unsigned(st.wMonth),
                              unsigned(st.wDay), unsigned(st.wHour), unsigned(st.wMinute),
                              unsigned(st.wSecond), static_cast<unsigned long>(pid));

                HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    MINIDUMP_EXCEPTION_INFORMATION exInfo {};
                    exInfo.ThreadId = GetCurrentThreadId();
                    exInfo.ExceptionPointers = exceptionPointers;
                    exInfo.ClientPointers = FALSE;

                    const auto dumpType = static_cast<MINIDUMP_TYPE>(
                            MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo);
                    writeDump(GetCurrentProcess(), pid, hFile, dumpType,
                              exceptionPointers ? &exInfo : nullptr, nullptr, nullptr);
                    CloseHandle(hFile);
                }
            }
        }
    }

    if (g_previousFilter)
        return g_previousFilter(exceptionPointers);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void pruneOldDumps(const QDir &dir, int keepCount)
{
    QFileInfoList dumps = dir.entryInfoList(QStringList() << QStringLiteral("ToriGraph-*.dmp"),
                                            QDir::Files, QDir::Time);
    for (int i = keepCount; i < dumps.size(); ++i) {
        QFile::remove(dumps[i].absoluteFilePath());
    }
}
#endif // _WIN32

} // namespace

void installCrashHandler()
{
    if (g_installed)
        return;

    if (qEnvironmentVariableIntValue("TORIGRAPH_DISABLE_CRASH_DUMP") != 0)
        return;

#ifdef _WIN32
    if (isAddressSanitizerLoaded())
        return;

    QString dirPath = qEnvironmentVariable("TORIGRAPH_CRASH_DUMP_DIR").trimmed();
    if (dirPath.isEmpty()) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (base.isEmpty())
            return;
        dirPath = QDir(base).filePath(QStringLiteral("CrashDumps"));
    }

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return;

    const QString nativeDir = QDir::toNativeSeparators(dir.absolutePath());
    const std::wstring wDir = nativeDir.toStdWString();
    if (wDir.empty() || wDir.size() >= MAX_PATH - 64)
        return;

    pruneOldDumps(dir, 10);

    wcsncpy_s(g_dumpDirWide, wDir.c_str(), _TRUNCATE);
    const std::wstring wVer = SciDAVis::versionString().trimmed().toStdWString();
    if (!wVer.empty())
        wcsncpy_s(g_versionWide, wVer.c_str(), _TRUNCATE);

    g_dumpDirString = dir.absolutePath();
    g_previousFilter = SetUnhandledExceptionFilter(&unhandledSehFilter);
    g_installed = true;
#endif
}

bool isCrashHandlerInstalled()
{
    return g_installed;
}

QString crashDumpDirectory()
{
    return g_dumpDirString;
}

} // namespace ToriGraphCrash
