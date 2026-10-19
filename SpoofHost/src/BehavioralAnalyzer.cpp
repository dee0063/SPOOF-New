#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <ntstatus.h>  // <-- ДОБАВЛЕНО
#include "BehavioralAnalyzer.h"
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

// === Глобальные переменные ===
HANDLE g_hBehaviorAnalyzer = NULL;
CRITICAL_SECTION g_AnalyzerLock;

// === Вспомогательная функция проверки процесса ===
BOOLEAN IsProcessValid(HANDLE hProcess) {
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }

    return FALSE;
}

// === Инициализация анализатора поведения ===
VOID InitBehavioralAnalyzer() {
    InitializeCriticalSection(&g_AnalyzerLock);
    g_hBehaviorAnalyzer = CreateEventW(NULL, FALSE, FALSE, L"SpoofAnalyzerEvent");  // <-- ИСПРАВЛЕНО

    if (g_hBehaviorAnalyzer == NULL) {
        fprintf(stderr, "[Analyzer] Failed to create event: %lu\n", GetLastError());
    }
    else {
        fprintf(stdout, "[Analyzer] Behavioral analyzer initialized\n");
    }
}

// === Анализ процесса на предмет аномалий ===
BOOLEAN AnalyzeProcess(HANDLE hProcess) {
    if (!hProcess || !IsProcessValid(hProcess)) {
        return FALSE;
    }

    EnterCriticalSection(&g_AnalyzerLock);

    TIMING_SAMPLE sample = { 0 };
    sample.ProcessId = GetProcessId(hProcess);

    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    sample.CurrentTime = now;

    // Мониторинг времени выполнения
    if (MonitorTimingAnomalies(&sample)) {
        fprintf(stderr, "[Analyzer] Timing anomaly detected in PID %lu\n", sample.ProcessId);
    }

    // Проверка на инъекции EAC
    if (DetectEACInjection(hProcess)) {
        fprintf(stderr, "[Analyzer] Possible EAC injection detected in PID %lu\n", sample.ProcessId);
    }

    LeaveCriticalSection(&g_AnalyzerLock);
    return TRUE;
}

// === Детекция инъекций Easy Anti-Cheat ===
BOOLEAN DetectEACInjection(HANDLE hProcess) {
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    EnterCriticalSection(&g_AnalyzerLock);

    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetProcessId(hProcess));

    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&g_AnalyzerLock);
        return FALSE;
    }

    MODULEENTRY32W me32 = { 0 };
    me32.dwSize = sizeof(MODULEENTRY32W);

    BOOLEAN eacDetected = FALSE;

    if (Module32FirstW(hModuleSnap, &me32)) {
        do {
            if (wcsstr(me32.szModule, L"EasyAntiCheat") != NULL ||
                wcsstr(me32.szModule, L"EAC") != NULL) {
                fprintf(stdout, "[Analyzer] EAC module detected: %ls\n", me32.szModule);  // <-- ИСПРАВЛЕНО: %ls
                eacDetected = TRUE;
            }
        } while (Module32NextW(hModuleSnap, &me32));
    }

    CloseHandle(hModuleSnap);
    LeaveCriticalSection(&g_AnalyzerLock);

    return eacDetected;
}

// === Мониторинг аномалий времени ===
BOOLEAN MonitorTimingAnomalies(PTIMING_SAMPLE pSample) {
    if (!pSample) {
        return FALSE;
    }

    ULONGLONG startTime = ((ULONGLONG)pSample->StartTime.dwHighDateTime << 32) |
        pSample->StartTime.dwLowDateTime;
    ULONGLONG currentTime = ((ULONGLONG)pSample->CurrentTime.dwHighDateTime << 32) |
        pSample->CurrentTime.dwLowDateTime;

    ULONGLONG elapsed = currentTime - startTime;

    if (elapsed < 10000) {
        return TRUE;
    }

    return FALSE;
}

// === Проверка паттернов доступа к памяти ===
BOOLEAN CheckMemoryAccessPatterns(PMEMORY_PATTERN pPattern) {
    if (!pPattern) {
        return FALSE;
    }

    fprintf(stdout, "[Analyzer] Memory pattern check at address: %p\n", pPattern->BaseAddress);

    return pPattern->IsModified;
}

// === Очистка анализатора ===
VOID CleanupBehavioralAnalyzer() {
    if (g_hBehaviorAnalyzer != NULL) {
        CloseHandle(g_hBehaviorAnalyzer);
        g_hBehaviorAnalyzer = NULL;
    }
    DeleteCriticalSection(&g_AnalyzerLock);
    fprintf(stdout, "[Analyzer] Behavioral analyzer cleaned up\n");
}