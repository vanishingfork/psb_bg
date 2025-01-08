#ifndef RWEVERYTHING_HPP
#define RWEVERYTHING_HPP

#include <windows.h>

typedef unsigned long long QWORD; //because I want the lines to line up.

struct PCI_RW_DWORD {
	BYTE bus;      // [00]
	BYTE dev;      // [01]
	BYTE func;     // [02]
	BYTE pad0 = 0; // [03]     // words must be <address> % 2
	WORD offset;   // [04..05]
	WORD pad1 = 0; // [06..07] // dwords must be <address> % 4
	DWORD value;   // [08..11]
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
	BOOL did_load_driver = FALSE;
	BOOL load_driver();
	BOOL unload_driver();
public:
	RwEverything();
	~RwEverything();
	QWORD read_msr(DWORD msr_address);
	DWORD pci_write_dword(BYTE bus, BYTE dev, BYTE func, WORD offset, DWORD value);
	DWORD pci_read_dword(BYTE bus, BYTE dev, BYTE func, WORD offset);
	DWORD read_memory_dword(QWORD phys_address);
};

#endif // RWEVERYTHING_HPP