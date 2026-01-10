#ifndef TEST_MMU_TABLES_H
#define TEST_MMU_TABLES_H

void test_ttbr0_allocated(void);
void test_l2_table_exists(void);
void test_l0_points_to_l1(void);
void test_identity_map_entry_0(void);
void test_identity_map_entry_64(void);
void test_all_128_entries_valid(void);

#endif
