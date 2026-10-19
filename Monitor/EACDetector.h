#pragma once

#include <windows.h>
#include "HostTypes.h"

// === Константы для сигнатур EAC ===
#define EAC_SIGNATURE_1 "\x48\x89\xe5\x41\xff\xc2" // mov rbp,rsp; call qword ptr [r10]
#define EAC_SIGNATURE_2 "\x4c\x8b\x4d\xf8" // mov rcx,qword ptr [rbp-0x10]

// === Дополнительные сигнатуры для надёжности ===
#define EAC_MODULE_NAME L"EACClient.dll"
#define EAC_DRIVER_NAME L"EACDriver.sys"
#define EAC_MAX_MODULES 64
#define EAC_SCAN_TIMEOUT_MS 5000

// === Структуры для анализа памяти ===
#pragma pack(push, 1)
typedef struct _EAC_MODULE_INFO {
    HANDLE hModule;
    PVOID BaseAddress;
    SIZE_T Size;
    BOOLEAN IsInjected;
} EAC_MODULE_INFO, * PEAC_MODULE_INFO;

typedef struct _MEMORY_SCAN_RESULT {
    BOOLEAN FoundSignature;
    PVOID SignatureLocation;
    ULONG ScanSize;
    BOOLEAN IsEACModule;
} MEMORY_SCAN_RESULT, * PMEMORY_SCAN_RESULT;

typedef struct _TIMING_SAMPLE {
    LARGE_INTEGER StartTick;
    LARGE_INTEGER EndTick;
    ULONGLONG TotalTime;
    BOOLEAN IsAnomaly;
} TIMING_SAMPLE, * PTIMING_SAMPLE;

typedef struct _EAC_DETECTION_STATE {
    BOOLEAN Detected;
    BOOLEAN Bypassed;
    ULONG DetectionCount;
    LARGE_INTEGER LastDetectionTime;
    PEAC_MODULE_INFO FoundModules[EAC_MAX_MODULES];
    ULONG ModuleCount;
} EAC_DETECTION_STATE, * PEAC_DETECTION_STATE;
#pragma pack(pop)

// === Функции (объявления) ===
BOOLEAN DetectEACInMemory(HANDLE hProcess);
VOID ScanForESignatures(PMEMORY_SCAN_RESULT pResult);
BOOLEAN BypassEACChecks(HANDLE hProcess);
VOID MonitorEACBehavior(PTIMING_SAMPLE pSample);
PEAC_MODULE_INFO GetFoundModules();
ULONG GetModuleCount();
BOOLEAN IsEACDetected();
BOOLEAN IsEACBypassed();
VOID ResetDetectionState();

// === Глобальные переменные ===
extern HANDLE g_hEACDetector;
extern CRITICAL_SECTION g_EACLSection;
extern EAC_DETECTION_STATE g_EACState;
extern PEAC_MODULE_INFO g_pFoundModules;
extern ULONG g_ModuleCount;
extern BOOLEAN g_IsInitialized;

// === Инициализация и очистка ===
VOID InitializeEACDetector();
VOID CleanupEACDetector();
