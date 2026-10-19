#pragma once
#include <windows.h>
#include "HostTypes.h"

// === Настройки конфигурации ===
#define MAX_SNAPSHOTS 16
#define CONFIG_FILE   L".spoof\\config.dat"
#define SNAPSHOT_DIR  L".spoof\\snapshots\\"

// === Структура снапшота ===
#pragma pack(push, 1)
typedef struct _SNAPSHOT_HEADER {
    ULONG Magic;       // Магическое число для проверки целостности
    ULONG Version;     // Версия формата снапшота
    ULONG Timestamp;   // Время создания (FILETIME)
    ULONG Flags;       // Сохранённые флаги спуфинга
} SNAPSHOT_HEADER, * PSNAPSHOT_HEADER;

typedef struct _SNAPSHOT_DATA {
    SNAPSHOT_HEADER Header;
    BYTE            Data[1];  // Динамический массив данных
} SNAPSHOT_DATA, * PSNAPSHOT_DATA;
#pragma pack(pop)

#define SNAPSHOT_MAGIC   0x53504F4F  // "SPOO" в hex
#define SNAPSHOT_VERSION 1

// === Функции конфигурации (объявления) ===
void    InitConfig();
BOOLEAN LoadConfig(PSPOOF_FLAGS pFlags);
BOOLEAN SaveConfig(const SPOOF_FLAGS* pFlags);
BOOLEAN CreateSnapshot(ULONG* pSnapshotId);
BOOLEAN RollbackSnapshot(ULONG SnapshotId);
BOOLEAN ListSnapshots(PULONG pCount, PULONG pIds);
BOOLEAN DeleteSnapshot(ULONG SnapshotId);

// === Глобальные переменные (ВНЕ структур!) ===
extern HMODULE          g_hConfigModule;
extern CRITICAL_SECTION g_ConfigLock;