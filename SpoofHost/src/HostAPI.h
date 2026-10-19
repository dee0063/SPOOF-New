#pragma once
#include <windows.h>
#include "HostTypes.h"

// === Функции для открытия/закрытия устройства ===
HANDLE OpenSpoofDevice();
VOID   CloseSpoofDevice(HANDLE hDevice);

// === Управление спуфингом ===
NTSTATUS StartSpoofing(HANDLE hDevice);
NTSTATUS StopSpoofing(HANDLE hDevice);
NTSTATUS SetSpoofFlags(HANDLE hDevice, ULONG Flags);
NTSTATUS GetSpoofStatus(HANDLE hDevice, PSPOOF_STATUS pStatus);

// === Снапшоты и откат ===
NTSTATUS CreateSnapshot(HANDLE hDevice, PULONG pSnapshotId);
NTSTATUS RollbackSnapshot(HANDLE hDevice, ULONG SnapshotId);
NTSTATUS ListSnapshots(HANDLE hDevice, PULONG pCount, PULONG pIds);

// === Внутренние структуры (не для экспорта) ===
#pragma pack(push, 1)
typedef struct _SNAPSHOT_INFO {
    ULONG       Id;
    FILETIME    CreateTime;
    SPOOF_FLAGS Flags;
} SNAPSHOT_INFO, * PSNAPSHOT_INFO;
#pragma pack(pop)

// === Глобальные переменные (ВНЕ структур!) ===
extern HANDLE          g_hSpoofDevice;
extern CRITICAL_SECTION g_DeviceLock;