#include "DirectXHook.h"
#include <vector>
#include <string>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <d3d11.h>
#include <d3d9.h>
#include <d3d10_1.h>
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

// === Глобальные переменные ===
HANDLE g_hDirectXHook = NULL;
CRITICAL_SECTION g_DXSection;
DIRECTX_HOOK_STATE g_DXState = { 0 };
PDIRECT3D11_INTERFACE g_pDX11Interface = nullptr;
PDIRECT3D9_INTERFACE g_pDX9Interface = nullptr;

// === Внутренние структуры для перехвата ===
typedef struct _MODULE_LIST_ENTRY {
    HANDLE hModule;
    PVOID BaseAddress;
    SIZE_T Size;
} MODULE_LIST_ENTRY, * PMODULE_LIST_ENTRY;

typedef struct _DIRECT3D11_INTERFACE_INFO {
    GUID InterfaceName;
    REFGUID ParentInterface;
} DIRECT3D11_INTERFACE_INFO, * PDIRECT3D11_INTERFACE_INFO;

// === Внутренние функции NT ===
static NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) = nullptr;
static NTSTATUS(NTAPI* pNtQueryVirtualMemory)(HANDLE ProcessHandle, HANDLE ProcessId, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID* Buffer, PSIZE_T BufferSize, SIZE_T* ReturnLength) = nullptr;
static NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE hProcess, PVOID lpBaseAddress, PVOID lpBuffer, SIZE_T nSize, PSIZE_T lpNumberOfBytesRead) = nullptr;
static NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE hProcess, PVOID lpBaseAddress, PVOID lpBuffer, SIZE_T nSize, PSIZE_T lpNumberOfBytesWritten) = nullptr;

// === Инициализация DirectX Hook ===
VOID InitializeDirectXHook() {
    if (g_DXState.IsInitialized) return;

    EnterCriticalSection(&g_DXSection);

    if (!g_DXState.IsInitialized) {
        // Инициализируем критическую секцию
        InitializeCriticalSection(&g_DXSection);

        // Получаем адреса внутренних функций NT
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            pNtQueryInformationProcess = (NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG))GetProcAddress(hNtdll, "NtQueryInformationProcess");
            pNtQueryVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, HANDLE, MEMORY_INFORMATION_CLASS, PVOID*, PSIZE_T, SIZE_T*))GetProcAddress(hNtdll, "NtQueryVirtualMemory");
            pNtReadVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T))GetProcAddress(hNtdll, "NtReadVirtualMemory");
            pNtWriteVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T))GetProcAddress(hNtdll, "NtWriteVirtualMemory");
        }

        // Инициализируем состояние хука
        g_DXState.IsInitialized = TRUE;
        g_DXState.DX11Detected = FALSE;
        g_DXState.DX9Detected = FALSE;
        g_DXState.HookedFunctionsCount = 0;

        // Выделяем память для интерфейсов
        SIZE_T interfaceSize = sizeof(DIRECT3D11_INTERFACE_INFO) * DX_MAX_INTERFACES;
        g_pDX11Interface = (PDIRECT3D11_INTERFACE)VirtualAlloc(nullptr, interfaceSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    LeaveCriticalSection(&g_DXSection);
}

// === Очистка DirectX Hook ===
VOID CleanupDirectXHook() {
    EnterCriticalSection(&g_DXSection);

    if (g_pDX11Interface) {
        VirtualFree(g_pDX11Interface, 0, MEM_RELEASE);
        g_pDX11Interface = nullptr;
    }

    if (!IsEmpty(&g_DXSection)) {
        DeleteCriticalSection(&g_DXSection);
    }

    g_DXState.IsInitialized = FALSE;
}

// === Проверка на пустую критическую секцию ===
static BOOLEAN IsEmpty(CRITICAL_SECTION* cs) {
    return (cs->LockCount == 0 && cs->CreatorBackTraceIndex == 0);
}

