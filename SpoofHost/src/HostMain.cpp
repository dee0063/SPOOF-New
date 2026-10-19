#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <ntstatus.h>  // <-- ДОБАВЛЕНО
#include "HostTypes.h"
#include "HostAPI.h"
#include "HostConfig.h"
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

// === Глобальные переменные ===
HANDLE g_hSpoofDevice = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_DeviceLock;

// === Инициализация критической секции ===
VOID InitDeviceLock() {
    InitializeCriticalSection(&g_DeviceLock);
}

// === Открытие устройства драйвера ===
HANDLE OpenSpoofDevice() {
    HANDLE hDevice = CreateFileW(  // <-- ИСПРАВЛЕНО: CreateFileW вместо CreateFile
        L"\\\\.\\SpoofLink",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Host] Failed to open SpoofLink device: %lu\n", GetLastError());
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

// === Запуск спуфинга ===
NTSTATUS StartSpoofing(HANDLE hDevice) {
    if (!hDevice || hDevice == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_START_SPOOFING,
        NULL, 0, NULL, 0,
        &bytesReturned, NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to start spoofing: %lu\n", GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// === Остановка спуфинга ===
NTSTATUS StopSpoofing(HANDLE hDevice) {
    if (!hDevice || hDevice == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STOP_SPOOFING,
        NULL, 0, NULL, 0,
        &bytesReturned, NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to stop spoofing: %lu\n", GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// === Установка флагов спуфинга ===
NTSTATUS SetSpoofFlags(HANDLE hDevice, ULONG Flags) {
    if (!hDevice || hDevice == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_SET_FLAGS,
        &Flags, sizeof(Flags),
        NULL, 0,
        &bytesReturned, NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to set spoof flags: %lu\n", GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// === Получение статуса драйвера ===
NTSTATUS GetSpoofStatus(HANDLE hDevice, PSPOOF_STATUS pStatus) {
    if (!hDevice || !pStatus) {
        return STATUS_INVALID_PARAMETER;
    }

    SPOOF_STATUS status = { 0 };
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_GET_STATUS,
        NULL, 0,
        &status, sizeof(status),
        &bytesReturned, NULL
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
    if (!hDevice || !pSnapshotId) {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG snapshotId = 0;
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_CREATE_SNAPSHOT,
        NULL, 0,
        &snapshotId, sizeof(snapshotId),
        &bytesReturned, NULL
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
    if (!hDevice || SnapshotId == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_ROLLBACK,
        &SnapshotId, sizeof(SnapshotId),
        NULL, 0,
        &bytesReturned, NULL
    );

    if (!result) {
        fprintf(stderr, "[Host] Failed to rollback snapshot %lu: %lu\n",
            SnapshotId, GetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

// === Список снапшотов ===
NTSTATUS ListSnapshots(HANDLE hDevice, PULONG pCount, PULONG pIds) {
    if (!hDevice || !pCount) {
        return STATUS_INVALID_PARAMETER;
    }

    // Заглушка - реальная реализация требует поддержки от драйвера
    *pCount = 0;

    fprintf(stdout, "[Host] ListSnapshots: No snapshots available (driver support required)\n");
    return STATUS_SUCCESS;
}

// === Точка входа хост-сервиса ===
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    fprintf(stdout, "[Host] SpoofHost.exe starting...\n");

    // Инициализация критической секции
    InitDeviceLock();

    // Открытие устройства драйвера
    g_hSpoofDevice = OpenSpoofDevice();
    if (g_hSpoofDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Host] Failed to initialize device. Is the driver loaded?\n");
        fprintf(stderr, "[Host] Run as Administrator and ensure SpoofDriver.sys is loaded.\n");
        return 1;
    }

    // Получение статуса
    SPOOF_STATUS status = { 0 };
    NTSTATUS ntStatus = GetSpoofStatus(g_hSpoofDevice, &status);

    if (NT_SUCCESS(ntStatus)) {
        fprintf(stdout, "[Host] Current status: %s\n",
            status.IsSpoofingActive ? "ACTIVE" : "INACTIVE");
    }

    fprintf(stdout, "[Host] SpoofHost.exe running. Press Ctrl+C to exit.\n");

    // Основной цикл
    while (true) {
        Sleep(1000);
    }

    // Очистка
    CloseSpoofDevice(g_hSpoofDevice);
    DeleteCriticalSection(&g_DeviceLock);

    return 0;
}