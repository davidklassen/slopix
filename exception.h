#ifndef EXCEPTION_H
#define EXCEPTION_H

#define ESR_EC(esr)  (((esr) >> 26) & 0x3F)
#define ESR_ISS(esr) ((esr) & 0x1FFFFFF)

// Exception class values
#define EC_UNKNOWN     0x00
#define EC_SVC_AARCH64 0x15
#define EC_IABT_SAME   0x21
#define EC_DABT_SAME   0x25

// Data/Instruction Fault Status Code (DFSC/IFSC) - ISS bits [5:0]
#define FSC_MASK      0x3F
#define FSC_ADDR_L0   0x00 // Address size fault, level 0
#define FSC_ADDR_L1   0x01 // Address size fault, level 1
#define FSC_ADDR_L2   0x02 // Address size fault, level 2
#define FSC_ADDR_L3   0x03 // Address size fault, level 3
#define FSC_TRANS_L0  0x04 // Translation fault, level 0
#define FSC_TRANS_L1  0x05 // Translation fault, level 1
#define FSC_TRANS_L2  0x06 // Translation fault, level 2
#define FSC_TRANS_L3  0x07 // Translation fault, level 3
#define FSC_ACCESS_L1 0x09 // Access flag fault, level 1
#define FSC_ACCESS_L2 0x0A // Access flag fault, level 2
#define FSC_ACCESS_L3 0x0B // Access flag fault, level 3
#define FSC_PERM_L1   0x0D // Permission fault, level 1
#define FSC_PERM_L2   0x0E // Permission fault, level 2
#define FSC_PERM_L3   0x0F // Permission fault, level 3
#define FSC_SYNC_EXT  0x10 // Synchronous external abort
#define FSC_ALIGN     0x21 // Alignment fault

// ISS bits for data abort
#define ISS_WNR (1 << 6) // Write not Read (1=write, 0=read)

struct trap_frame {
	unsigned long regs[31];
	unsigned long sp_el0;
	unsigned long elr;
	unsigned long spsr;
};

const char *fault_status_string(unsigned int fsc);

#endif