// === Сканирование модулей процесса ===
static PMODULE_LIST_ENTRY ScanProcessModules(HANDLE hProcess, ULONG& ModuleCount) {
    MODULE_LIST_ENTRY* modules = nullptr;

    if (pNtQueryInformationProcess && pNtQueryVirtualMemory) {
        PROCESS_MEMORY_INFORMATION pmInfo;
        SIZE_T infoSize = sizeof(PROCESS_MEMORY_INFORMATION);

        NTSTATUS status = pNtQueryInformationProcess(hProcess, ProcessBasicInfo, &pmInfo, infoSize, nullptr);

        if (status == STATUS_SUCCESS && pmInfo.BaseAddress) {
            HMODULE hMods = GetModuleHandleA(nullptr);

            MODULEENTRY32W moduleEntry;
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);

            if (hSnap != INVALID_HANDLE_VALUE) {
                modules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, sizeof(MODULE_LIST_ENTRY), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                MODULEENTRY32W* pModule = &moduleEntry;
                pModule->dwSize = sizeof(MODULEENTRY32W);

                if (Module32FirstW(hSnap, pModule)) {
                    do {
                        SIZE_T size = sizeof(MODULE_LIST_ENTRY) * 1024;
                        MODULE_LIST_ENTRY* tempModules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                        if (tempModules && modules) {
                            SIZE_T newSize = sizeof(MODULE_LIST_ENTRY) * 2048;
                            tempModules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, newSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                            if (tempModules) {
                                modules = tempModules;
                            }
                        }

                        // Добавляем модуль в список
                        if (modules && ModuleCount < DX_MAX_MODULES) {
                            PMODULE_LIST_ENTRY pMod = &modules[ModuleCount];
                            pMod->hModule = hProcess;
                            pMod->BaseAddress = (PVOID)pModule->modBaseAddr;
                            pMod->Size = pModule->modBaseSize;

                            ModuleCount++;
                        }

                    } while (Module32NextW(hSnap, pModule) && ModuleCount < DX_MAX_MODULES);
                }

                CloseHandle(hSnap);
            }
        }
    }

    return modules;
}

// === Поиск модулей DirectX в процессе ===
static BOOLEAN FindDirectXModules(HANDLE hProcess, PDIRECT3D11_INTERFACE& FoundInterfaces, ULONG& InterfaceCount) {
    MODULE_LIST_ENTRY* modules = ScanProcessModules(hProcess, InterfaceCount);

    if (!modules || InterfaceCount == 0) return FALSE;

    // Проверяем каждый модуль на наличие DirectX
    for (ULONG i = 0; i < InterfaceCount; i++) {
        PMODULE_LIST_ENTRY pMod = &modules[i];

        // Проверяем имя модуля
        if (pNtQueryInformationProcess && pNtReadVirtualMemory) {
            char moduleName[MAX_PATH] = { 0 };

            NTSTATUS status = pNtQueryInformationProcess(hProcess, ProcessModuleInfo, &pMod->hModule, sizeof(pMod->hModule), nullptr);

            if (status == STATUS_SUCCESS && strlen(moduleName) > 0) {
                // Проверяем на наличие DirectX в имени
                if (strstr(moduleName, "d3d11") || strstr(moduleName, "D3D11") ||
                    strstr(moduleName, "d3d9") || strstr(moduleName, "D3D9") ||
                    strstr(moduleName, "d3d10") || strstr(moduleName, "D3D10")) {

                    PDIRECT3D11_INTERFACE pDXInterface = &FoundInterfaces[i];
                    pDXInterface->hModule = pMod->hModule;
                    pDXInterface->BaseAddress = pMod->BaseAddress;
                    pDXInterface->Size = pMod->Size;
                    pDXInterface->IsHooked = FALSE;

                    InterfaceCount++;
                }
            }
        }
    }

    return (InterfaceCount > 0);
}

// === Поиск сигнатур Direct3D11 в памяти ===
static BOOLEAN FindDirectX11Signature(HANDLE hProcess, PVOID BaseAddress, SIZE_T Size, const char* Signature, PVOID& FoundLocation) {
    SIZE_T sigLen = strlen(Signature);

    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, Size + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) return FALSE;

    BOOLEAN found = FALSE;
    SIZE_T bytesRead = 0;

    for (SIZE_T offset = 0; offset < Size - sigLen; offset += 256) {
        if (pNtReadVirtualMemory(hProcess, (PVOID)(BaseAddress + offset), pBuffer, 256, &bytesRead)) {
            for (SIZE_T i = 0; i < sigLen && i < bytesRead; i++) {
                if (pBuffer[i] == Signature[i]) {
                    FoundLocation = (PVOID)((BYTE*)BaseAddress + offset);
                    found = TRUE;
                    break;
                }
            }

            if (found) break;
        }
    }

    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return found;
}

