//main.cpp
#include <Windows.h>
#include <iostream>
#include <string>
#include <stdint.h>
#include <intrin.h>

#include "RwEverything.hpp"

// Intel
#define BG_MSR 0x13A // Intel Boot Guard MSR

// AMD
// SYSTEM MANAGEMENT UNIT (SMU) Registers
#define SMU_INDEX_ADDR 0xB8
#define SMU_DATA_ADDR 0xBC

// SYSTEM MANAGEMENT NETWORK (SMN) Registers
#define SMN_PUBLIC_BASE 0x03800000
#define PSB_STATUS_OFFSET 0x00010994
//03810994

// 0x13E102E0 for families 17h, model 30h/70h or family 19h, model 20h
// 0x13B102E0 for all other models
#define PSP_MMIO_BASE 0x13E102E0
#define PSP_MMIO_BASE_ALT 0x13B102E0

// Useful values
#define CONFIG_UNIMPLEMENTED_FUNCTION 0xFFFFFFFF
#define CONFIG_UNIMPLEMENTED_REGISTER 0x00000000

// PCI Root Complex 
#define pci_rt_bus 0
#define pci_rt_dev 0
#define pci_rt_fun 0

//my fav antipattern
RwEverything rwe;

DWORD PCI_ID_READ_DWORD(BYTE bus,BYTE dev,BYTE func, DWORD value) {
    rwe.pci_write_dword(bus, dev, func, SMU_INDEX_ADDR, value);
    return rwe.pci_read_dword(bus, dev, func, SMU_DATA_ADDR);
}

VOID get_cpu_vendor(char* vendor) {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    memcpy(vendor + 0, &cpuInfo[1], 4); // EBX
    memcpy(vendor + 8, &cpuInfo[2], 4); // ECX lovely quirks of x86 cpuid
    memcpy(vendor + 4, &cpuInfo[3], 4); // EDX
    vendor[12] = '\0';
}

VOID intel_bootguard_check() {                     // Enjoy my weird formatting
    DWORD msr_val = rwe.read_msr(BG_MSR);              // MSR address -> BootGuard
    printf("Intel CPU detected\nBootGuard MSR: 0x%08X\n", msr_val);
    if ((msr_val & 0x30000000) == 0x0)             printf("BootGuard: disabled\n");
    else if ((msr_val & 0x30000000) == 0x10000000) printf("BootGuard: verified boot\n");
    else if ((msr_val & 0x30000000) == 0x20000000) printf("BootGuard: measured boot\n");
    else if ((msr_val & 0x30000000) == 0x30000000) printf("BootGuard: verified + measured boot\n");
}


VOID amd_psb_check() {
    printf("AMD CPU detected\n");
    DWORD config = PCI_ID_READ_DWORD(0x0,0x0,0x0,SMN_PUBLIC_BASE+PSB_STATUS_OFFSET); //read psp psb status register (0x3810994)
    //shamelessly stolen from https://github.com/IOActive/Platbox.
    printf("PSP Config: %08x\n", config);
    printf(" - Platform vendor ID: %02x\n", config & 0xFF);
    printf(" - Platform model ID: %02x\n", (config >> 8) & 0xF);
    printf(" - BIOS key revision ID: %04x\n", (config >> 12) & 0xFFFF);
    printf(" - Root key select: %02x\n", (config >> 16) & 0xF);
    printf(" - Platform Secure Boot Enable: %d\n", (config >> 24) & 1);
    printf(" - Disable BIOS key anti-rollback:  %d\n", (config >> 25) & 1);
    printf(" - Disable AMD key usage:  %d\n", (config >> 26) & 1);
    printf(" - Disable secure debug unlock:  %d\n", (config >> 27) & 1);
    printf(" - Customer key unlock:  %d\n", (config >> 28) & 1);
    printf("\n");
}

BOOL is_hypervisor() {
    // Open a handle to the Service Control Manager
    SC_HANDLE scmHandle = OpenSCManager(nullptr, nullptr, GENERIC_READ);
    if (!scmHandle) return false; // Could not open SCM
    // Open a handle to the Hyper-V VM Management service (vmms)
    SC_HANDLE serviceHandle = OpenService(scmHandle, L"vmms", SERVICE_QUERY_STATUS);
    if (!serviceHandle) {
        CloseServiceHandle(scmHandle);
        return false; // Service not found or can't be opened
    }
    SERVICE_STATUS_PROCESS ssp = {};
    DWORD bytesNeeded = 0;
    bool isRunning = false;
    if (QueryServiceStatusEx(    // Query Hyper-V VM Management service status
        serviceHandle,
        SC_STATUS_PROCESS_INFO,
        reinterpret_cast<LPBYTE>(&ssp),
        sizeof(ssp),
        &bytesNeeded
    )) isRunning = (ssp.dwCurrentState == SERVICE_RUNNING);
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
    return isRunning;
}

int main() {
    char cpu[13]; //12 + 1 for null term
    get_cpu_vendor(cpu);
    if (is_hypervisor()) {
        printf("You appear to be running under Hyper-V\nPress any key to continue\nCtrl+C to exit and avoid potential bluescreen.\n");
        (void)getchar();
    }
    if (!strcmp(cpu, "GenuineIntel")) intel_bootguard_check();
    else if (!strcmp(cpu, "AuthenticAMD")) amd_psb_check();
    else printf("Unsupported CPU detected\n");
    return 0;
}