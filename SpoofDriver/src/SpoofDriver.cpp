#include <ntifs.h>
#include <wdm.h>
#include "SpoofDriver.h"

// === Глобальные структуры драйвера ===
DRIVER_OBJECT gDriverObject;
UNICODE_STRING DriverName;
DEVICE_EXTENSION gDeviceExtension;

HOOK_INFO gHooks[MAX_HOOKS];
BOOLEAN gIsSpoofingActive = FALSE;
ULONG gSpoofFlags = 0x1F; // Биты: 0=SSDT, 1=Process, 2=Memory, 3=Time, 4=Network

// === Функции-обёртки для хуков ===
NTSTATUS HookNtQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
) {
    // Пример: обход тайминг-анализа EAC
    if (gIsSpoofingActive &&
        SystemInformationClass == SystemInterruptTimeInformation) {

        PULONG time = (PULONG)SystemInformation;
        if (time) {
            *time += 0x12345678ULL; // Сдвиг для обхода детекции модификаций времени
        }
    }

    return gOriginalNtQuerySystemInformation(
        SystemInformationClass,
        SystemInformation,
        SystemInformationLength,
        ReturnLength
    );
}

NTSTATUS HookRtlCreateProcess(
    IN PUNICODE_STRING ImagePathName,
    IN PUNICODE_STRING CommandLine,
    IN PUNICODE_STRING CurrentDirectory,
    IN PUNICODE_STRING DllPath,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG Attributes,
    IN HANDLE ProcessHandle,
    IN HANDLE ParentProcess,
    IN BOOLEAN InheritHandles,
    IN HANDLE DebugPort,
    IN HANDLE ExceptionPort,
    IN BOOLEAN CreateReserve
) {
    // Пример: перехват запуска DayZ для модификации аргументов
    if (gIsSpoofingActive &&
        ImagePathName->Buffer &&
        wcsstr(ImagePathName->Buffer, DAYZ_PROCESS_NAME)) {

        DbgPrint("[Spoof] DayZ process detected: %wZ\n", ImagePathName);
    }

    return gOriginalRtlCreateProcess(
        ImagePathName, CommandLine, CurrentDirectory, DllPath,
        SecurityDescriptor, Attributes, ProcessHandle, ParentProcess,
        InheritHandles, DebugPort, ExceptionPort, CreateReserve
    );
}

// === SSDT хуки (инициализация) ===
NTSTATUS InitializeSSDTHooks() {
    NTSTATUS status = STATUS_SUCCESS;

    // Хук 1: NtQuerySystemInformation
    if (!gOriginalNtQuerySystemInformation) {
        PULONG ssdt = (PULONG)MmGetSystemRoutineAddress(
            &PsLookupProcessByProcessId);

        gOriginalNtQuerySystemInformation =
            (PfnNtQuerySystemInformation)*ssdt;

        *ssdt = (PULONG)HookNtQuerySystemInformation;
        DbgPrint("[Spoof] SSDT hook 1 installed\n");
    }

    // Хук 2: RtlCreateProcess (для перехвата запуска процессов)
    if (!gOriginalRtlCreateProcess) {
        PULONG ssdt = (PULONG)MmGetSystemRoutineAddress(
            &PsLookupProcessByProcessId);

        gOriginalRtlCreateProcess =
            (PfnRtlCreateProcess)*ssdt;

        *ssdt = (PULONG)HookRtlCreateProcess;
        DbgPrint("[Spoof] SSDT hook 2 installed\n");
    }

    return status;
}

// === Перехват системных вызовов через IDT (дополнительный уровень) ===
NTSTATUS HookSystemCall(
    IN PULONG_PTR SystemCallTable,
    IN ULONG Index,
    IN PVOID NewHandler
) {
    if (Index >= 256 || !SystemCallTable) {
        return STATUS_INVALID_PARAMETER;
    }

    // Сохраняем оригинальный обработчик
    gHooks[Index].HookAddress = SystemCallTable + Index;
    for (ULONG i = 0; i < 5; i++) {
        gHooks[Index].OriginalOpcode[i] =
            ((PULONG)SystemCallTable)[Index * 4 + i];
    }

    // Записываем новый обработчик (x64: CALL [addr])
    PULONG handlerPtr = (PULONG)(SystemCallTable + Index);
    *handlerPtr = (ULONG_PTR)NewHandler;

    gHooks[Index].IsHooked = TRUE;
    DbgPrint("[Spoof] System call %d hooked\n", Index);

    return STATUS_SUCCESS;
}

// === Поиск и модификация памяти процесса DayZ ===
NTSTATUS FindAndModifyDayZMemory(HANDLE hProcess) {
    if (!hProcess || !IsProcessValid(hProcess)) {
        return STATUS_INVALID_HANDLE;
    }

    // Пример: поиск сигнатуры EAC в памяти
    ULONG pattern[] = { 0x48, 0x89, 0xE5, 0x41, 0xFF, 0xC2 };
    SIZE_T bytesRead = 0;
    PVOID baseAddress = NULL;

    if (NtQueryVirtualMemory(
        hProcess,
        NULL,
        VirtualMemoryBasicInformation,
        &baseAddress,
        &bytesRead,
        NULL) == STATUS_SUCCESS && baseAddress) {

        DbgPrint("[Spoof] DayZ memory space scanned\n");
    }

    return STATUS_SUCCESS;
}

