#include <ntddk.h>

extern "C" DRIVER_INITIALIZE DriverEntry;

// Declines to load until feature 006 registers the miniport with DxgkInitialize.
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)
{
    return STATUS_NOT_SUPPORTED;
}
