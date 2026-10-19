#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <ntstatus.h>  // <-- ДОБАВЛЕНО: для NT_SUCCESS
#include "HostTypes.h"
#include "HostAPI.h"
#include "CLITypes.h"
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

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
    NTSTATUS ntStatus = GetSpoofStatus(g_hDevice, &status);
    g_IsSpoofingActive = status.IsSpoofingActive;

    fprintf(stdout, "\n"
        "╔════════════════════════════════════════════╗\n"
        "║     SPOOF DRIVER TERMINAL v1.0              ║\n"
        "╚════════════════════════════════════════════╝\n");
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
    fprintf(stdout, "\n"
        "╔════════════════════════════════════════════╗\n"
        "║     SPOOF DRIVER TERMINAL v1.0              \n"
        "╚════════════════════════════════════════════╝\n");
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
        if (NT_SUCCESS(status)) {
            g_IsSpoofingActive = TRUE;
            fprintf(stdout, "[SUCCESS] Spoofing started\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to start spoofing: 0x%X\n", status);
        }
        break;

    case 2: // Stop Spoofing
        if (!g_IsSpoofingActive) {
            fprintf(stderr, "[ERROR] Not active\n");
            return 0;
        }
        status = StopSpoofing(g_hDevice);
        if (NT_SUCCESS(status)) {
            g_IsSpoofingActive = FALSE;
            fprintf(stdout, "[SUCCESS] Spoofing stopped\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to stop spoofing: 0x%X\n", status);
        }
        break;

    case 3: // Get Status
    {
        SPOOF_STATUS currentStatus = { 0 };
        status = GetSpoofStatus(g_hDevice, &currentStatus);
        if (NT_SUCCESS(status)) {
            fprintf(stdout, "\n[STATUS]\n");
            fprintf(stdout, "  Active: %s\n",
                currentStatus.IsSpoofingActive ? "YES" : "NO");
            fprintf(stdout, "  Flags: 0x%X\n", currentStatus.SpoofFlags);
            fprintf(stdout, "  Snapshots: %lu\n", currentStatus.SnapshotCount);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to get status: 0x%X\n", status);
        }
        break;
    }

    case 4: // Create Snapshot
    {
        ULONG snapshotId = 0;
        status = CreateSnapshot(g_hDevice, &snapshotId);
        if (NT_SUCCESS(status)) {
            fprintf(stdout, "[SUCCESS] Snapshot created: ID %lu\n", snapshotId);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to create snapshot: 0x%X\n", status);
        }
        break;
    }

    case 5: // Rollback Snapshot
    {
        ULONG rollbackId = 0;
        fprintf(stdout, "\nEnter snapshot ID to rollback: ");
        scanf("%lu", &rollbackId);
        status = RollbackSnapshot(g_hDevice, rollbackId);
        if (NT_SUCCESS(status)) {
            fprintf(stdout, "[SUCCESS] Rolled back to snapshot %lu\n", rollbackId);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to rollback: 0x%X\n", status);
        }
        break;
    }

    case 6: // List Snapshots
    {
        ULONG count = 0;
        ULONG ids[16] = { 0 };  // <-- ИСПРАВЛЕНО: используем массив вместо указателя
        status = ListSnapshots(g_hDevice, &count, ids);

        if (NT_SUCCESS(status) && count > 0) {
            fprintf(stdout, "\n[SNAPSHOTS]\n");
            for (ULONG i = 0; i < count && i < 16; i++) {
                fprintf(stdout, "  %lu. Snapshot ID: %lu\n", i + 1, ids[i]);
            }
        }
        else if (count == 0) {
            fprintf(stdout, "[INFO] No snapshots found\n");
        }
        else {
            fprintf(stderr, "[ERROR] Failed to list snapshots: 0x%X\n", status);
        }
        break;
    }

    case 7: // Set Spoof Flags
    {
        ULONG flags = 0;
        fprintf(stdout, "\nEnter spoof flags (hex): ");
        scanf("%lx", &flags);
        status = SetSpoofFlags(g_hDevice, flags);
        if (NT_SUCCESS(status)) {
            fprintf(stdout, "[SUCCESS] Spoof flags set to: 0x%X\n", flags);
        }
        else {
            fprintf(stderr, "[ERROR] Failed to set flags: 0x%X\n", status);
        }
        break;
    }

    case 8: // Exit
        fprintf(stdout, "\n[INFO] Exiting CLI...\n");
        return 1;

    default:
        fprintf(stderr, "[ERROR] Invalid command\n");
        break;
    }

    return 0;
}

// === Основная функция CLI ===
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    InitCLI();

    if (g_hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[CLI] Failed to initialize. Exiting.\n");
        return 1;
    }

    while (1) {
        DrawMenu();

        INT cmd = _getch();

        if (cmd >= '1' && cmd <= '8') {
            INT result = HandleCommand(cmd - '0');
            if (result == 1) {
                break;
            }
            fprintf(stdout, "\nPress any key to continue...");
            _getch();
        }
        else if (cmd == 27) {
            fprintf(stdout, "\n[INFO] Press Enter to exit...");
            _getch();
            break;
        }
    }

    CleanupCLI();
    return 0;
}