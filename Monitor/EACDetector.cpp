#include "EACDetector.h"
#include <vector>
#include <string>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>

// === Глобальные переменные ===
HANDLE g_hEACDetector = NULL;
CRITICAL_SECTION g_EACLSection;
EAC_DETECTION_STATE g_EACState = { 0 };
PEAC_MODULE_INFO g_pFoundModules = nullptr;
ULONG g_ModuleCount = 0;
BOOLEAN g_IsInitialized = FALSE;

// === Внутренние структуры для сканирования ===
typedef struct _MODULE_LIST_ENTRY {
    HANDLE hModule;
    PVOID BaseAddress;
    SIZE_T Size;
} MODULE_LIST_ENTRY, * PMODULE_LIST_ENTRY;

// === Внутренние функции ===
static NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) = nullptr;
static NTSTATUS(NTAPI* pNtQueryVirtualMemory)(HANDLE ProcessHandle, HANDLE ProcessId, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID* Buffer, PSIZE_T BufferSize, SIZE_T* ReturnLength) = nullptr;
static NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE hProcess, PVOID lpBaseAddress, PVOID lpBuffer, SIZE_T nSize, PSIZE_T lpNumberOfBytesRead) = nullptr;
static NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE hProcess, PVOID lpBaseAddress, PVOID lpBuffer, SIZE_T nSize, PSIZE_T lpNumberOfBytesWritten) = nullptr;

// === Инициализация EAC Detector ===
VOID InitializeEACDetector() {
    if (g_IsInitialized) return;

    EnterCriticalSection(&g_EACLSection);

    if (!g_IsInitialized) {
        // Инициализируем критическую секцию
        InitializeCriticalSection(&g_EACLSection);

        // Получаем адреса внутренних функций NT
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            pNtQueryInformationProcess = (NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG))GetProcAddress(hNtdll, "NtQueryInformationProcess");
            pNtQueryVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, HANDLE, MEMORY_INFORMATION_CLASS, PVOID*, PSIZE_T, SIZE_T*))GetProcAddress(hNtdll, "NtQueryVirtualMemory");
            pNtReadVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T))GetProcAddress(hNtdll, "NtReadVirtualMemory");
            pNtWriteVirtualMemory = (NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T))GetProcAddress(hNtdll, "NtWriteVirtualMemory");
        }

        // Инициализируем состояние детекции
        g_EACState.Detected = FALSE;
        g_EACState.Bypassed = FALSE;
        g_EACState.DetectionCount = 0;
        g_ModuleCount = 0;
        g_IsInitialized = TRUE;

        // Выделяем память для найденных модулей
        SIZE_T moduleSize = sizeof(MODULE_LIST_ENTRY) * EAC_MAX_MODULES;
        g_pFoundModules = (PEAC_MODULE_INFO)VirtualAlloc(nullptr, moduleSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    LeaveCriticalSection(&g_EACLSection);
}

// === Очистка EAC Detector ===
VOID CleanupEACDetector() {
    EnterCriticalSection(&g_EACLSection);

    if (g_pFoundModules) {
        VirtualFree(g_pFoundModules, 0, MEM_RELEASE);
        g_pFoundModules = nullptr;
    }

    if (!IsEmpty(&g_EACLSection)) {
        DeleteCriticalSection(&g_EACLSection);
    }

    g_IsInitialized = FALSE;
    g_ModuleCount = 0;
}

// === Проверка на пустую критическую секцию ===
static BOOLEAN IsEmpty(CRITICAL_SECTION* cs) {
    return (cs->LockCount == 0 && cs->CreatorBackTraceIndex == 0);
}

