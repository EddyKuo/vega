#define NOMINMAX
#include "core/CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <ctime>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace vega {

static LONG WINAPI vegaCrashFilter(EXCEPTION_POINTERS* ex_info)
{
    // ── Build dump filename with timestamp ──
    char temp_dir[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, temp_dir);

    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now);

    char dump_path[MAX_PATH];
    snprintf(dump_path, MAX_PATH,
             "%svega_crash_%04d%02d%02d_%02d%02d%02d.dmp",
             temp_dir,
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    // ── Create the dump file ──
    HANDLE file = CreateFileA(dump_path, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    bool dump_ok = false;
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId          = GetCurrentThreadId();
        mdei.ExceptionPointers = ex_info;
        mdei.ClientPointers    = FALSE;

        // MiniDumpWithDataSegs includes global variable data which helps
        // diagnose the crash; MiniDumpWithIndirectlyReferencedMemory grabs
        // memory referenced by stack locals.
        MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);

        dump_ok = MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(),
            file, dump_type, &mdei, nullptr, nullptr) != FALSE;

        CloseHandle(file);
    }

    // ── Show a message box so the user knows what happened ──
    char msg[1024];
    if (dump_ok)
    {
        snprintf(msg, sizeof(msg),
                 "Vega has encountered an unexpected error and needs to close.\n\n"
                 "A crash dump has been saved to:\n%s\n\n"
                 "Exception code: 0x%08lX\n"
                 "Exception address: 0x%p\n\n"
                 "Please include the dump file when reporting this issue.",
                 dump_path,
                 ex_info->ExceptionRecord->ExceptionCode,
                 ex_info->ExceptionRecord->ExceptionAddress);
    }
    else
    {
        snprintf(msg, sizeof(msg),
                 "Vega has encountered an unexpected error and needs to close.\n\n"
                 "Failed to write crash dump.\n\n"
                 "Exception code: 0x%08lX\n"
                 "Exception address: 0x%p",
                 ex_info->ExceptionRecord->ExceptionCode,
                 ex_info->ExceptionRecord->ExceptionAddress);
    }

    MessageBoxA(nullptr, msg, "Vega — Fatal Error",
                MB_OK | MB_ICONERROR | MB_TASKMODAL);

    return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashHandler()
{
    SetUnhandledExceptionFilter(vegaCrashFilter);
}

} // namespace vega
