#ifndef MMU_H
#define MMU_H

void mmu_init(void);
unsigned long mmu_get_ttbr0(void);
unsigned long mmu_get_ttbr1(void);
unsigned long* mmu_get_l2_table(void);
unsigned long* mmu_get_ttbr1_l2_kernel(void);

#endif