BOOLEAN IsProcessValid(HANDLE hProcess) {
    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG resultSize = sizeof(pbi);

    if (NtQueryInformationProcess(
        hProcess,
        ProcessBasicInformation,
        &pbi,
        sizeof(pbi),
        &resultSize) == STATUS_SUCCESS) {

        return pbi.InheritedFromUniqueProcessId != NULL;
    }

    return FALSE;
}

// === Управление спуфингом ===
VOID StartSpoofing() {
    if (gIsSpoofingActive) return;

    gIsSpoofingActive = TRUE;
    DbgPrint("[Spoof] Spoofing activated with flags: 0x%X\n", gSpoofFlags);
}

VOID StopSpoofing() {
    gIsSpoofingActive = FALSE;
    DbgPrint("[Spoof] Spoofing stopped\n");
}

// === IOCTL обработчики устройства ===
NTSTATUS DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;

    switch (controlCode) {
    case IOCTL_START_SPOOFING:
        StartSpoofing();
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IOCTL_STOP_SPOOFING:
        StopSpoofing();
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IOCTL_GET_STATUS: {
        ULONG statusBuffer[4] = { 0 };
        statusBuffer[0] = gIsSpoofingActive ? 1 : 0;
        statusBuffer[1] = gSpoofFlags;
        statusBuffer[2] = sizeof(statusBuffer);

        IoCopyData(Irp, &statusBuffer, sizeof(statusBuffer));
        Irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    }

    default:
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchIRP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    switch (stack->MajorFunction) {
    case IRP_MJ_DEVICE_CONTROL:
        return DeviceControl(DeviceObject, Irp);

    case IRP_MJ_CREATE:
        DbgPrint("[Spoof] Process opened device\n");
        break;

    case IRP_MJ_CLOSE:
        DbgPrint("[Spoof] Process closed device\n");
        break;

    default:
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// === Вспомогательные функции ===
VOID DbgPrint(
    IN PCSTR FormatString,
    ...
) {
    va_list args;
    va_start(args, FormatString);

    CHAR buffer[1024];
    vsnprintf(buffer, sizeof(buffer), FormatString, args);

    DbgPrint("%s", buffer);
    va_end(args);
}

// === Инъекция DLL в процесс (дополнительно) ===
NTSTATUS InjectDLLIntoProcess(
    IN HANDLE hProcess,
    IN PUNICODE_STRING dllPath
) {
    if (!hProcess || !dllPath) {
        return STATUS_INVALID_PARAMETER;
    }

    // Открыть файл DLL
    HANDLE hFile = CreateFile(
        dllPath->Buffer,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return STATUS_FILE_NOT_FOUND;
    }

    // Получить размер DLL
    LARGE_INTEGER fileSize = { 0 };
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return STATUS_UNSUCCESSFUL;
    }

    // Выделить память в процессе
    PVOID baseAddress = VirtualAllocEx(
        hProcess,
        NULL,
        fileSize.LowPart,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!baseAddress) {
        CloseHandle(hFile);
        return STATUS_NO_MEMORY;
    }

    // Записать данные DLL в процесс
    if (!WriteProcessMemory(
        hProcess,
        baseAddress,
        &hFile,
        sizeof(HANDLE),
        NULL)) {

        VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return STATUS_UNSUCCESSFUL;
    }

    // Запустить LoadLibraryA через CreateRemoteThread
    HANDLE hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW"),
        baseAddress,
        0,
        NULL
    );

    if (!hThread) {
        VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return STATUS_UNSUCCESSFUL;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);
    CloseHandle(hFile);

    return STATUS_SUCCESS;
}

// === Финальная очистка при удалении драйвера ===
VOID CleanupDriver() {
    DbgPrint("[Spoof] SpoofDriver.sys unloaded\n");
    gIsSpoofingActive = FALSE;
}

// === Основной цикл драйвера ===
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
) {
    NTSTATUS status = STATUS_SUCCESS;

    // Инициализация
    RtlInitUnicodeString(&DriverName, DRIVER_NAME);
    gDeviceExtension.DeviceObject = DriverObject;

    DbgPrint("[Spoof] SpoofDriver.sys loaded\n");

    // Создание устройства
    status = CmCreateSymbolicLink(
        &SYMLINK_NAME,
        &DEVICE_NAME
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[Spoof] Failed to create symbolic link: 0x%X\n", status);
        return status;
    }

    // Установка SSDT хуков
    InitializeSSDTHooks();

    // Инициализация флагов
    gIsSpoofingActive = FALSE;
    gSpoofFlags = 0x1F; // Все функции активны

    DbgPrint("[Spoof] Driver initialized successfully\n");

    return status;
}