// === Поиск сигнатур Direct3D9 в памяти ===
static BOOLEAN FindDirectX9Signature(HANDLE hProcess, PVOID BaseAddress, SIZE_T Size, const char* Signature, PVOID& FoundLocation) {
    SIZE_T sigLen = strlen(Signature);

    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, Size + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) return FALSE;

    BOOLEAN found = FALSE;
    SIZE_T bytesRead = 0;

    for (SIZE_T offset = 0; offset < Size - sigLen; offset += 256) {
        if (pNtReadVirtualMemory(hProcess, (PVOID)(BaseAddress + offset), pBuffer, 256, &bytesRead)) {
            for (SIZE_T i = 0; i < sigLen && i < bytesRead; i++) {
                if (pBuffer[i] == Signature[i]) {
                    FoundLocation = (PVOID)((BYTE*)BaseAddress + offset);
                    found = TRUE;
                    break;
                }
            }

            if (found) break;
        }
    }

    VirtualFree(pBuffer, 0, MEM_RELEASE);
    return found;
}

// === Основная функция детекции DirectX в памяти ===
BOOLEAN DetectDirectXInMemory(HANDLE hProcess) {
    DIRECT3D11_INTERFACE result = { FALSE, nullptr, 0, FALSE };

    // Инициализируем результат
    result.Size = 0x10000;
    result.FoundSignature = FALSE;
    result.SignatureLocation = nullptr;
    result.IsDirectXModule = FALSE;

    // Проверяем наличие DirectX модулей в процессе
    PDIRECT3D11_INTERFACE foundInterfaces;
    ULONG interfaceCount = 0;

    if (FindDirectXModules(hProcess, foundInterfaces, interfaceCount)) {
        result.FoundSignature = TRUE;
        result.IsDirectXModule = TRUE;
        result.Size = sizeof(DIRECT3D11_INTERFACE) * interfaceCount;

        // Сохраняем найденные интерфейсы в глобальное состояние
        EnterCriticalSection(&g_DXSection);
        g_DXState.DX11Detected = TRUE;
        g_DXState.InterfaceCount = interfaceCount;
        memcpy(g_pDX11Interface, foundInterfaces, sizeof(DIRECT3D11_INTERFACE) * interfaceCount);
        LeaveCriticalSection(&g_DXSection);

        return TRUE;
    }

    // Если модули не найдены, ищем по сигнатурам в памяти
    if (pNtReadVirtualMemory && pNtQueryVirtualMemory) {
        SIZE_T bufferSize = 0x100000;

        BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (pBuffer) {
            NTSTATUS status = pNtReadVirtualMemory(hProcess, nullptr, pBuffer, bufferSize, &bytesRead);

            if (status == STATUS_SUCCESS && bytesRead > 0) {
                // Ищем сигнатуру Direct3D11
                for (SIZE_T i = 0; i < bytesRead - strlen(DX_SIGNATURE_1); i++) {
                    if (memcmp(pBuffer + i, DX_SIGNATURE_1, strlen(DX_SIGNATURE_1)) == 0) {
                        result.FoundSignature = TRUE;
                        result.SignatureLocation = (PVOID)(pBuffer + i);
                        result.Size = bytesRead;

                        EnterCriticalSection(&g_DXSection);
                        g_DXState.DX11Detected = TRUE;
                        LeaveCriticalSection(&g_DXSection);

                        VirtualFree(pBuffer, 0, MEM_RELEASE);
                        return TRUE;
                    }
                }

                // Ищем сигнатуру Direct3D9
                for (SIZE_T i = 0; i < bytesRead - strlen(DX_SIGNATURE_2); i++) {
                    if (memcmp(pBuffer + i, DX_SIGNATURE_2, strlen(DX_SIGNATURE_2)) == 0) {
                        result.FoundSignature = TRUE;
                        result.SignatureLocation = (PVOID)(pBuffer + i);
                        result.Size = bytesRead;

                        EnterCriticalSection(&g_DXSection);
                        g_DXState.DX11Detected = TRUE;
                        LeaveCriticalSection(&g_DXSection);

                        VirtualFree(pBuffer, 0, MEM_RELEASE);
                        return TRUE;
                    }
                }
            }

            VirtualFree(pBuffer, 0, MEM_RELEASE);
        }
    }

    EnterCriticalSection(&g_DXSection);
    g_DXState.DX11Detected = FALSE;
    LeaveCriticalSection(&g_DXSection);

    return FALSE;
}

