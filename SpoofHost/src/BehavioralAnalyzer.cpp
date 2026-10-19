#include <windows.h>
#include <stdio.h>
#include "BehavioralAnalyzer.h"

// === Глобальные переменные ===
HANDLE g_hBehaviorAnalyzer = NULL;
CRITICAL_SECTION g_AnalyzerLock;

// Инициализация анализатора поведения
VOID InitBehavioralAnalyzer() {
    InitializeCriticalSection(&g_AnalyzerLock);
}

// Анализ процесса на предмет аномалий
BOOLEAN AnalyzeProcess(HANDLE hProcess) {
    if (!hProcess || !IsProcessValid(hProcess)) return FALSE;

    TIMING_SAMPLE sample = { 0 };
    sample.ProcessId = GetCurrentProcessId();

    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    sample.CurrentTime = now;

    // Получаем базовую информацию о процессе
    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG resultSize = sizeof(pbi);

    if (NtQueryInformationProcess(hProcess, ProcessBasicInformation,
        &pbi, sizeof(pbi), &resultSize) != STATUS_SUCCESS) {
        fprintf(stderr, "[Analyzer] Failed to query process info\n");
        return FALSE;
    }

    // Мониторинг времени выполнения
    if (MonitorTimingAnomalies(&sample)) {
        fprintf(stderr, "[Analyzer] Timing anomaly detected in PID %lu\n",
            sample.ProcessId);
    }

    // Проверка на инъекции EAC
    if (DetectEACInjection(hProcess)) {
        fprintf(stderr, "[Analyzer] Possible EAC injection detected in PID %lu\n",
            sample.ProcessId);
    }

    return TRUE;
}

// Детекция инъекций Easy Anti-Cheat
BOOLEAN DetectEACInjection(HANDLE hProcess) {
    if (!hProcess) return FALSE;

    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODUL
