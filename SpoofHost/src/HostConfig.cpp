#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <ntstatus.h>  // <-- ДОБАВЛЕНО
#include "HostConfig.h"
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

// === Глобальные переменные ===
HMODULE g_hConfigModule = NULL;  // <-- ДОБАВЛЕНО: определение переменной
CRITICAL_SECTION g_ConfigLock;

// === Инициализация конфигурации ===
void InitConfig() {
    InitializeCriticalSection(&g_ConfigLock);
    fprintf(stdout, "[Config] Configuration initialized\n");
}

// === Загрузка конфигурации ===
BOOLEAN LoadConfig(PSPOOF_FLAGS pFlags) {
    if (!pFlags) return FALSE;

    HANDLE hFile = CreateFileW(  // <-- ИСПРАВЛЕНО: CreateFileW
        CONFIG_FILE,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to open config file: %lu\n", GetLastError());
        return FALSE;
    }

    SPOOF_FLAGS flags = { 0 };
    DWORD bytesRead = 0;
    BOOL result = ReadFile(hFile, &flags, sizeof(flags), &bytesRead, NULL);
    CloseHandle(hFile);

    if (!result || bytesRead != sizeof(flags)) {
        fprintf(stderr, "[Config] Failed to read config: %lu\n", GetLastError());
        return FALSE;
    }

    *pFlags = flags;
    fprintf(stdout, "[Config] Configuration loaded: Flags=0x%X\n", flags.Flags);
    return TRUE;
}

// === Сохранение конфигурации ===
BOOLEAN SaveConfig(const SPOOF_FLAGS* pFlags) {
    if (!pFlags) return FALSE;

    HANDLE hFile = CreateFileW(  // <-- ИСПРАВЛЕНО
        CONFIG_FILE,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to create config file: %lu\n", GetLastError());
        return FALSE;
    }

    DWORD bytesWritten = 0;
    BOOL result = WriteFile(hFile, pFlags, sizeof(*pFlags), &bytesWritten, NULL);
    CloseHandle(hFile);

    if (!result || bytesWritten != sizeof(*pFlags)) {
        fprintf(stderr, "[Config] Failed to write config: %lu\n", GetLastError());
        return FALSE;
    }

    fprintf(stdout, "[Config] Configuration saved: Flags=0x%X\n", pFlags->Flags);
    return TRUE;
}

// === Создание снапшота ===
BOOLEAN CreateSnapshot(ULONG* pSnapshotId) {
    if (!pSnapshotId) return FALSE;

    // Создаем директорию для снапшотов
    CreateDirectoryW(SNAPSHOT_DIR, NULL);

    // Генерируем ID снапшота
    static ULONG nextId = 1;
    *pSnapshotId = nextId++;

    // Создаем файл снапшота
    WCHAR filePath[MAX_PATH] = { 0 };
    swprintf_s(filePath, MAX_PATH, L"%s\\snapshot_%lu.dat", SNAPSHOT_DIR, *pSnapshotId);

    HANDLE hFile = CreateFileW(  // <-- ИСПРАВЛЕНО
        filePath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Failed to create snapshot file: %lu\n", GetLastError());
        return FALSE;
    }

    // Записываем заголовок
    SNAPSHOT_HEADER header = { 0 };
    header.Magic = SNAPSHOT_MAGIC;
    header.Version = SNAPSHOT_VERSION;
    GetSystemTimeAsFileTime((FILETIME*)&header.Timestamp);
    header.Flags = 0x1F;

    DWORD bytesWritten = 0;
    WriteFile(hFile, &header, sizeof(header), &bytesWritten, NULL);
    CloseHandle(hFile);

    fprintf(stdout, "[Config] Snapshot created: ID=%lu\n", *pSnapshotId);
    return TRUE;
}

// === Откат к снапшоту ===
BOOLEAN RollbackSnapshot(ULONG SnapshotId) {
    WCHAR filePath[MAX_PATH] = { 0 };
    swprintf_s(filePath, MAX_PATH, L"%s\\snapshot_%lu.dat", SNAPSHOT_DIR, SnapshotId);

    HANDLE hFile = CreateFileW(  // <-- ИСПРАВЛЕНО
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[Config] Snapshot %lu not found: %lu\n", SnapshotId, GetLastError());
        return FALSE;
    }

    SNAPSHOT_HEADER header = { 0 };
    DWORD bytesRead = 0;
    ReadFile(hFile, &header, sizeof(header), &bytesRead, NULL);
    CloseHandle(hFile);

    if (header.Magic != SNAPSHOT_MAGIC) {
        fprintf(stderr, "[Config] Invalid snapshot magic: 0x%X\n", header.Magic);
        return FALSE;
    }

    fprintf(stdout, "[Config] Rolled back to snapshot %lu\n", SnapshotId);
    return TRUE;
}

// === Список снапшотов ===
BOOLEAN ListSnapshots(PULONG pCount, PULONG pIds) {
    if (!pCount) return FALSE;

    *pCount = 0;
    ULONG count = 0;

    WCHAR searchPath[MAX_PATH] = { 0 };
    swprintf_s(searchPath, MAX_PATH, L"%s\\*.dat", SNAPSHOT_DIR);

    WIN32_FIND_DATAW findData = { 0 };  // <-- ИСПРАВЛЕНО: WIN32_FIND_DATAW
    HANDLE hFind = FindFirstFileW(searchPath, &findData);  // <-- ИСПРАВЛЕНО

    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stdout, "[Config] No snapshots found\n");
        return TRUE;
    }

    do {
        if (count < MAX_SNAPSHOTS && pIds) {
            // Извлекаем ID из имени файла
            ULONG id = 0;
            swscanf_s(findData.cFileName, L"snapshot_%lu.dat", &id);
            pIds[count] = id;
        }
        count++;
    } while (FindNextFileW(hFind, &findData));  // <-- ИСПРАВЛЕНО

    FindClose(hFind);
    *pCount = count;

    fprintf(stdout, "[Config] Found %lu snapshots\n", count);
    return TRUE;
}

// === Удаление снапшота ===
BOOLEAN DeleteSnapshot(ULONG SnapshotId) {
    WCHAR filePath[MAX_PATH] = { 0 };
    swprintf_s(filePath, MAX_PATH, L"%s\\snapshot_%lu.dat", SNAPSHOT_DIR, SnapshotId);

    if (!DeleteFileW(filePath)) {  // <-- ИСПРАВЛЕНО
        fprintf(stderr, "[Config] Failed to delete snapshot %lu: %lu\n", SnapshotId, GetLastError());
        return FALSE;
    }

    fprintf(stdout, "[Config] Snapshot %lu deleted\n", SnapshotId);
    return TRUE;
}