// === Функция сканирования сигнатур DirectX ===
VOID ScanForDXSignatures(PDIRECT3D11_INTERFACE pResult) {
    if (!pResult || !g_DXState.IsInitialized) return;

    EnterCriticalSection(&g_DXSection);

    // Инициализируем результат
    pResult->FoundSignature = FALSE;
    pResult->SignatureLocation = nullptr;
    pResult->Size = 0;
    pResult->IsDirectXModule = FALSE;

    if (pNtReadVirtualMemory && g_DXState.DX11Detected) {
        for (ULONG i = 0; i < g_DXState.InterfaceCount; i++) {
            PDIRECT3D11_INTERFACE pInterface = &g_pDX11Interface[i];

            if (pNtReadVirtualMemory(g_hDirectXHook, pInterface->BaseAddress, nullptr, sizeof(DIRECT3D11_INTERFACE), nullptr)) {
                DIRECT3D11_INTERFACE localResult = { FALSE, nullptr, 0, FALSE };

                for (SIZE_T offset = 0; offset < pInterface->Size - strlen(DX_SIGNATURE_1); offset += 256) {
                    if (memcmp((PVOID)(pInterface->BaseAddress + offset), DX_SIGNATURE_1, strlen(DX_SIGNATURE_1)) == 0) {
                        localResult.FoundSignature = TRUE;
                        localResult.SignatureLocation = (PVOID)((BYTE*)pInterface->BaseAddress + offset);
                        localResult.Size = pInterface->Size;
                        localResult.IsDirectXModule = TRUE;

                        memcpy(pResult, &localResult, sizeof(DIRECT3D11_INTERFACE));

                        LeaveCriticalSection(&g_DXSection);
                        return;
                    }
                }
            }
        }
    }

    LeaveCriticalSection(&g_DXSection);
}

// === Функция перехвата вызовов Direct3D11 ===
BOOLEAN HookDirectX11Functions(HANDLE hProcess) {
    if (!hProcess || !g_DXState.IsInitialized) return FALSE;

    EnterCriticalSection(&g_DXSection);

    // Инициализируем состояние хука
    g_DXState.HookedFunctionsCount++;

    // Создаем буфер для модификации памяти
    SIZE_T bufferSize = 0x10000;
    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) {
        LeaveCriticalSection(&g_DXSection);
        return FALSE;
    }

    // Читаем память процесса для анализа
    SIZE_T bytesRead = 0;
    NTSTATUS status = pNtReadVirtualMemory(hProcess, nullptr, pBuffer, bufferSize, &bytesRead);

    if (status == STATUS_SUCCESS && bytesRead > 0) {
        BOOLEAN hooked = FALSE;

        // Ищем точки входа для модификации вызовов Direct3D11
        for (SIZE_T i = 0; i < bytesRead - strlen(DX_SIGNATURE_1); i++) {
            if (memcmp(pBuffer + i, DX_SIGNATURE_1, strlen(DX_SIGNATURE_1)) == 0) {
                BYTE* callLocation = (BYTE*)pBuffer + i;

                // Пример модификации: заменяем вызов на троллинг
                for (SIZE_T j = 0; j < strlen(DX_SIGNATURE_1); j++) {
                    callLocation[j] = 0x90; // NOP instruction
                }

                SIZE_t bytesWritten = 0;
                status = pNtWriteVirtualMemory(hProcess, (PVOID)(callLocation - i), pBuffer + i, strlen(DX_SIGNATURE_1), &bytesWritten);

                if (status == STATUS_SUCCESS) {
                    hooked = TRUE;
                }

                break;
            }
        }

        VirtualFree(pBuffer, 0, MEM_RELEASE);
    }

    LeaveCriticalSection(&g_DXSection);

    return hooked;
}

