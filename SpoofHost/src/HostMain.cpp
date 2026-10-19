#include <windows.h>
#include <stdio.h>
#include "HostTypes.h"
#include "HostAPI.h"
#include "HostConfig.h"

// === Глобальные переменные ===
HANDLE g_hSpoofDevice = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_DeviceLock;

// === Инициализация критической секции ===
VOID InitDeviceLock() {
    InitializeCriticalSection(&g_DeviceLock);
}

// === Открытие устройства драйвера ===
HANDLE OpenSpoofDevice() {
    HANDLE hDevice = CreateFile(
        L"\\\\.\\SpoofLink",           // Символическая ссылка драйвера
        GENERIC_READ | GENERIC_WRITE,  // Доступ
        0,                             // Исключительный доступ
        NULL,                          // Безопасность по умолчанию
        OPEN_EXISTING,                 // Устройство должно существовать
        FILE_ATTRIBUTE_NORMAL,         // Атрибуты файла
        NULL                           // Шаблон файловой обработки
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Host] Failed to open SpoofLink device\n");
        return INVALID_HANDLE_VALUE;
    }

    return hDevice;
}

// === Закрытие устройства ===
VOID CloseSpoofDevice(HANDLE hDevice) {
    if (hDevice != INVALID_HANDLE_VALUE && hDevice != NULL) {
        CloseHandle(hDevice);
    }
}

// === Получение статуса драйвера ===
NTSTATUS GetSpoofStatus(HANDLE hDevice, PSPOOF_STATUS pStatus) {
    if (!hDevice || !pStatus) return STATUS_INVALID_PARAMETER;

    SPOOF_STATUS status = { 0 };

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_GET_STATUS,
        NULL,
        0,
        &status,
        sizeof(status),
        &bytesReturned,
        NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to get spoof status: %lu\n", GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    *pStatus = status;
    return STATUS_SUCCESS;
}

// === Создание снапшота ===
NTSTATUS CreateSnapshot(HANDLE hDevice, PULONG pSnapshotId) {
    if (!hDevice || !pSnapshotId) return STATUS_INVALID_PARAMETER;

    ULONG snapshotId = 0;

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_CREATE_SNAPSHOT,
        NULL,
        0,
        &snapshotId,
        sizeof(snapshotId),
        &bytesReturned,
        NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to create snapshot: %lu\n", GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    *pSnapshotId = snapshotId;
    return STATUS_SUCCESS;
}

// === Откат к снапшоту ===
NTSTATUS RollbackSnapshot(HANDLE hDevice, ULONG SnapshotId) {
    if (!hDevice || SnapshotId == 0) return STATUS_INVALID_PARAMETER;

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_ROLLBACK,
        &SnapshotId,
        sizeof(SnapshotId),
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to rollback snapshot %lu: %lu\n",
            SnapshotId, GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// === Перехват системных вызовов (для поведенческого анализа) ===
VOID HookSystemCalls() {
    // Пример хука на NtQuerySystemInformation для мониторинга изменений времени
    PULONG ssdt = (PULONG)MmGetSystemRoutineAddress(&PsLookupProcessByProcessId);

    if (!ssdt) return;

    // Сохраняем оригинальный адрес
    ULONG originalHandler = *ssdt;

    // Записываем наш хук (упрощённо для примера)
    *ssdt = 0x12345678; // Адрес нашей функции-обёртки

    fprintf(stderr, "[Host] System call hook installed at %p\n", ssdt);
}

// === Точка входа хост-сервиса ===
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nShowCmd) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    fprintf(stderr, "[Host] SpoofHost.exe starting...\n");

    // Инициализация критической секции
    InitDeviceLock();

    // Открытие устройства драйвера
    g_hSpoofDevice = OpenSpoofDevice();
    if (g_hSpoofDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Host] Failed to initialize device\n");
        return 1;
    }

    // Загрузка конфигурации
    SPOOF_STATUS status = { 0 };
    GetSpoofStatus(g_hSpoofDevice, &status);

    if (status.IsSpoofingActive) {
        fprintf(stderr, "[Host] Spoofing already active\n");
    }
    else {
        fprintf(stderr, "[Host] Starting spoofing...\n");
        StartSpoofing(g_hSpoofDevice);
    }

    // Перехват системных вызовов (для анализа)
    HookSystemCalls();

    fprintf(stderr, "[Host] SpoofHost.exe running. Press Ctrl+C to exit.\n");

    // Основной цикл — ждём команды от CLI или таймера
    while (true) {
        Sleep(1000);  // Ждём 1 секунду

        // Здесь можно добавить мониторинг процессов, DLL injections и т.д.
    }

    return 0;
}
