// ВАЖНО: Этот файл требует Windows Driver Kit (WDK) для компиляции!
// Без WDK он не соберется. Используйте Visual Studio с компонентом "Windows Driver Kit".

#include <ntifs.h>
#include <wdm.h>
#include "SpoofDriver.h"
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

// Если нужно #pragma warning(disable: 4005)  // Подавить предупреждения о макросах

// === Глобальные структуры драйвера ===
PDEVICE_OBJECT g_DeviceObject = NULL;
UNICODE_STRING g_SymLinkName;
BOOLEAN gIsSpoofingActive = FALSE;
ULONG gSpoofFlags = 0x1F;

// === IOCTL обработчики устройства ===
NTSTATUS DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_SUCCESS;

    switch (controlCode) {
    case IOCTL_START_SPOOFING:
        gIsSpoofingActive = TRUE;
        DbgPrint("[Spoof] Spoofing activated with flags: 0x%X\n", gSpoofFlags);
        break;

    case IOCTL_STOP_SPOOFING:
        gIsSpoofingActive = FALSE;
        DbgPrint("[Spoof] Spoofing stopped\n");
        break;

    case IOCTL_GET_STATUS: {
        if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG)) {
            PULONG statusBuffer = (PULONG)Irp->AssociatedIrp.SystemBuffer;
            statusBuffer[0] = gIsSpoofingActive ? 1 : 0;
            statusBuffer[1] = gSpoofFlags;
            Irp->IoStatus.Information = 2 * sizeof(ULONG);
        }
        else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;
    }

    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DispatchIRP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
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
        break;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// === Очистка при удалении драйвера ===
VOID UnloadDriver(IN PDRIVER_OBJECT DriverObject)
{
    DbgPrint("[Spoof] SpoofDriver.sys unloading\n");

    // Удаляем символическую ссылку
    if (g_SymLinkName.Buffer != NULL) {
        IoDeleteSymbolicLink(&g_SymLinkName);
        RtlFreeUnicodeString(&g_SymLinkName);
    }

    // Удаляем устройство
    if (DriverObject->DeviceObject != NULL) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    gIsSpoofingActive = FALSE;
    DbgPrint("[Spoof] Driver unloaded successfully\n");
}

// === Основной цикл драйвера ===
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[Spoof] SpoofDriver.sys loading...\n");

    // Инициализация имени устройства
    RtlInitUnicodeString(&deviceName, L"\\Device\\SpoofDriver");
    RtlInitUnicodeString(&g_SymLinkName, L"\\DosDevices\\SpoofLink");

    // Создание устройства
    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_DeviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[Spoof] Failed to create device: 0x%X\n", status);
        return status;
    }

    // Создание символической ссылки
    status = IoCreateSymbolicLink(&g_SymLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[Spoof] Failed to create symbolic link: 0x%X\n", status);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }

    // Установка обработчиков IRP
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchIRP;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchIRP;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    DriverObject->DriverUnload = UnloadDriver;

    // Инициализация флагов
    gIsSpoofingActive = FALSE;
    gSpoofFlags = 0x1F;

    DbgPrint("[Spoof] Driver initialized successfully\n");
    return STATUS_SUCCESS;
}