//RwEverything.cpp
#include "RwEverything.hpp"
#include "driver.hpp"
#include <iostream>
#include <windows.h>

typedef unsigned long long QWORD;

#define IOCTL_READ_MSR 0x222848
#define IOCTL_PCI_WRITE_DWORD 0x222844
#define IOCTL_PCI_READ_DWORD 0x222840
#define IOCTL_MEMORY_READ_DWORD 0x222808

#define IOCTL_FAIL 0x4141414141414141

DWORD RwEverything::load_driver() {
	GetTempPathA(MAX_PATH, driverPath);
	strcat_s(driverPath, "RwDrv.sys");

	HANDLE hFile = CreateFileA(driverPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		printf("Failed to write driver to disk. Error code: %lu\n", error);
		return 1;
	}

	WriteFile(hFile, driver, (DWORD)driver_size, NULL, nullptr);
	CloseHandle(hFile);
	SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
	if (!hSCM) {
		DeleteFileA(driverPath);
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
		driverPath,
		nullptr, nullptr, nullptr, nullptr, nullptr);

	if (!hService) {
		printf("Failed to create service.\n");
		CloseServiceHandle(hSCM);
		return 1;
	}

	if (!StartServiceA(hService, 0, nullptr)) {
		printf("Failed to start service.");
		DeleteService(hService);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return 1;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return 0;
}

DWORD RwEverything::unload_driver() {
	SC_HANDLE hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCM) {
		printf("OpenSCManagerA failed.\n");
		return ERROR;
	}

	SC_HANDLE hService = OpenServiceA(hSCM, "RwDrv", SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
	if (!hService) {
		printf("OpenServiceA failed.\n");
		CloseServiceHandle(hSCM);
		return ERROR;
	}

	SERVICE_STATUS_PROCESS ssp;
	DWORD bytesNeeded;
	if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
		printf("QueryServiceStatusEx failed.\n");
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return ERROR;
	}

	if (ssp.dwCurrentState != SERVICE_STOPPED) {
		if (!ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
			DWORD error = GetLastError();
			if (error != ERROR_SERVICE_NOT_ACTIVE) {
				printf("ControlService failed.\n");
				CloseServiceHandle(hService);
				CloseServiceHandle(hSCM);
				return ERROR;
			}
		}
	}

	if (!DeleteService(hService)) {
		DWORD error = GetLastError();
		printf("DeleteService failed. Error code: %lu\n", error);
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return ERROR;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return 1;
}

RwEverything::RwEverything() {
	if (load_driver() != 0) throw std::runtime_error("Failed to load driver");
	RwDrvHandle = CreateFileA(
		"\\\\.\\RwDrv",                      // Device name 
		GENERIC_READ | GENERIC_WRITE,        // Access mode
		0,                                   // Share mode (exclusive)
		NULL,                                // Security attributes
		OPEN_EXISTING,                       // Creation disposition
		FILE_ATTRIBUTE_NORMAL,               // Flags and attributes
		NULL                                 // Template file
	);
	if (RwDrvHandle == INVALID_HANDLE_VALUE) {
		printf("RwDrv handle acquisition failed\n");
		this->~RwEverything(); // do the spooky self-destructor call
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
	//NOT FUNCTIONAL ATM
	/*
	DWORD returnValue = -1;
	QWORD inBuffer = phys_address;
	DWORD bytesReturned = 0;
	int size = 4;
	int flag = 2;

	struct {
		QWORD addr;
		int size;
		int flag;
		DWORD* output;
	} ioBuffer = { phys_address, size, flag, &returnValue };

	if (!DeviceIoControl(RwDrvHandle,
		IOCTL_MEMORY_READ_DWORD,
		&ioBuffer,
		sizeof(ioBuffer),
		&ioBuffer,
		sizeof(ioBuffer),
		nullptr,
		nullptr)) 
	{
		return returnValue;
	}
	return -1;
	*/
}