//RwEverything.cpp
#include "RwEverything.hpp"
#include "driver.hpp"
#include <windows.h>
#include <stdio.h>

typedef unsigned long long QWORD;

#define IOCTL_READ_MSR 0x222848
#define IOCTL_PCI_WRITE_DWORD 0x222844
#define IOCTL_PCI_READ_DWORD 0x222840
#define IOCTL_MEMORY_READ_DWORD 0x222808

#define IOCTL_FAIL 0x4141414141414141


BOOL RwEverything::load_driver() {
	SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (!hSCM) {
		printf("Failed to open SC Manager.\n");
		return ERROR; // ERROR = FALSE
	}

	SC_HANDLE hService = OpenServiceA(hSCM, "RwDrv", SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
	if (hService) {
		SERVICE_STATUS status = {};
		if (QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_RUNNING) {
			printf("Service already running.\n");
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCM);
			return TRUE; // already loaded, but result is the same.
		}
		CloseServiceHandle(hService);
	}

	GetTempPathA(MAX_PATH, driverPath);
	strcat_s(driverPath, "RwDrv.sys");

	HANDLE hFile = CreateFileA(driverPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("Failed to write driver to disk.\n");
		CloseServiceHandle(hSCM);
		return ERROR;
	}
	WriteFile(hFile, driver, (DWORD)driver_size, nullptr, nullptr);
	CloseHandle(hFile);

	hService = CreateServiceA(hSCM,
		"RwDrv",
		"RwDrv",
		SERVICE_START | DELETE | SERVICE_STOP,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_IGNORE,
		driverPath,
		nullptr, nullptr, nullptr, nullptr, nullptr);

	if (!hService) {
		printf("Failed to create service.\n");
		DeleteFileA(driverPath);
		CloseServiceHandle(hSCM);
		return ERROR;
	}

	if (!StartServiceA(hService, 0, nullptr)) {
		printf("Failed to start service.\n");
		DeleteService(hService);
		DeleteFileA(driverPath);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return ERROR;
	}
	did_load_driver = TRUE;
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return TRUE;
}


BOOL RwEverything::unload_driver() {
	if (did_load_driver == FALSE) return -1;// we_loaded_driver driver == FALSE -> we didnt load the driver, dont unload, its not our responsibility.
	SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCM) {
		printf("OpenSCManagerA failed.\n");
		DeleteFileA(driverPath);
		return ERROR;
	}

	SC_HANDLE hService = OpenServiceA(hSCM, "RwDrv", SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
	if (!hService) {
		printf("OpenServiceA failed.\n");
		CloseServiceHandle(hSCM);
		DeleteFileA(driverPath);
		return ERROR;
	}

	SERVICE_STATUS_PROCESS ssp;
	DWORD bytesNeeded;
	if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
		printf("QueryServiceStatusEx failed.\n");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		DeleteFileA(driverPath);
		return ERROR;
	}

	if (ssp.dwCurrentState != SERVICE_STOPPED) {
		if (!ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
			DWORD error = GetLastError();
			if (error != ERROR_SERVICE_NOT_ACTIVE) {
				printf("ControlService failed.\n");
				CloseServiceHandle(hService);
				CloseServiceHandle(hSCM);
				DeleteFileA(driverPath);
				return ERROR;
			}
		}
	}

	if (!DeleteService(hService)) {
		DWORD error = GetLastError();
		printf("DeleteService failed. Error code: %lu\n", error);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		DeleteFileA(driverPath);
		return ERROR;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return 1;
}

RwEverything::RwEverything() {
	if (load_driver() == ERROR) this->~RwEverything(); //See errors in load_driver() source
	
	RwDrvHandle = CreateFileA(
		"\\\\.\\RwDrv",                      // Device name 
		GENERIC_READ | GENERIC_WRITE,        // Access mode
		0,									 // Share mode (exclusive)
		NULL,                                // Security attributes
		OPEN_EXISTING,                       // Creation disposition
		FILE_ATTRIBUTE_NORMAL,               // Flags and attributes
		NULL                                 // Template file
	);
	if (RwDrvHandle == INVALID_HANDLE_VALUE) {
		printf("RwDrv handle acquisition failed.\n");
		this->~RwEverything(); 
	}
}

RwEverything::~RwEverything() {
	if (RwDrvHandle) CloseHandle(RwDrvHandle);
	unload_driver();
	DeleteFileA(driverPath);
}

QWORD RwEverything::read_msr(DWORD msr_address) {
	MsrReadBuf cmd { 0, msr_address, 0 };
	if (DeviceIoControl(RwDrvHandle, IOCTL_READ_MSR, &cmd, sizeof(cmd), &cmd, sizeof(cmd), nullptr, nullptr))
		return cmd.main | (static_cast<QWORD>(cmd.ormask)) << 32; //align the two values and or them. funky driver quirk.
	return IOCTL_FAIL;
}

DWORD RwEverything::pci_write_dword(BYTE bus, BYTE dev, BYTE func, WORD offset, DWORD value) {
	PCI_RW_DWORD cmd{ bus, dev, func, 0, offset, 0, value };
	if (DeviceIoControl(RwDrvHandle, IOCTL_PCI_WRITE_DWORD, &cmd, sizeof(cmd), &cmd, sizeof(cmd), nullptr, nullptr))
		return cmd.value;
	return static_cast<DWORD>(IOCTL_FAIL);
}

DWORD RwEverything::pci_read_dword(BYTE bus, BYTE dev, BYTE func, WORD offset) {
	PCI_RW_DWORD cmd{ bus, dev, func, 0, offset, 0, 0 };
	if (DeviceIoControl(RwDrvHandle, IOCTL_PCI_READ_DWORD, &cmd, sizeof(cmd), &cmd, sizeof(cmd), nullptr, nullptr))
		return cmd.value;
	return static_cast<DWORD>(IOCTL_FAIL);
}

DWORD RwEverything::read_memory_dword(QWORD phys_address) {
	return 0;
}