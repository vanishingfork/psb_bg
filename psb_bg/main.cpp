#include <Windows.h>
#include <iostream>
#include <string>
#include <stdint.h>

#include "driver.h"

#define IOCTL_READ_MSR 0x222848
#define IOCTL_PCI_WRITE_DWORD 0x222844
#define IOCTL_PCI_READ_DWORD 0x222840

HANDLE load_driver()
{
    // Get temp path
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring driverPath = std::wstring(tempPath) + L"RwDrv.sys";

    // Write driver to temp - Use wide-char printing
    wprintf(L"Temp path: %ls\n", tempPath);
    wprintf(L"Full driver path: %ls\n", driverPath.c_str());
    
    HANDLE hFile = CreateFileW(driverPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        wprintf(L"Failed to write file. Error code: %lu\n", error);
        return INVALID_HANDLE_VALUE;
    }
    DWORD written = 0;
    WriteFile(hFile, driver, sizeof(driver), &written, nullptr);
    CloseHandle(hFile);

    // 2) Register service + start
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        DeleteFileW(driverPath.c_str());
        printf("INVALID HANDLE VALUE\n");
        return INVALID_HANDLE_VALUE;
    }
    SC_HANDLE hService = CreateServiceW(
        hSCM,
        L"RwDrv",
        L"RwDrv",
        SERVICE_START | DELETE | SERVICE_STOP,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!hService) {
        CloseServiceHandle(hSCM);
        DeleteFileW(driverPath.c_str());
        printf("INVALID HANDLE VALUE\n");
        return INVALID_HANDLE_VALUE;
    }

    if (!StartServiceW(hService, 0, nullptr)) {
        DeleteService(hService);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        DeleteFileW(driverPath.c_str());
        printf("INVALID HANDLE VALUE\n");
        return INVALID_HANDLE_VALUE;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    // 3) Delete driver file
    DeleteFileW(driverPath.c_str());
    printf("Success\n");
    return 0;
}

HANDLE open_handle() 
{
    HANDLE hDevice = CreateFileW(
        L"\\\\.\\RwDrv",                    // Device name 
        GENERIC_READ | GENERIC_WRITE,        // Access mode
        0,                                   // Share mode (exclusive)
        NULL,                               // Security attributes
        OPEN_EXISTING,                      // Creation disposition
        FILE_ATTRIBUTE_NORMAL,              // Flags and attributes
        NULL                                // Template file
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    return hDevice;
}

DWORD unload_driver() {
    //unload rwdrv.sys
    return NULL;
}

DWORD intel_bootguard_check() {
    //if bootguard, return 1, else return 0
    return NULL;
}

DWORD amd_psb_check() {
    //if psb, return 1, else return 0
    return NULL;
}

DWORD read_msr(DWORD msr_num) {
    return NULL;
}

DWORD pci_write_dword(DWORD bus, DWORD dev, DWORD fun, DWORD offest, DWORD value) {
    return NULL;
}

DWORD pci_read_dword(DWORD bus, DWORD dev, DWORD fun, DWORD offset) {
    return NULL;
}


int get_cpu_arch() {
    return NULL;
}

int main() {
    printf("output: %d\n", load_driver());
    HANDLE rwdrv_handle = open_handle();
    if (rwdrv_handle != INVALID_HANDLE_VALUE) {
        printf("HANDLE: 0x%zx\n", (intptr_t)rwdrv_handle);
    }
    printf("Press any key to exit...\n");
    fflush(stdin);  // Clear input buffer
    getchar();      // Wait for keypress
    return 0;
}