// === Функция перехвата вызовов Direct3D9 ===
BOOLEAN HookDirectX9Functions(HANDLE hProcess) {
    if (!hProcess || !g_DXState.IsInitialized) return FALSE;

    EnterCriticalSection(&g_DXSection);

    g_DXState.HookedFunctionsCount++;

    SIZE_T bufferSize = 0x10000;
    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) {
        LeaveCriticalSection(&g_DXSection);
        return FALSE;
    }

    SIZE_T bytesRead = 0;
    NTSTATUS status = pNtReadVirtualMemory(hProcess, nullptr, pBuffer, bufferSize, &bytesRead);

    if (status == STATUS_SUCCESS && bytesRead > 0) {
        BOOLEAN hooked = FALSE;

        for (SIZE_T i = 0; i < bytesRead - strlen(DX_SIGNATURE_2); i++) {
            if (memcmp(pBuffer + i, DX_SIGNATURE_2, strlen(DX_SIGNATURE_2)) == 0) {
                BYTE* callLocation = (BYTE*)pBuffer + i;

                for (SIZE_T j = 0; j < strlen(DX_SIGNATURE_2); j++) {
                    callLocation[j] = 0x90; // NOP instruction
                }

                SIZE_t bytesWritten = 0;
                status = pNtWriteVirtualMemory(hProcess, (PVOID)(callLocation - i), pBuffer + i, strlen(DX_SIGNATURE_2), &bytesWritten);

                if (status == STATUS_SUCCESS) {
                    hooked = TRUE;
                }

                break;
            }
        }

        VirtualFree(pBuffer, 0, MEM_RELEASE);
    }

    LeaveCriticalSection(&g_DXSection);

    return hooked;
}

// === Функция мониторинга GPU-активности ===
VOID MonitorGPUActivity(PTIMING_SAMPLE pSample) {
    if (!pSample || !g_DXState.IsInitialized) return;

    EnterCriticalSection(&g_DXSection);

    pSample->StartTick = QueryPerformanceCounter();

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    for (ULONG i = 0; i < g_DXState.InterfaceCount; i++) {
        PDIRECT3D11_INTERFACE pInterface = &g_pDX11Interface[i];

        ULONGLONG currentTime = QueryPerformanceCounter().QuadPart;

        if (pSample->TotalTime > 0) {
            ULONGLONG timeDiff = currentTime - pSample->StartTick.QuadPart;

            // Если время превышает порог, помечаем как аномалию
            if (timeDiff > 1000000) {
                pSample->IsAnomaly = TRUE;
            }
            else {
                pSample->IsAnomaly = FALSE;
            }

            pSample->EndTick.QuadPart = currentTime;
            pSample->TotalTime = timeDiff;
        }
    }

    LeaveCriticalSection(&g_DXSection);
}

// === Геттеры для глобальных переменных ===
PDIRECT3D11_INTERFACE GetFoundInterfaces() {
    EnterCriticalSection(&g_DXSection);
    PDIRECT3D11_INTERFACE result = g_pDX11Interface;
    LeaveCriticalSection(&g_DXSection);
    return result;
}

ULONG GetInterfaceCount() {
    EnterCriticalSection(&g_DXSection);
    ULONG result = g_DXState.InterfaceCount;
    LeaveCriticalSection(&g_DXSection);
    return result;
}

BOOLEAN IsDXDetected() {
    EnterCriticalSection(&g_DXSection);
    BOOLEAN result = g_DXState.DX11Detected;
    LeaveCriticalSection(&g_DXSection);
    return result;
}

// === Сброс состояния хука ===
VOID ResetHookState() {
    EnterCriticalSection(&g_DXSection);

    g_DXState.DX11Detected = FALSE;
    g_DXState.HookedFunctionsCount = 0;
    g_DXState.InterfaceCount = 0;

    LeaveCriticalSection(&g_DXSection);
}
