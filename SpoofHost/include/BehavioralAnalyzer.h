#pragma once
#include <windows.h>
#include "HostTypes.h"

// === Структуры для анализа поведения ===
#pragma pack(push, 1)
typedef struct _TIMING_SAMPLE {
    ULONG    ProcessId;
    FILETIME StartTime;
    FILETIME CurrentTime;
    ULONG    MemoryAccesses;
    ULONG    SystemCalls;
} TIMING_SAMPLE, * PTIMING_SAMPLE;

typedef struct _MEMORY_PATTERN {
    PVOID   BaseAddress;
    SIZE_T  Size;
    BYTE    Signature[16];
    BOOLEAN IsModified;
} MEMORY_PATTERN, * PMEMORY_PATTERN;
#pragma pack(pop)

// === Функции анализа (объявления) ===
VOID    InitBehavioralAnalyzer();
BOOLEAN AnalyzeProcess(HANDLE hProcess);
BOOLEAN DetectEACInjection(HANDLE hProcess);
BOOLEAN MonitorTimingAnomalies(PTIMING_SAMPLE pSample);
BOOLEAN CheckMemoryAccessPatterns(PMEMORY_PATTERN pPattern);

// === Глобальные переменные (ВНЕ структур!) ===
extern HANDLE          g_hBehaviorAnalyzer;
extern CRITICAL_SECTION g_AnalyzerLock;