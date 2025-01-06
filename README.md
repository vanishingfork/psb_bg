# psb_bg

Checks if PSB (Platform Secure Boot) or BootGuard are enabled.
Useful for determining if you can run unsigned firmware.

BootGuard = cannot run unsigned firmware.
PSB = cannot run unsigned firmware.

No PSB/BootGuard = can run unsigned firmware (flashing that firmware may still prove challenging)

# Hypervisors

Running under a Hypervisor such as Hyper-V on Intel results in a BSOD for me.

# Status

I don't actually know that this can detect BootGuard or PSB, all I know is that it appears to be able to detect the absence of it.
If you can prove that it does or doesn't work, I would be interested to know. Feel free to open an issue.