//main.cpp
#include <Windows.h>
#include <intrin.h>
#include <iostream>
#include "../RwEverything/RwEverything.hpp"

// Intel
#define BG_MSR 0x13A // Intel Boot Guard MSR

// AMD
// SYSTEM MANAGEMENT UNIT (SMU) Registers + Offsets
#define SMU_INDEX_ADDR 0xB8
#define SMU_DATA_ADDR 0xBC
#define SMN_PUBLIC_BASE 0x03800000
#define PSB_STATUS_OFFSET 0x00010994 // SMN+PSB = 0x03810994

// PCI Root Complex 
#define pci_rt_bus 0
#define pci_rt_dev 0
#define pci_rt_fun 0

//my fav antipattern
RwEverything rwe;

#define IOCTL_FAIL 0x4141414141414141

BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		rwe.~RwEverything(); // Call the destructor directly
		return TRUE;
	default:
		return FALSE;
	}
}

VOID get_cpu_vendor(char* vendor) {
	int cpuInfo[4];
	__cpuid(cpuInfo, 0);
	memcpy(vendor + 0, &cpuInfo[1], 4); // EBX
	memcpy(vendor + 8, &cpuInfo[2], 4); // ECX lovely quirks of x86 cpuid
	memcpy(vendor + 4, &cpuInfo[3], 4); // EDX
	vendor[12] = '\0';
}

BOOL is_hypervisor() {
	int cpuInfo[4] = { 0 };
	__cpuid(cpuInfo, 1); // Check the hypervisor present bit (bit 31 of ECX)
	return (cpuInfo[2] & (1 << 31)) != 0;
}

VOID intel_bootguard_check() { // Enjoy my weird formatting
	QWORD msr_val = rwe.read_msr(BG_MSR); // MSR address -> BootGuard
	if (msr_val == IOCTL_FAIL) {
		printf("IOCTL FAILED!\n");
		return;
	}
	printf("Intel CPU detected\nBootGuard MSR: 0x%016llx\n", msr_val);
	switch (msr_val & 0x30000000) { //we only care about MSR_0x13A[29:28]
		case 0x0: printf("BootGuard: disabled\n"); break;
		case 0x10000000: printf("BootGuard: verified boot\n"); break;
		case 0x20000000: printf("BootGuard: measured boot\n"); break;
		case 0x30000000: printf("BootGuard: verified + measured boot\n"); break;
		default: printf("BootGuard: unknown status\n"); break;
	}
}

DWORD SMU_READ_DWORD(DWORD value) {
	rwe.pci_write_dword(pci_rt_bus, pci_rt_dev, pci_rt_fun, SMU_INDEX_ADDR, value);
	return rwe.pci_read_dword(pci_rt_bus, pci_rt_dev, pci_rt_fun, SMU_DATA_ADDR);
}

VOID amd_psb_check() {
	printf("AMD CPU detected\n");
	DWORD config = SMU_READ_DWORD(SMN_PUBLIC_BASE + PSB_STATUS_OFFSET); //read psp psb status register (0x3810994)
	printf("PSP Config: %08x\n", config); //shamelessly stolen from https://github.com/IOActive/Platbox.
	printf(" - Platform vendor ID: %02x\n", config & 0xFF);
	printf(" - Platform model ID: %02x\n", (config >> 8) & 0xF);
	printf(" - BIOS key revision ID: %04x\n", (config >> 12) & 0xFFFF);
	printf(" - Root key select: %02x\n", (config >> 16) & 0xF);
	printf(" - Platform Secure Boot Enable: %d\n", (config >> 24) & 1);
	printf(" - Disable BIOS key anti-rollback:  %d\n", (config >> 25) & 1);
	printf(" - Disable AMD key usage:  %d\n", (config >> 26) & 1);
	printf(" - Disable secure debug unlock:  %d\n", (config >> 27) & 1);
	printf(" - Customer key unlock:  %d\n", (config >> 28) & 1);
}

int main() {
	SetConsoleCtrlHandler(ConsoleHandler, TRUE); //make sure to setup the descructor handler
	char cpu[13]; //12 + 1 for null term
	get_cpu_vendor(cpu);
	if (is_hypervisor() && !strcmp(cpu, "GenuineIntel")) { // If Hyper-V and Intel Processor, give user chance to abort.
		printf("You appear to be running under a hypervisor\nPress any key to continue\nCtrl+C to exit and avoid potential bluescreen.\n");
		(void)getchar();
	}

	if (!strcmp(cpu, "GenuineIntel")) intel_bootguard_check();
	else if (!strcmp(cpu, "AuthenticAMD")) amd_psb_check();
	else printf("Unsupported CPU detected\n");
	printf("\nPress enter to exit.\n");
	(void)getchar();
	return 0;
}