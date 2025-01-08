# psb_bg

Checks if PSB (Platform Secure Boot) or BootGuard are enabled.
Useful for determining if you can run unsigned firmware.

BootGuard = cannot run unsigned firmware.
PSB = cannot run unsigned firmware.

No PSB/BootGuard = can run unsigned firmware (flashing that firmware may still prove challenging)

# Hypervisors

In my testing, the tool requires windows to be running baremetal to prevent bluescreening when reading the MSR. 
The script attempts to check if it is running under a hypervisor. 
Easiest way I know of to reliably disable Hyper-V and all similar features is to just toggle AMD-V/VT-x in the UEFI Firmware.

# Windows Security / Defender

Requires vulnerable driver blocklist and memory integrity disabled because it uses RWEverything's driver.
Windows Defender will complain about bundled RWEverything, but I can not be bothered to fix.

# Status

I don't actually know that this can detect BootGuard or PSB, all I know is that it appears to be able to detect the absence of it.
If you can prove that it does or doesn't work, I would be interested to know. Feel free to open an issue.