#include <windows.h>
#include <stdio.h>
#include "HostTypes.h"
#include "HostAPI.h"
#include "HostConfig.h"

// === Глобальные переменные ===
HMODULE g_hConfigModule = NULL;
CRITICAL_SECTION g_ConfigLock;

// Инициализация критической секции конфигурации
VOID InitConfig() {
    InitializeCriticalSection(&g_ConfigLock);
}

// Загрузка конфигурации из файла
BOOLEAN LoadConfig(PSPOOF_FLAGS pFlags) {
    if (!pFlags) return FALSE;

    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Config file not found: %s\n", CONFIG_FILE);
        return FALSE;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize > 4096) {
        CloseHandle(hFile);
        fprintf(stderr, "[Config] Invalid config file size: %lu\n", fileSize);
        return FALSE;
    }

    BYTE* buffer = (BYTE*)malloc(fileSize);
    if (!buffer) {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        free(buffer);
        CloseHandle(hFile);
        fprintf(stderr, "[Config] Failed to read config file\n");
        return FALSE;
    }

    CloseHandle(hFile);

    // Парсинг (упрощённый)
    SPOOF_FLAGS flags = { 0 };

    if (bytesRead >= sizeof(SPOOF_STATUS)) {
        PSPOOF_STATUS status = (PSPOOF_STATUS)buffer;
        flags.Flags = status->SpoofFlags;
    }

    free(buffer);

    *pFlags = flags;
    fprintf(stderr, "[Config] Loaded config: flags=0x%X\n", flags.Flags);
    return TRUE;
}

// Сохранение конфигурации в файл
BOOLEAN SaveConfig(const SPOOF_FLAGS* pFlags) {
    if (!pFlags) return FALSE;

    HANDLE hFile = CreateFile(CONFIG_FILE, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, CREATE_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to create config file\n");
        return FALSE;
    }

    SPOOF_STATUS status = { 0 };
    status.SpoofFlags = pFlags->Flags;

    DWORD bytesWritten = 0;
    BOOL result = WriteFile(hFile, &status, sizeof(status), &bytesWritten, NULL);

    CloseHandle(hFile);

    if (result && bytesWritten == sizeof(status)) {
        fprintf(stderr, "[Config] Saved config successfully\n");
        return TRUE;
    }

    fprintf(stderr, "[Config] Failed to save config: %lu\n", GetLastError());
    return FALSE;
}

// Создание снапшота текущего состояния
BOOLEAN CreateSnapshot(ULONG* pSnapshotId) {
    if (!pSnapshotId) return FALSE;

    HANDLE hDevice = g_hSpoofDevice;
    if (!hDevice) {
        fprintf(stderr, "[Config] No device handle available\n");
        return FALSE;
    }

    // Создаём снапшот через драйвер
    NTSTATUS status = CreateSnapshot(hDevice, pSnapshotId);

    if (status != STATUS_SUCCESS) {
        fprintf(stderr, "[Config] Failed to create snapshot via driver\n");
        return FALSE;
    }

    // Сохраняем метаданные снапшота локально
    SNAPSHOT_INFO info = { 0 };
    info.Id = *pSnapshotId;

    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    info.CreateTime = now;

    HANDLE hSnapFile = CreateFile(
        (SNAPSHOT_DIR L"snapshot_%lu.dat"),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hSnapFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to create snapshot file\n");
        return FALSE;
    }

    // Записываем метаданные и текущие флаги
    SNAPSHOT_HEADER header = { 0 };
    header.Magic = SNAPSHOT_MAGIC;
    header.Version = SNAPSHOT_VERSION;

    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    header.Timestamp = ftNow.dwHighDateTime | ((ULONG)ftNow.dwLowDateTime << 32);
    header.Flags = g_ConfigModule ? *(PSPOOF_FLAGS)g_ConfigModule : (SPOOF_FLAGS) { 0 };

    DWORD written = 0;
    WriteFile(hSnapFile, &header, sizeof(header), &written, NULL);
    WriteFile(hSnapFile, &info, sizeof(info), &written, NULL);

    CloseHandle(hSnapFile);

    fprintf(stderr, "[Config] Snapshot %lu created successfully\n", *pSnapshotId);
    return TRUE;
}

// Откат к сохранённому снапшоту
BOOLEAN RollbackSnapshot(ULONG SnapshotId) {
    if (SnapshotId == 0) {
        fprintf(stderr, "[Config] Invalid snapshot ID: %lu\n", SnapshotId);
        return FALSE;
    }

    // Загружаем метаданные снапшота
    HANDLE hSnapFile = CreateFile(
        (SNAPSHOT_DIR L"snapshot_%lu.dat"),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hSnapFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Snapshot file not found: snapshot_%lu.dat\n", SnapshotId);
        return FALSE;
    }

    SNAPSHOT_HEADER header = { 0 };
    DWORD bytesRead = 0;

    BOOL result = ReadFile(hSnapFile, &header, sizeof(header), &bytesRead, NULL);
    if (!result || bytesRead != sizeof(header)) {
        CloseHandle(hSnapFile);
        fprintf(stderr, "[Config] Failed to read snapshot header\n");
        return FALSE;
    }

    // Проверяем магическое число
    if (header.Magic != SNAPSHOT_MAGIC) {
        CloseHandle(hSnapFile);
        fprintf(stderr, "[Config] Invalid snapshot magic number\n");
        return FALSE;
    }

    // Откатываем через драйвер
    NTSTATUS status = RollbackSnapshot(g_hSpoofDevice, SnapshotId);

    if (status != STATUS_SUCCESS) {
        CloseHandle(hSnapFile);
        fprintf(stderr, "[Config] Failed to rollback via driver\n");
        return FALSE;
    }

    CloseHandle(hSnapFile);
    fprintf(stderr, "[Config] Rolled back to snapshot %lu\n", SnapshotId);
    return TRUE;
}

// Список всех доступных снапшотов
BOOLEAN ListSnapshots(PULONG pCount, PULONG pIds) {
    if (!pCount || !pIds) return FALSE;

    *pCount = 0;

    HANDLE hDir = CreateFile(SNAPSHOT_DIR, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_DIRECTORY, NULL);

    if (hDir == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to open snapshot directory\n");
        return FALSE;
    }

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((SNAPSHOT_DIR L"*"), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        CloseHandle(hDir);
        fprintf(stderr, "[Config] No snapshots found\n");
        return FALSE;
    }

    ULONG count = 0;
    do {
        if (findData.cFileName[0] != '.' &&
            findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        // Ищем файлы snapshot_*.dat
        if (strstr(findData.cFileName, "snapshot_") &&
            strstr(findData.cFileName, ".dat")) {

            ULONG id = 0;
            sscanf(findData.cFileName + strlen("snapshot_"), "%lu", &id);

            pIds[count++] = id;
        }
    } while (FindNextFileA(hFind, &findData) != FALSE);

    FindClose(hFind);
    CloseHandle(hDir);

    *pCount = count;
    fprintf(stderr, "[Config] Found %lu snapshots\n", count);
    return TRUE;
}

// Удаление снапшота
BOOLEAN DeleteSnapshot(ULONG SnapshotId) {
    if (SnapshotId == 0) return FALSE;

    char filename[256];
    snprintf(filename, sizeof(filename), SNAPSHOT_DIR "snapshot_%lu.dat", SnapshotId);

    BOOL result = DeleteFileA(filename);

    if (result) {
        fprintf(stderr, "[Config] Deleted snapshot %lu\n", SnapshotId);
        return TRUE;
    }

    fprintf(stderr, "[Config] Failed to delete snapshot: %lu\n", GetLastError());
    return FALSE;
}
