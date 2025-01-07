#pragma once
#ifndef RWEVERYTHING_HPP
#define RWEVERYTHING_HPP

#include <windows.h>
#include <string>
#include <iostream>

#pragma pack(push,1)
struct PCI_WRITE_DWORD {
    BYTE bus;      // [0]
    BYTE dev;      // [1]
    BYTE func;     // [2]
    BYTE pad0 = 0;     // [3] (padding to align offset)
    WORD offset;   // [4..5]
    WORD pad1 = 0;    // [6..7] (padding to align value)
    DWORD value;    // [8..11]
};
struct PCI_READ_DWORD {
    BYTE bus;    // 0
    BYTE dev;    // 1
    BYTE func;   // 2
    BYTE pad0;   // 3
    WORD offset; // 4..5
    WORD pad1;   // 6..7
    DWORD value; // 8..11
};
#pragma pack(pop)

class RwEverything {
private:
    HANDLE RwDrvHandle;
    std::string driverPath;

    DWORD load_driver();
    DWORD unload_driver();

public:
    RwEverything();
    ~RwEverything();

    DWORD read_msr(DWORD msr_address);
    DWORD pci_write_dword(BYTE bus, BYTE dev, BYTE func, WORD offset, DWORD value);
    DWORD pci_read_dword(BYTE bus, BYTE dev, BYTE func, WORD offset);
    DWORD read_memory_dword(__int64 phys_address);
};

#endif // RWEVERYTHING_HPP