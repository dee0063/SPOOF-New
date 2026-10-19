#pragma once

#include <windows.h>
#include "HostTypes.h"

// === Константы для сигнатур DirectX ===
#define DX_SIGNATURE_1 "\x48\x89\xe5\x41\xff\xc2" // mov rbp,rsp; call qword ptr [r10] (Direct3D11)
#define DX_SIGNATURE_2 "\x4c\x8b\x4d\xf8" // mov rcx,qword ptr [rbp-0x10] (Direct3D9)

// === Дополнительные константы ===
#define DX_MODULE_NAME L"d3d11.dll"
#define DX_DRIVER_NAME L"D3D11Driver.sys"
#define DX_MAX_MODULES 64
#define DX_SCAN_TIMEOUT_MS 5000
#define DX_MAX_INTERFACES 32

// === Структуры для анализа памяти DirectX ===
#pragma pack(push, 1)
typedef struct _DIRECT3D11_INTERFACE_INFO {
    HANDLE hModule;
    PVOID BaseAddress;
    SIZE_T Size;
    BOOLEAN IsHooked;
} DIRECT3D11_INTERFACE_INFO, * PDIRECT3D11_INTERFACE_INFO;

typedef struct _MEMORY_SCAN_RESULT {
    BOOLEAN FoundSignature;
    PVOID SignatureLocation;
    ULONG ScanSize;
    BOOLEAN IsDirectXModule;
} MEMORY_SCAN_RESULT, * PMEMORY_SCAN_RESULT;

typedef struct _DIRECTX_HOOK_STATE {
    BOOLEAN IsInitialized;
    BOOLEAN DX11Detected;
    BOOLEAN DX9Detected;
    BOOLEAN HookedFunctionsCount;
    ULONG InterfaceCount;
    PDIRECT3D11_INTERFACE_INFO FoundInterfaces[DX_MAX_INTERFACES];
} DIRECTX_HOOK_STATE, * PDIRECTX_HOOK_STATE;

typedef struct _TIMING_SAMPLE {
    LARGE_INTEGER StartTick;
    LARGE_INTEGER EndTick;
    ULONGLONG TotalTime;
    BOOLEAN IsAnomaly;
} TIMING_SAMPLE, * PTIMING_SAMPLE;
#pragma pack(pop)

// === Функции (объявления) ===
VOID InitializeDirectXHook();
VOID CleanupDirectXHook();
BOOLEAN DetectDirectXInMemory(HANDLE hProcess);
VOID ScanForDXSignatures(PDIRECT3D11_INTERFACE pResult);
BOOLEAN HookDirectX11Functions(HANDLE hProcess);
BOOLEAN HookDirectX9Functions(HANDLE hProcess);
VOID MonitorGPUActivity(PTIMING_SAMPLE pSample);

// === Геттеры для глобальных переменных ===
PDIRECT3D11_INTERFACE GetFoundInterfaces();
ULONG GetInterfaceCount();
BOOLEAN IsDXDetected();
BOOLEAN IsDXHooked();

// === Управление состоянием ===
VOID ResetHookState();
