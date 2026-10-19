#pragma once

#include <windows.h>
#include "HostTypes.h"

// === Команды терминала ===
#define CMD_NONE              0x00
#define CMD_START             0x01
#define CMD_STOP              0x02
#define CMD_STATUS            0x03
#define CMD_SNAPSHOT          0x04
#define CMD_ROLLBACK          0x05
#define CMD_LIST              0x06
#define CMD_FLAGS             0x07

// === Структуры для обмена данными с драйвером ===
#pragma pack(push, 1)

typedef struct _CLI_STATUS_REQUEST {
    ULONG Command;      // Команда (CMD_*)
    ULONG SubCommand;   // Подкоманда (если нужна)
    ULONG Reserved[3];
} CLI_STATUS_REQUEST, * PCLI_STATUS_REQUEST;

typedef struct _CLI_SNAPSHOT_INFO {
    ULONG Id;
    FILETIME CreateTime;
    SPOOF_FLAGS Flags;
    BOOLEAN Valid;
} CLI_SNAPSHOT_INFO, * PCLI_SNAPSHOT_INFO;

#pragma pack(pop)

// === Функции (объявления для расширения) ===
VOID LogCommand(INT cmd);
BOOLEAN ValidateSnapshotId(ULONG id);
INT ParseSpoofFlags(const char* input, PULONG pFlags);

// === Глобальные переменные ===
extern HANDLE g_hDevice;
extern BOOLEAN g_IsSpoofingActive;