// === Сканирование модулей процесса ===
static PMODULE_LIST_ENTRY ScanProcessModules(HANDLE hProcess, ULONG& ModuleCount) {
    MODULE_LIST_ENTRY* modules = nullptr;

    // Получаем список модулей через NtQueryInformationProcess
    if (pNtQueryInformationProcess && pNtQueryVirtualMemory) {
        PROCESS_MEMORY_INFORMATION pmInfo;
        SIZE_T infoSize = sizeof(PROCESS_MEMORY_INFORMATION);

        NTSTATUS status = pNtQueryInformationProcess(hProcess, ProcessBasicInfo, &pmInfo, infoSize, nullptr);

        if (status == STATUS_SUCCESS && pmInfo.BaseAddress) {
            // Получаем список модулей через EnumProcessModulesEx
            HMODULE hMods = GetModuleHandleA(nullptr);

            MODULEENTRY32W moduleEntry;
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);

            if (hSnap != INVALID_HANDLE_VALUE) {
                modules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, sizeof(MODULE_LIST_ENTRY), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                MODULEENTRY32W* pModule = &moduleEntry;
                pModule->dwSize = sizeof(MODULEENTRY32W);

                if (Module32FirstW(hSnap, pModule)) {
                    do {
                        SIZE_T size = sizeof(MODULE_LIST_ENTRY) * 1024; // Увеличиваем буфер при необходимости
                        MODULE_LIST_ENTRY* tempModules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                        if (tempModules && modules) {
                            SIZE_T newSize = sizeof(MODULE_LIST_ENTRY) * 2048;
                            tempModules = (MODULE_LIST_ENTRY*)VirtualAlloc(nullptr, newSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                            if (tempModules) {
                                modules = tempModules;
                            }
                        }

                        // Добавляем модуль в список
                        if (modules && ModuleCount < EAC_MAX_MODULES) {
                            PMODULE_LIST_ENTRY pMod = &modules[ModuleCount];
                            pMod->hModule = hProcess;
                            pMod->BaseAddress = (PVOID)pModule->modBaseAddr;
                            pMod->Size = pModule->modBaseSize;

                            ModuleCount++;
                        }

                    } while (Module32NextW(hSnap, pModule) && ModuleCount < EAC_MAX_MODULES);
                }

                CloseHandle(hSnap);
            }
        }
    }

    return modules;
}

