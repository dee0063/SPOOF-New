#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "HostTypes.h"
#include "HostAPI.h"
#include "CLITypes.h"

// === Глобальные переменные ===
HANDLE g_hDevice = INVALID_HANDLE_VALUE;
BOOLEAN g_IsSpoofingActive = FALSE;

// === Инициализация CLI ===
VOID InitCLI() {
    g_hDevice = OpenSpoofDevice();
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[CLI] Failed to open device\n");
        return;
    }

    SPOOF_STATUS status = { 0 };
    GetSpoofStatus(g_hDevice, &status);
    g_IsSpoofingActive = status.IsSpoofingActive;

    fprintf(stdout, "\n"
        "╔════════════════════════════════════════════╗\n"
        "║     SPOOF DRIVER TERMINAL v1.0              ║\n"
        "╚════════════════════════════════════════════╝\n\n");

    fprintf(stdout, "[INFO] Device opened: SpoofLink\n");
    fprintf(stdout, "[INFO] Current status: %s\n",
        g_IsSpoofingActive ? "ACTIVE" : "INACTIVE");
}

// === Очистка CLI ===
VOID CleanupCLI() {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseSpoofDevice(g_hDevice);
    }
}

// === Отрисовка меню ===
VOID DrawMenu() {
    system("cls");

    fprintf(stdout, "\n╔════════════════════════════════════════════╗\n"
        "║     SPOOF DRIVER TERMINAL v1.0              ║\n"
        "╚════════════════════════════════════════════╝\n\n");

    fprintf(stdout, "[STATUS] Spoofing: %s\n",
        g_IsSpoofingActive ? "ACTIVE" : "INACTIVE");

    fprintf(stdout, "\n───────────────────────────────────────────────\n");
    fprintf(stdout, "  1. Start Spoofing\n");
    fprintf(stdout, "  2. Stop Spoofing\n");
    fprintf(stdout, "  3. Get Status\n");
    fprintf(stdout, "  4. Create Snapshot\n");
    fprintf(stdout, "  5. Rollback Snapshot\n");
    fprintf(stdout, "  6. List Snapshots\n");
    fprintf(stdout, "  7. Set Spoof Flags\n");
    fprintf(stdout, "  8. Exit\n");
    fprintf(stdout, "───────────────────────────────────────────────\n");

    fprintf(stdout, "\nEnter command: ");
}

// === Обработка команд ===
INT HandleCommand(INT cmd) {
    switch (cmd) {
    case 1: // Start Spoofing
        if (g_IsSpoofingActive) {
            fprintf(stderr, "[ERROR] Already active\n");
            return 0;
        }

        NTSTATUS status = StartSpoofing(g_hDevice);
        if (status == STATUS_SUCCESS) {
            g_IsSpoofingActive = TRUE;
            fprintf(stdout, "[SUCCESS] Spoofing started\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to start spoofing: %lu\n", GetLastError());
        }
        break;

    case 2: // Stop Spoofing
        if (!g_IsSpoofingActive) {
            fprintf(stderr, "[ERROR] Not active\n");
            return 0;
        }

        status = StopSpoofing(g_hDevice);
        if (status == STATUS_SUCCESS) {
            g_IsSpoofingActive = FALSE;
            fprintf(stdout, "[SUCCESS] Spoofing stopped\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to stop spoofing: %lu\n", GetLastError());
        }
        break;

    case 3: // Get Status
        SPOOF_STATUS currentStatus = { 0 };
        status = GetSpoofStatus(g_hDevice, &currentStatus);

        if (status == STATUS_SUCCESS) {
            fprintf(stdout, "\n[STATUS]\n");
            fprintf(stdout, "  Active: %s\n",
                currentStatus.IsSpoofingActive ? "YES" : "NO");
            fprintf(stdout, "  Flags: 0x%X\n", currentStatus.SpoofFlags);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to get status\n");
        }
        break;

    case 4: // Create Snapshot
        ULONG snapshotId = 0;
        status = CreateSnapshot(g_hDevice, &snapshotId);

        if (status == STATUS_SUCCESS) {
            fprintf(stdout, "[SUCCESS] Snapshot created: ID %lu\n", snapshotId);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to create snapshot: %lu\n", GetLastError());
        }
        break;

    case 5: // Rollback Snapshot
        ULONG rollbackId = 0;
        fprintf(stdout, "\nEnter snapshot ID to rollback: ");
        scanf("%lu", &rollbackId);

        status = RollbackSnapshot(g_hDevice, rollbackId);

        if (status == STATUS_SUCCESS) {
            fprintf(stdout, "[SUCCESS] Rolled back to snapshot %lu\n", rollbackId);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to rollback: %lu\n", GetLastError());
        }
        break;

    case 6: // List Snapshots
        ULONG count = 0;
        ULONG* ids = NULL;

        status = ListSnapshots(g_hDevice, &count, &ids);

        if (status == STATUS_SUCCESS && count > 0) {
            fprintf(stdout, "\n[SNAPSHOTS]\n");
            for (ULONG i = 0; i < count; i++) {
                fprintf(stdout, "  %lu. Snapshot ID: %lu\n",
                    i + 1, ids[i]);
            }
        }
        else if (count == 0) {
            fprintf(stdout, "[INFO] No snapshots found\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to list snapshots\n");
        }

        if (ids) free(ids);
        break;

    case 7: // Set Spoof Flags
        ULONG flags = 0;
        fprintf(stdout, "\nEnter spoof flags (hex): ");
        scanf("%lx", &flags);

        status = SetSpoofFlags(g_hDevice, flags);

        if (status == STATUS_SUCCESS) {
            fprintf(stdout, "[SUCCESS] Spoof flags set to: 0x%X\n", flags);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to set flags: %lu\n", GetLastError());
        }
        break;

    case 8: // Exit
        fprintf(stdout, "\n[INFO] Exiting CLI...\n");
        return 1;

    default:
        fprintf(stderr, "[ERROR] Invalid command\n");
    }

    return 0;
}

// === Основной цикл CLI ===
INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nShowCmd) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    InitCLI();

    while (1) {
        DrawMenu();

        INT cmd = _getch();

        if (cmd >= '1' && cmd <= '8') {
            HandleCommand(cmd - '0');
        }
        else if (cmd == 27) { // ESC
            fprintf(stdout, "\n[INFO] Press Enter to continue...\n");
            _getch();
        }
        else if (cmd == '\r' || cmd == '\n') {
            DrawMenu();
        }
    }

    CleanupCLI();
    return 0;
}
