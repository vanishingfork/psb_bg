#pragma once
//RwEverything.cpp
#include <windows.h>
#include <string>
#include <iostream>

#include "driver.h"
#include "RwEverything.hpp"

#define IOCTL_READ_MSR 0x222848
#define IOCTL_PCI_WRITE_DWORD 0x222844
#define IOCTL_PCI_READ_DWORD 0x222840
#define IOCTL_MEMORY_READ_DWORD 0x222808

    //static HANDLE RwDrvHandle;
    //static std::string driverPath;

    DWORD RwEverything::load_driver() {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        driverPath = std::string(tempPath) + "RwDrv.sys";

        HANDLE hFile = CreateFileA(driverPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            printf("Failed to write driver to disk. Error code: %lu\n", error);
            return error;
        }

        DWORD written = 0;
        WriteFile(hFile, driver, sizeof(driver), &written, nullptr);
        CloseHandle(hFile);

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
            wprintf(L"OpenSCManagerA failed.\n");
            return ERROR;
        }

        SC_HANDLE hService = OpenServiceA(hSCM, "RwDrv", SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
        if (!hService) {
            wprintf(L"OpenServiceA failed.\n");
            CloseServiceHandle(hSCM);
            return ERROR;
        }

        SERVICE_STATUS_PROCESS ssp;
        DWORD bytesNeeded;
        if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
            wprintf(L"QueryServiceStatusEx failed.\n");
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
        if (load_driver() != 0) {
            throw std::runtime_error("Failed to load driver");
        }
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
        DeleteFileA(driverPath.c_str());
    }

    DWORD RwEverything::read_msr(DWORD msr_address) {
        uint32_t iobuf[4] = { 0, 0, msr_address, 0 };
        DWORD bytes_returned = 0;
        BOOL success = DeviceIoControl(
            RwDrvHandle,
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

    DWORD RwEverything::pci_write_dword(BYTE bus, BYTE dev, BYTE func, WORD offset, DWORD value) {
        PCI_WRITE_DWORD cmd;
        cmd.bus = bus;
        cmd.dev = dev;
        cmd.func = func;
        cmd.offset = offset;
        cmd.value = value;
        /*
        buf[0] = bus & 0xFF;
        buf[1] = dev & 0xFF;
        buf[2] = fun & 0xFF;
        buf[3] = 0;
        buf[4] = (reg >> 0) & 0xFF;
        buf[5] = (reg >> 8) & 0xFF;
        
        buf[6] = (value >> 0) & 0xFF;
        buf[7] = (value >> 8) & 0xFF;
        buf[8] = (value >> 16) & 0xFF;
        buf[9] = (value >> 24) & 0xFF;
        
        
        printf("W RwEverything::pci_write_dword ibuf prewrite: ");
        for (int i = 0; i < sizeof(buf); i++) {
            printf("%02X ", buf[i]);
        }
        */
        DWORD bytes_returned = 0;
        BOOL success = DeviceIoControl(
            RwDrvHandle,
            IOCTL_PCI_WRITE_DWORD,
            &cmd,
            sizeof(cmd),
            &cmd,
            sizeof(cmd),
            &bytes_returned,
            nullptr
        );

        /*
        printf("W RwEverything::pci_write_dword obuf postwrite: ");
        for (int i = 0; i < sizeof(buf); i++) {
            printf("%02X ", buf[i]);
        }
        printf("\n");
        */
        return success;
    }

    DWORD RwEverything::pci_read_dword(BYTE bus, BYTE dev, BYTE func, WORD offset) {
        BYTE buf[12] = { 0 };
        PCI_READ_DWORD cmd;
        cmd.bus = bus;
        cmd.dev = dev;
        cmd.func = func;
        cmd.offset = offset;
        /*
        ibuf[0] = bus & 0xFF;
        ibuf[1] = dev & 0xFF;
        ibuf[2] = fun & 0xFF;
        ibuf[3] = 0;
        ibuf[4] = (offset >> 0) & 0xFF;
        ibuf[5] = (offset >> 8) & 0xFF;
        
        byte obuf[12] = { 0 };
        */
        DWORD bytes_returned = 0;
        BOOL success = DeviceIoControl(
            RwDrvHandle,
            IOCTL_PCI_READ_DWORD,
            &cmd,
            sizeof(cmd),
            &cmd,
            sizeof(cmd),
            &bytes_returned,
            nullptr
        );
        /*
        printf("W RwEverything::pci_read_dword buf postwrite: ");
        for (int i = 0; i < sizeof(cmd); i++) {
            printf("%02X ", *reinterpret_cast<BYTE*>(&reinterpret_cast<BYTE*>(&cmd)[i]));
        }
        printf("\n");
        */
        return cmd.value;
    }

     DWORD RwEverything::read_memory_dword(__int64 phys_address) {
        DWORD returnValue = -1;
        __int64 inBuffer = phys_address;
        DWORD bytesReturned = 0;
        int size = 4;
        int flag = 2;

        struct {
            __int64 addr;
            int size;
            int flag;
            DWORD* output;
        } ioBuffer = { inBuffer, size, flag, &returnValue };

        BOOL success = DeviceIoControl(RwDrvHandle,
            IOCTL_MEMORY_READ_DWORD,
            &ioBuffer,
            sizeof(ioBuffer),
            &ioBuffer,
            sizeof(ioBuffer),
            &bytesReturned,
            nullptr);

        if (!success) {
            std::cerr << "DeviceIoControl failed. Error: " << GetLastError() << std::endl;
            return -1;
        }
        return returnValue;
    }