// === Поиск сигнатур в памяти ===
static BOOLEAN FindSignatureInMemory(HANDLE hProcess, PVOID BaseAddress, SIZE_T Size, const char* Signature, PVOID& FoundLocation) {
    SIZE_T sigLen = strlen(Signature);

    // Создаем буфер для чтения памяти
    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, Size + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) return FALSE;

    BOOLEAN found = FALSE;
    SIZE_T bytesRead = 0;

    // Читаем память по блокам для поиска сигнатуры
    for (SIZE_T offset = 0; offset < Size - sigLen; offset += 256) {
        if (pNtReadVirtualMemory(hProcess, (PVOID)(BaseAddress + offset), pBuffer, 256, &bytesRead)) {
            // Сравниваем сигнатуру с прочитанной памятью
            for (SIZE_T i = 0; i < sigLen && i < bytesRead; i++) {
                if (pBuffer[i] == Signature[i]) {
                    // Найдена совпадение, проверяем контекст
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

// === Поиск модулей EAC ===
static BOOLEAN FindEACModules(HANDLE hProcess, PEAC_MODULE_INFO& FoundModules, ULONG& ModuleCount) {
    MODULE_LIST_ENTRY* modules = ScanProcessModules(hProcess, ModuleCount);

    if (!modules || ModuleCount == 0) return FALSE;

    // Проверяем каждый модуль на наличие EAC сигнатур
    for (ULONG i = 0; i < ModuleCount; i++) {
        PMODULE_LIST_ENTRY pMod = &modules[i];

        // Проверяем имя модуля
        if (pNtQueryInformationProcess && pNtReadVirtualMemory) {
            char moduleName[MAX_PATH] = { 0 };

            NTSTATUS status = pNtQueryInformationProcess(hProcess, ProcessModuleInfo, &pMod->hModule, sizeof(pMod->hModule), nullptr);

            if (status == STATUS_SUCCESS && strlen(moduleName) > 0) {
                // Проверяем на наличие EAC в имени
                if (strstr(moduleName, "EAC") || strstr(moduleName, "EasyAntiCheat")) {
                    PEAC_MODULE_INFO pEacMod = &FoundModules[i];
                    pEacMod->hModule = pMod->hModule;
                    pEacMod->BaseAddress = pMod->BaseAddress;
                    pEacMod->Size = pMod->Size;
                    pEacMod->IsInjected = TRUE;

                    ModuleCount++;
                }
            }
        }
    }

    return (ModuleCount > 0);
}

// === Основная функция детекции EAC в памяти ===
BOOLEAN DetectEACInMemory(HANDLE hProcess) {
    MEMORY_SCAN_RESULT result = { FALSE, nullptr, 0, FALSE };

    // Инициализируем результат
    result.ScanSize = 0x10000; // Начинаем с 64KB блока
    result.FoundSignature = FALSE;
    result.SignatureLocation = nullptr;
    result.IsEACModule = FALSE;

    // Проверяем наличие EAC модулей в процессе
    PEAC_MODULE_INFO foundModules;
    ULONG moduleCount = 0;

    if (FindEACModules(hProcess, foundModules, moduleCount)) {
        result.FoundSignature = TRUE;
        result.IsEACModule = TRUE;
        result.ScanSize = sizeof(EAC_MODULE_INFO) * moduleCount;

        // Сохраняем найденные модули в глобальное состояние
        EnterCriticalSection(&g_EACLSection);
        g_EACState.Detected = TRUE;
        g_EACState.ModuleCount = moduleCount;
        memcpy(g_pFoundModules, foundModules, sizeof(EAC_MODULE_INFO) * moduleCount);
        LeaveCriticalSection(&g_EACLSection);

        return TRUE;
    }

    // Если модули не найдены, ищем по сигнатурам в памяти
    if (pNtReadVirtualMemory && pNtQueryVirtualMemory) {
        SIZE_T bufferSize = 0x100000; // 1MB буфер для сканирования

        BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (pBuffer) {
            NTSTATUS status = pNtReadVirtualMemory(hProcess, nullptr, pBuffer, bufferSize, &bytesRead);

            if (status == STATUS_SUCCESS && bytesRead > 0) {
                // Ищем первую сигнатуру EAC
                for (SIZE_T i = 0; i < bytesRead - strlen(EAC_SIGNATURE_1); i++) {
                    if (memcmp(pBuffer + i, EAC_SIGNATURE_1, strlen(EAC_SIGNATURE_1)) == 0) {
                        result.FoundSignature = TRUE;
                        result.SignatureLocation = (PVOID)(pBuffer + i);
                        result.ScanSize = bytesRead;

                        EnterCriticalSection(&g_EACLSection);
                        g_EACState.Detected = TRUE;
                        LeaveCriticalSection(&g_EACLSection);

                        VirtualFree(pBuffer, 0, MEM_RELEASE);
                        return TRUE;
                    }
                }

                // Ищем вторую сигнатуру EAC
                for (SIZE_T i = 0; i < bytesRead - strlen(EAC_SIGNATURE_2); i++) {
                    if (memcmp(pBuffer + i, EAC_SIGNATURE_2, strlen(EAC_SIGNATURE_2)) == 0) {
                        result.FoundSignature = TRUE;
                        result.SignatureLocation = (PVOID)(pBuffer + i);
                        result.ScanSize = bytesRead;

                        EnterCriticalSection(&g_EACLSection);
                        g_EACState.Detected = TRUE;
                        LeaveCriticalSection(&g_EACLSection);

                        VirtualFree(pBuffer, 0, MEM_RELEASE);
                        return TRUE;
                    }
                }
            }

            VirtualFree(pBuffer, 0, MEM_RELEASE);
        }
    }

    // Если ничего не найдено
    EnterCriticalSection(&g_EACLSection);
    g_EACState.Detected = FALSE;
    LeaveCriticalSection(&g_EACLSection);

    return FALSE;
}

// === Функция сканирования сигнатур EAC ===
VOID ScanForESignatures(PMEMORY_SCAN_RESULT pResult) {
    if (!pResult || !g_IsInitialized) return;

    EnterCriticalSection(&g_EACLSection);

    // Инициализируем результат
    pResult->FoundSignature = FALSE;
    pResult->SignatureLocation = nullptr;
    pResult->ScanSize = 0;
    pResult->IsEACModule = FALSE;

    if (pNtReadVirtualMemory && g_EACState.Detected) {
        // Используем глобальное состояние для сканирования
        for (ULONG i = 0; i < g_EACState.ModuleCount; i++) {
            PEAC_MODULE_INFO pMod = &g_pFoundModules[i];

            if (pNtReadVirtualMemory(g_hEACDetector, pMod->BaseAddress, nullptr, sizeof(MEMORY_SCAN_RESULT), nullptr)) {
                MEMORY_SCAN_RESULT localResult = { FALSE, nullptr, 0, FALSE };

                // Сканируем каждый модуль на наличие сигнатур
                for (SIZE_T offset = 0; offset < pMod->Size - strlen(EAC_SIGNATURE_1); offset += 256) {
                    if (memcmp((PVOID)(pMod->BaseAddress + offset), EAC_SIGNATURE_1, strlen(EAC_SIGNATURE_1)) == 0) {
                        localResult.FoundSignature = TRUE;
                        localResult.SignatureLocation = (PVOID)((BYTE*)pMod->BaseAddress + offset);
                        localResult.ScanSize = pMod->Size;
                        localResult.IsEACModule = TRUE;

                        memcpy(pResult, &localResult, sizeof(MEMORY_SCAN_RESULT));

                        LeaveCriticalSection(&g_EACLSection);
                        return;
                    }
                }
            }
        }
    }

    LeaveCriticalSection(&g_EACLSection);
}

// === Функция байпаса проверок EAC ===
BOOLEAN BypassEACChecks(HANDLE hProcess) {
    if (!hProcess || !g_IsInitialized) return FALSE;

    EnterCriticalSection(&g_EACLSection);

    // Инициализируем состояние байпаса
    g_EACState.Bypassed = TRUE;
    g_EACState.DetectionCount++;

    // Создаем буфер для модификации памяти
    SIZE_T bufferSize = 0x10000;
    BYTE* pBuffer = (BYTE*)VirtualAlloc(nullptr, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pBuffer) {
        LeaveCriticalSection(&g_EACLSection);
        return FALSE;
    }

    // Читаем память процесса для анализа
    SIZE_T bytesRead = 0;
    NTSTATUS status = pNtReadVirtualMemory(hProcess, nullptr, pBuffer, bufferSize, &bytesRead);

    if (status == STATUS_SUCCESS && bytesRead > 0) {
        BOOLEAN bypassed = FALSE;

        // Ищем точки входа для модификации проверок EAC
        for (SIZE_T i = 0; i < bytesRead - strlen(EAC_SIGNATURE_1); i++) {
            if (memcmp(pBuffer + i, EAC_SIGNATURE_1, strlen(EAC_SIGNATURE_1)) == 0) {
                // Найдена точка проверки, модифицируем её
                BYTE* checkLocation = (BYTE*)pBuffer + i;

                // Пример модификации: заменяем проверку на NOP
                for (SIZE_T j = 0; j < strlen(EAC_SIGNATURE_1); j++) {
                    checkLocation[j] = 0x90; // NOP instruction
                }

                // Записываем изменения обратно в процесс
                SIZE_t bytesWritten = 0;
                status = pNtWriteVirtualMemory(hProcess, (PVOID)(checkLocation - i), pBuffer + i, strlen(EAC_SIGNATURE_1), &bytesWritten);

                if (status == STATUS_SUCCESS) {
                    bypassed = TRUE;
                }

                break; // Переходим к следующей точке проверки после успешного байпаса
            }
        }

        VirtualFree(pBuffer, 0, MEM_RELEASE);
    }

    LeaveCriticalSection(&g_EACLSection);

    return bypassed;
}

// === Функция мониторинга поведения EAC ===
VOID MonitorEACBehavior(PTIMING_SAMPLE pSample) {
    if (!pSample || !g_IsInitialized) return;

    EnterCriticalSection(&g_EACLSection);

    // Инициализируем образец времени
    pSample->StartTick = QueryPerformanceCounter();

    // Запускаем мониторинг через периодическую проверку
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    for (ULONG i = 0; i < g_EACState.ModuleCount; i++) {
        PEAC_MODULE_INFO pMod = &g_pFoundModules[i];

        // Проверяем время выполнения операций с модулем EAC
        ULONGLONG currentTime = QueryPerformanceCounter().QuadPart;

        if (pSample->TotalTime > 0) {
            // Вычисляем разницу времени
            ULONGLONG timeDiff = currentTime - pSample->StartTick.QuadPart;

            // Если время превышает порог, помечаем как аномалию
            if (timeDiff > 1000000) { // 1 секунда в наносекундах
                pSample->IsAnomaly = TRUE;
            }
            else {
                pSample->IsAnomaly = FALSE;
            }

            pSample->EndTick.QuadPart = currentTime;
            pSample->TotalTime = timeDiff;
        }
    }

    LeaveCriticalSection(&g_EACLSection);
}

// === Геттеры для глобальных переменных ===
PEAC_MODULE_INFO GetFoundModules() {
    EnterCriticalSection(&g_EACLSection);
    PEAC_MODULE_INFO result = g_pFoundModules;
    LeaveCriticalSection(&g_EACLSection);
    return result;
}

ULONG GetModuleCount() {
    EnterCriticalSection(&g_EACLSection);
    ULONG result = g_ModuleCount;
    LeaveCriticalSection(&g_EACLSection);
    return result;
}

BOOLEAN IsEACDetected() {
    EnterCriticalSection(&g_EACLSection);
    BOOLEAN result = g_EACState.Detected;
    LeaveCriticalSection(&g_EACLSection);
    return result;
}

BOOLEAN IsEACBypassed() {
    EnterCriticalSection(&g_EACLSection);
    BOOLEAN result = g_EACState.Bypassed;
    LeaveCriticalSection(&g_EACLSection);
    return result;
}

// === Сброс состояния детекции ===
VOID ResetDetectionState() {
    EnterCriticalSection(&g_EACLSection);

    g_EACState.Detected = FALSE;
    g_EACState.Bypassed = FALSE;
    g_EACState.DetectionCount = 0;
    g_ModuleCount = 0;

    LeaveCriticalSection(&g_EACLSection);
}
