#include <Windows.h>
#include <iostream>
#include <string>
#include <stdint.h>
#include <intrin.h>

#include "driver.h"

// RwDrv IOCTLs
#define IOCTL_READ_MSR 0x222848
#define IOCTL_PCI_WRITE_DWORD 0x222844
#define IOCTL_PCI_READ_DWORD 0x222840

// Intel
#define BG_MSR 0x13A // Intel Boot Guard MSR

// AMD
// SYSTEM MANAGEMENT UNIT (SMU) Registers
#define SMU_ADDR_OFFSET 0xB8
#define SMU_DATA_OFFSET 0xBC

// SYSTEM MANAGEMENT NETWORK (SMN) Registers
#define SMN_PUBLIC_BASE 0x3800000
#define PSB_STATUS_OFFSET 0x10994

// PCI Root Complex 
#define pci_rt_bus 0
#define pci_rt_dev 0
#define pci_rt_fun 0

// GLOBAL handle for driver, fight me.
HANDLE RwDrv_handle;

int load_driver()
{
    // Get temp path
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string driverPath = std::string(tempPath) + "RwDrv.sys";
    //printf("Driver path: %s\n", driverPath.c_str());

    HANDLE hFile = CreateFileA(driverPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        printf("Failed to write driver to disk. Error code: %lu\n", error);
        return error;
    }

    DWORD written = 0;
    WriteFile(hFile, driver, sizeof(driver), &written, nullptr);
    //printf("Driver bytes written to disk: %u\n", written);
    CloseHandle(hFile);

    // 2) Register service + start
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        DeleteFileA(driverPath.c_str());
        printf("Failed to open SC Manager\n");
        return 1;
    }
    SC_HANDLE hService = CreateServiceA(
        hSCM,
        "RwDrv",
        "RwDrv",
        SERVICE_START | DELETE | SERVICE_STOP,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!hService) {
        DWORD error = GetLastError();
        printf("Failed to create service. Error code: %lu\n", error);
        CloseServiceHandle(hSCM);
        return 1;
    }

    if (!StartServiceA(hService, 0, nullptr)) {
        DWORD error = GetLastError();
        printf("Failed to start service. Error code: %lu\n", error);
        DeleteService(hService);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return 1;
    }


    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return 0;
}

HANDLE open_handle()
{
    HANDLE hDevice = CreateFileA(
        "\\\\.\\RwDrv",                     // Device name 
        GENERIC_READ | GENERIC_WRITE,        // Access mode
        0,                                   // Share mode (exclusive)
        NULL,                                // Security attributes
        OPEN_EXISTING,                       // Creation disposition
        FILE_ATTRIBUTE_NORMAL,               // Flags and attributes
        NULL                                 // Template file
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    return hDevice;
}

DWORD unload_driver() {
    SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        DWORD error = GetLastError();
        wprintf(L"OpenSCManagerA failed. Error code: %lu\n", error);
        return error;
    }

    SC_HANDLE hService = OpenServiceA(hSCM, "RwDrv", SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        wprintf(L"OpenServiceA failed. Error code: %lu\n", error);
        CloseServiceHandle(hSCM);
        return error;
    }

    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
        DWORD error = GetLastError();
        wprintf(L"QueryServiceStatusEx failed. Error code: %lu\n", error);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return error;
    }

    if (ssp.dwCurrentState != SERVICE_STOPPED) {
        if (!ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
            DWORD error = GetLastError();
            if (error != ERROR_SERVICE_NOT_ACTIVE) {
                printf("ControlService failed. Error code: %lu\n", error);
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCM);
                return error;
            }
        }

        // Wait for the service to stop
        const int maxWaitTime = 30000; // 30 seconds
        const int waitInterval = 1000; // 1 second
        int waitedTime = 0;
        while (ssp.dwCurrentState != SERVICE_STOPPED && waitedTime < maxWaitTime) {
            Sleep(waitInterval);
            waitedTime += waitInterval;
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
                DWORD error = GetLastError();
                printf("QueryServiceStatusEx failed. Error code: %lu\n", error);
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCM);
                return error;
            }
        }

        if (ssp.dwCurrentState != SERVICE_STOPPED) {
            printf("Service did not stop within the expected time. Perhaps there is a dangling handle.\n");
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return ERROR_SERVICE_REQUEST_TIMEOUT;
        }
    }

    if (!DeleteService(hService)) {
        DWORD error = GetLastError();
        printf("DeleteService failed. Error code: %lu\n", error);
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return error;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    //wprintf(L"Driver unloaded successfully.\n");
    return 0;
}

DWORD read_msr(DWORD msr_address) {
    // Prepare input buffer: 16 bytes (4 uint32_t)
    // iobuf[2] = msr_address, iobuf[3] is upper 32 bits upon return, iobuf[0] is lower 32 bits
    uint32_t iobuf[4] = { 0, 0, msr_address, 0 };
    DWORD bytes_returned = 0;

    BOOL success = DeviceIoControl(
        RwDrv_handle,
        IOCTL_READ_MSR,
        &iobuf,
        sizeof(iobuf),
        &iobuf,
        sizeof(iobuf),
        &bytes_returned,
        nullptr
    );
    return (static_cast<uint64_t>(iobuf[3]) << 32) | iobuf[0];
}

