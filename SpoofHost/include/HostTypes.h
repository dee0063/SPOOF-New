#pragma once

#include <windows.h>
#include <stdint.h>

// === IOCTL коды для драйвера ===
#define IOCTL_START_SPOOFING   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STOP_SPOOFING    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_STATUS       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SET_FLAGS        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_CREATE_SNAPSHOT  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ROLLBACK         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LIST_SNAPSHOTS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_READ_ACCESS)

// === Структуры для IOCTL ===

#pragma pack(push, 1)

typedef struct _SPOOF_STATUS {
    BOOLEAN IsSpoofingActive;      // 1 = активен, 0 = неактивен
    ULONG SpoofFlags;              // Текущие флаги спуфинга
    ULONG SnapshotCount;           // Количество сохранённых снапшотов
} SPOOF_STATUS, * PSPOOF_STATUS;

typedef struct _SPOOF_FLAGS {
    ULONG Flags;                   // Маска флага
