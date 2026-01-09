#ifndef PMM_H
#define PMM_H

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
unsigned long pmm_get_free_pages(void);
unsigned long pmm_get_total_pages(void);

#endif