DWORD pci_write_dword(DWORD bus, DWORD dev, DWORD fun, DWORD reg, DWORD value) {
    BYTE iobuf[12] = { 0 };
    iobuf[0] = bus & 0xFF;
    iobuf[1] = dev & 0xFF;
    iobuf[2] = fun & 0xFF;
    iobuf[3] = 0; // reserved
    iobuf[4] = (reg >> 0) & 0xFF;
    iobuf[5] = (reg >> 8) & 0xFF;
    iobuf[6] = (value >> 0) & 0xFF;
    iobuf[7] = (value >> 8) & 0xFF;
    iobuf[8] = (value >> 16) & 0xFF;
    iobuf[9] = (value >> 24) & 0xFF;

    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        RwDrv_handle,
        IOCTL_PCI_READ_DWORD,
        &iobuf,
        sizeof(iobuf),
        &iobuf,
        sizeof(iobuf),
        &bytes_returned,
        nullptr
    );

    DWORD val;
    memcpy(&val, &iobuf[8], sizeof(DWORD));
    return val;
}

DWORD pci_read_dword(DWORD bus, DWORD dev, DWORD fun, DWORD reg) {
    BYTE iobuf[12] = { 0 };
    iobuf[0] = bus & 0xFF;
    iobuf[1] = dev & 0xFF;
    iobuf[2] = fun & 0xFF;
    iobuf[3] = 0; // reserved
    iobuf[4] = (reg >> 0) & 0xFF;
    iobuf[5] = (reg >> 8) & 0xFF;

    DWORD bytes_returned = 0;
    BOOL success = DeviceIoControl(
        RwDrv_handle,
        IOCTL_PCI_READ_DWORD,
        &iobuf,
        sizeof(iobuf),
        &iobuf,
        sizeof(iobuf),
        &bytes_returned,
        nullptr
    );

    DWORD val;
    memcpy(&val, &iobuf[8], sizeof(DWORD));
    return val;
}

VOID get_cpu_vendor(char* vendor) {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    memcpy(vendor + 0, &cpuInfo[1], 4); // EBX
    memcpy(vendor + 8, &cpuInfo[2], 4); // ECX lovely quirks of x86 cpuid
    memcpy(vendor + 4, &cpuInfo[3], 4); // EDX
    vendor[12] = '\0';
}

VOID intel_bootguard_check() {
    printf("Intel CPU detected\n");
    DWORD msr_val = read_msr(BG_MSR);  // MSR address -> BootGuard
    printf("BootGuard MSR: 0x%08X\n", msr_val);
    DWORD mask = msr_val & 0x30000000;
    if (mask == 0x0) {
        printf("BootGuard not enabled\n");
    }
    else if (mask == 0x10000000) {
        printf("BootGuard verified boot is enabled\n");
    }
    else if (mask == 0x20000000) {
        printf("BootGuard measured boot is enabled\n");
    }
    else {
        printf("BootGuard verified + measured boot is enabled\n");
    }
    return;
}

VOID amd_psb_check() {
    printf("AMD CPU detected\n");
    //write to SMU_IN_ADDR
    pci_write_dword(pci_rt_bus, pci_rt_dev, pci_rt_fun, SMU_ADDR_OFFSET, (SMN_PUBLIC_BASE + PSB_STATUS_OFFSET));
    //read from SMU_OUT_ADDR
    DWORD psb_status = pci_read_dword(pci_rt_bus, pci_rt_dev, pci_rt_fun, SMU_DATA_OFFSET);;
    printf("PSB status: 0x%x\n", psb_status);

    if (psb_status & 0x1000000) {
        printf("PSB is enabled\n");
        return;
    }
    else {
        printf("PSB is not enabled\n");
        return;
    }
}

void cleanup() {
    if (RwDrv_handle) CloseHandle(RwDrv_handle);
    unload_driver();
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string driverPath = std::string(tempPath) + "RwDrv.sys";
    DeleteFileA(driverPath.c_str());
    printf("\nPress any key to exit\n");
    (void)getchar();
}

int main() {
    int status = load_driver();
    //printf("load_driver() return code: %d\n", status);
    RwDrv_handle = open_handle();
    if (RwDrv_handle == INVALID_HANDLE_VALUE) {
        printf("RwDrv handle acquisition failed\n");
        cleanup();
        return 1;
    }

    char cpu[13];
    get_cpu_vendor(cpu);
    //Force CPU for debugging.
    //strcpy_s(cpu, "GenuineIntel");
    //strcpy_s(cpu, "AuthenticAMD");

    // If Intel
    if (strcmp(cpu, "GenuineIntel") == 0) intel_bootguard_check();
    // If AMD
    else if (strcmp(cpu, "AuthenticAMD") == 0) amd_psb_check();
    else printf("Unsupported CPU detected\n");

    cleanup();
    return 0;
}