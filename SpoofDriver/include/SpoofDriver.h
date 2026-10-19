#pragma once

#include <ntifs.h>
#include <wdm.h>

#define DRIVER_NAME L"\\SystemRoot\\System32\\drivers\\SpoofDriver.sys"
#define DEVICE_NAME L"\\Device\\SpoofDevice"
#define SYMLINK_NAME L"\\DosDevices\\SpoofLink"

// === Константы и флаги ===
#define MAX_HOOKS 64
#define DAYZ_PROCESS_NAME L"S3D.exe"
#define EAC_MODULE_NAME L"EACClient.dll"

typedef struct _HOOK_INFO {
    PULONG_PTR HookAddress;
    ULONG OriginalOpcode[5];
    BOOLEAN IsHooked;
} HOOK_INFO, * PHOOK_INFO;

// === Глобальные структуры драйвера ===
extern DRIVER_OBJECT gDriverObject;
extern UNICODE_STRING DriverName;
extern DEVICE_EXTENSION gDeviceExtension;

extern HOOK_INFO gHooks[MAX_HOOKS];
extern BOOLEAN gIsSpoofingActive;
extern ULONG gSpoofFlags;

// === Типы для хуков ===
typedef NTSTATUS(NTAPI* PfnNtQuerySystemInformation)(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    );

typedef NTSTATUS(NTAPI* PfnRtlCreateProcess)(
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
    );

// === Функции-обёртки для хуков ===
NTSTATUS HookNtQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
);

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
);

// === SSDT хуки (инициализация) ===
NTSTATUS InitializeSSDTHooks();

// === Перехват системных вызовов через IDT ===
NTSTATUS HookSystemCall(
    IN PULONG_PTR SystemCallTable,
    IN ULONG Index,
    IN PVOID NewHandler
);

// === Поиск и модификация памяти процесса DayZ ===
NTSTATUS FindAndModifyDayZMemory(HANDLE hProcess);
BOOLEAN IsProcessValid(HANDLE hProcess);

// === Управление спуфингом ===
VOID StartSpoofing();
VOID StopSpoofing();

// === IOCTL обработчики устройства ===
NTSTATUS DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
);

NTSTATUS DispatchIRP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
);

// === Вспомогательные функции ===
VOID DbgPrint(
    IN PCSTR FormatString,
    ...
);

BOOLEAN IsProcessValid(HANDLE hProcess);

// === Инъекция DLL в процесс (дополнительно) ===
NTSTATUS InjectDLLIntoProcess(
    IN HANDLE hProcess,
    IN PUNICODE_STRING dllPath
);

// === Финальная очистка при удалении драйвера ===
VOID CleanupDriver();
