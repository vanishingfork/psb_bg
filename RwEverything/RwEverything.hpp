#ifndef RWEVERYTHING_HPP
#define RWEVERYTHING_HPP

#include <windows.h>
#include <iostream>

typedef unsigned long long QWORD; //because I want the lines to line up.

struct PCI_RW_DWORD {
	BYTE bus;      // [0]
	BYTE dev;      // [1]
	BYTE func;     // [2]
	BYTE pad0 = 0; // [3] 
	WORD offset;   // [4..5]
	WORD pad1 = 0; // [6..7] 
	DWORD value;   // [8..11]
};
struct MsrReadBuf {
	QWORD main;
	DWORD msr;
	DWORD ormask;
};

class RwEverything {
private:
	HANDLE RwDrvHandle;
	char driverPath[MAX_PATH];
	DWORD load_driver();
	DWORD unload_driver();
public:
	RwEverything();
	~RwEverything();
	QWORD read_msr(DWORD msr_address);
	DWORD pci_write_dword(BYTE bus, BYTE dev, BYTE func, WORD offset, DWORD value);
	DWORD pci_read_dword(BYTE bus, BYTE dev, BYTE func, WORD offset);
	DWORD read_memory_dword(QWORD phys_address);
};

#endif // RWEVERYTHING_HPP