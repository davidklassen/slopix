#ifndef MMU_H
#define MMU_H

void mmu_init(void);
unsigned long mmu_get_ttbr0(void);
unsigned long mmu_get_ttbr1(void);

#endif
