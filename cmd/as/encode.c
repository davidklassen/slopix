#include "as.h"

// Extract general-purpose register number from token
// Returns register number (0-31), or -1 on error
int encode_gpr(Token *tok) {
	if (tok->kind != TOK_REGISTER) {
		return -1;
	}
	if (tok->reg_type == REG_FP) {
		return -1;
	}
	return tok->reg_num;
}

// Extract floating-point register number from token
// Returns register number (0-31), or -1 on error
int encode_fpr(Token *tok) {
	if (tok->kind != TOK_REGISTER) {
		return -1;
	}
	if (tok->reg_type != REG_FP) {
		return -1;
	}
	return tok->reg_num;
}

// Parse condition code string (e.g., "eq", "ne", "lt")
// Returns condition code (0-14), or -1 on error
int encode_cond(const char *cond_str) {
	static const struct {
		const char *name;
		int code;
	} conds[] = {
	    {"eq", COND_EQ},
	    {"ne", COND_NE},
	    {"cs", COND_CS},
	    {"hs", COND_CS},
	    {"cc", COND_CC},
	    {"lo", COND_CC},
	    {"mi", COND_MI},
	    {"pl", COND_PL},
	    {"vs", COND_VS},
	    {"vc", COND_VC},
	    {"hi", COND_HI},
	    {"ls", COND_LS},
	    {"ge", COND_GE},
	    {"lt", COND_LT},
	    {"gt", COND_GT},
	    {"le", COND_LE},
	    {"al", COND_AL},
	};

	for (size_t i = 0; i < sizeof(conds) / sizeof(conds[0]); i++) {
		if (strcasecmp(cond_str, conds[i].name) == 0) {
			return conds[i].code;
		}
	}
	return -1;
}

// Encode 12-bit immediate with optional shift
// Returns the 12-bit value, sets *shift to 0 or 12
// Returns -1 if value cannot be encoded
int encode_imm12(int64_t val, int *shift) {
	if (val < 0) {
		return -1;
	}
	if (val <= 0xFFF) {
		*shift = 0;
		return (int)val;
	}
	if ((val & 0xFFF) == 0 && (val >> 12) <= 0xFFF) {
		*shift = 12;
		return (int)(val >> 12);
	}
	return -1;
}

// Encode 16-bit immediate for MOVZ/MOVK/MOVN
// Returns the 16-bit value, or -1 if out of range
int encode_imm16(int64_t val) {
	if (val < 0 || val > 0xFFFF) {
		return -1;
	}
	return (int)val;
}

// Encode 26-bit branch offset (for B, BL)
// offset is in bytes, must be aligned to 4
// Returns encoded imm26 field, or -1 if out of range
int encode_branch26(int64_t offset) {
	if (offset & 3) {
		return -1;
	}
	int64_t imm = offset >> 2;
	if (imm < -(1 << 25) || imm >= (1 << 25)) {
		return -1;
	}
	return (int)(imm & 0x3FFFFFF);
}

// Encode 19-bit branch offset (for B.cond, CBZ, CBNZ)
// offset is in bytes, must be aligned to 4
// Returns encoded imm19 field, or -1 if out of range
int encode_branch19(int64_t offset) {
	if (offset & 3) {
		return -1;
	}
	int64_t imm = offset >> 2;
	if (imm < -(1 << 18) || imm >= (1 << 18)) {
		return -1;
	}
	return (int)(imm & 0x7FFFF);
}

// ADD Xd, Xn, Xm (register form)
// sf: 1 for 64-bit (X), 0 for 32-bit (W)
uint32_t encode_add_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x8B000000 : 0x0B000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ADD Xd, Xn, #imm (immediate form)
// sf: 1 for 64-bit, 0 for 32-bit
// shift: 0 or 1 (shift=1 means imm12 << 12)
uint32_t encode_add_imm(int sf, int rd, int rn, int imm12, int shift) {
	uint32_t base = sf ? 0x91000000 : 0x11000000;
	uint32_t sh = shift ? 1 : 0;
	return base | (sh << 22) | ((uint32_t)(imm12 & 0xFFF) << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SUB Xd, Xn, Xm (register form)
uint32_t encode_sub_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xCB000000 : 0x4B000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SUB Xd, Xn, #imm (immediate form)
uint32_t encode_sub_imm(int sf, int rd, int rn, int imm12, int shift) {
	uint32_t base = sf ? 0xD1000000 : 0x51000000;
	uint32_t sh = shift ? 1 : 0;
	return base | (sh << 22) | ((uint32_t)(imm12 & 0xFFF) << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// MOV Xd, Xm (alias for ORR Xd, XZR, Xm)
uint32_t encode_mov_reg(int sf, int rd, int rm) {
	uint32_t base = sf ? 0xAA0003E0 : 0x2A0003E0;
	return base | ((uint32_t)rm << 16) | (uint32_t)rd;
}

// MOVZ Xd, #imm, LSL #(hw*16)
// hw: 0, 1, 2, or 3 (shift amount / 16)
uint32_t encode_movz(int sf, int rd, int imm16, int hw) {
	uint32_t base = sf ? 0xD2800000 : 0x52800000;
	return base | ((uint32_t)hw << 21) | ((uint32_t)(imm16 & 0xFFFF) << 5) |
	       (uint32_t)rd;
}

// MOVK Xd, #imm, LSL #(hw*16)
uint32_t encode_movk(int sf, int rd, int imm16, int hw) {
	uint32_t base = sf ? 0xF2800000 : 0x72800000;
	return base | ((uint32_t)hw << 21) | ((uint32_t)(imm16 & 0xFFFF) << 5) |
	       (uint32_t)rd;
}

// MOVN Xd, #imm, LSL #(hw*16)
uint32_t encode_movn(int sf, int rd, int imm16, int hw) {
	uint32_t base = sf ? 0x92800000 : 0x12800000;
	return base | ((uint32_t)hw << 21) | ((uint32_t)(imm16 & 0xFFFF) << 5) |
	       (uint32_t)rd;
}

// B offset (unconditional branch)
uint32_t encode_b(int32_t offset) {
	int imm26 = encode_branch26(offset);
	if (imm26 < 0) {
		error("branch offset out of range: %d", offset);
	}
	return 0x14000000 | (uint32_t)imm26;
}

// BL offset (branch with link)
uint32_t encode_bl(int32_t offset) {
	int imm26 = encode_branch26(offset);
	if (imm26 < 0) {
		error("branch offset out of range: %d", offset);
	}
	return 0x94000000 | (uint32_t)imm26;
}

// B.cond offset (conditional branch)
uint32_t encode_bcond(int cond, int32_t offset) {
	int imm19 = encode_branch19(offset);
	if (imm19 < 0) {
		error("branch offset out of range: %d", offset);
	}
	return 0x54000000 | ((uint32_t)imm19 << 5) | (uint32_t)cond;
}

// CBZ Xt, offset (compare and branch if zero)
uint32_t encode_cbz(int sf, int rt, int32_t offset) {
	int imm19 = encode_branch19(offset);
	if (imm19 < 0) {
		error("branch offset out of range: %d", offset);
	}
	uint32_t base = sf ? 0xB4000000 : 0x34000000;
	return base | ((uint32_t)imm19 << 5) | (uint32_t)rt;
}

// CBNZ Xt, offset (compare and branch if not zero)
uint32_t encode_cbnz(int sf, int rt, int32_t offset) {
	int imm19 = encode_branch19(offset);
	if (imm19 < 0) {
		error("branch offset out of range: %d", offset);
	}
	uint32_t base = sf ? 0xB5000000 : 0x35000000;
	return base | ((uint32_t)imm19 << 5) | (uint32_t)rt;
}

// RET {Xn} (return, default X30)
uint32_t encode_ret(int rn) {
	return 0xD65F0000 | ((uint32_t)rn << 5);
}

// BLR Xn (branch with link to register)
uint32_t encode_blr(int rn) {
	return 0xD63F0000 | ((uint32_t)rn << 5);
}

static int test_count = 0;
static int test_pass = 0;

static void check_encoding(const char *name, uint32_t got, uint32_t expected) {
	test_count++;
	if (got == expected) {
		test_pass++;
		printf("  PASS: %s = 0x%08X\n", name, got);
	} else {
		printf("  FAIL: %s = 0x%08X (expected 0x%08X)\n", name, got, expected);
	}
}

void test_encode(void) {
	printf("Testing instruction encoding...\n\n");

	printf("ADD register:\n");
	check_encoding("ADD x0, x1, x2", encode_add_reg(1, 0, 1, 2), 0x8B020020);
	check_encoding("ADD w0, w1, w2", encode_add_reg(0, 0, 1, 2), 0x0B020020);
	check_encoding("ADD x10, x20, x30", encode_add_reg(1, 10, 20, 30), 0x8B1E028A);

	printf("\nADD immediate:\n");
	check_encoding("ADD x0, x1, #42", encode_add_imm(1, 0, 1, 42, 0), 0x9100A820);
	check_encoding("ADD w0, w1, #42", encode_add_imm(0, 0, 1, 42, 0), 0x1100A820);
	check_encoding("ADD x0, x1, #1, LSL #12", encode_add_imm(1, 0, 1, 1, 1), 0x91400420);

	printf("\nSUB register:\n");
	check_encoding("SUB x0, x1, x2", encode_sub_reg(1, 0, 1, 2), 0xCB020020);
	check_encoding("SUB w0, w1, w2", encode_sub_reg(0, 0, 1, 2), 0x4B020020);

	printf("\nSUB immediate:\n");
	check_encoding("SUB x0, x1, #42", encode_sub_imm(1, 0, 1, 42, 0), 0xD100A820);
	check_encoding("SUB w0, w1, #42", encode_sub_imm(0, 0, 1, 42, 0), 0x5100A820);

	printf("\nMOV register:\n");
	check_encoding("MOV x0, x1", encode_mov_reg(1, 0, 1), 0xAA0103E0);
	check_encoding("MOV w0, w1", encode_mov_reg(0, 0, 1), 0x2A0103E0);
	check_encoding("MOV x10, x20", encode_mov_reg(1, 10, 20), 0xAA1403EA);

	printf("\nMOVZ:\n");
	check_encoding("MOVZ x0, #0x1234", encode_movz(1, 0, 0x1234, 0), 0xD2824680);
	check_encoding("MOVZ w0, #0x1234", encode_movz(0, 0, 0x1234, 0), 0x52824680);
	check_encoding("MOVZ x0, #0x1234, LSL #16", encode_movz(1, 0, 0x1234, 1), 0xD2A24680);

	printf("\nMOVK:\n");
	check_encoding("MOVK x0, #0x5678", encode_movk(1, 0, 0x5678, 0), 0xF28ACF00);
	check_encoding("MOVK x0, #0x5678, LSL #16", encode_movk(1, 0, 0x5678, 1), 0xF2AACF00);

	printf("\nMOVN:\n");
	check_encoding("MOVN x0, #0", encode_movn(1, 0, 0, 0), 0x92800000);
	check_encoding("MOVN w0, #0", encode_movn(0, 0, 0, 0), 0x12800000);

	printf("\nB (unconditional branch):\n");
	check_encoding("B +8", encode_b(8), 0x14000002);
	check_encoding("B +16", encode_b(16), 0x14000004);
	check_encoding("B -4", encode_b(-4), 0x17FFFFFF);

	printf("\nBL (branch with link):\n");
	check_encoding("BL +8", encode_bl(8), 0x94000002);
	check_encoding("BL +16", encode_bl(16), 0x94000004);

	printf("\nB.cond (conditional branch):\n");
	check_encoding("B.EQ +12", encode_bcond(COND_EQ, 12), 0x54000060);
	check_encoding("B.NE +8", encode_bcond(COND_NE, 8), 0x54000041);
	check_encoding("B.LT +20", encode_bcond(COND_LT, 20), 0x540000AB);

	printf("\nCBZ/CBNZ:\n");
	check_encoding("CBZ x0, +8", encode_cbz(1, 0, 8), 0xB4000040);
	check_encoding("CBZ w0, +8", encode_cbz(0, 0, 8), 0x34000040);
	check_encoding("CBNZ x0, +8", encode_cbnz(1, 0, 8), 0xB5000040);
	check_encoding("CBNZ w0, +8", encode_cbnz(0, 0, 8), 0x35000040);

	printf("\nRET:\n");
	check_encoding("RET (x30)", encode_ret(30), 0xD65F03C0);
	check_encoding("RET x0", encode_ret(0), 0xD65F0000);

	printf("\nBLR:\n");
	check_encoding("BLR x0", encode_blr(0), 0xD63F0000);
	check_encoding("BLR x30", encode_blr(30), 0xD63F03C0);

	printf("\nCondition code parsing:\n");
	printf("  encode_cond(\"eq\") = %d (expected 0)\n", encode_cond("eq"));
	printf("  encode_cond(\"NE\") = %d (expected 1)\n", encode_cond("NE"));
	printf("  encode_cond(\"lt\") = %d (expected 11)\n", encode_cond("lt"));
	printf("  encode_cond(\"hs\") = %d (expected 2, alias for cs)\n",
	       encode_cond("hs"));
	printf("  encode_cond(\"lo\") = %d (expected 3, alias for cc)\n",
	       encode_cond("lo"));

	printf("\nImmediate encoding:\n");
	int shift;
	printf("  encode_imm12(42, &shift) = %d, shift=%d\n", encode_imm12(42, &shift), shift);
	printf("  encode_imm12(0x1000, &shift) = %d, shift=%d\n", encode_imm12(0x1000, &shift), shift);
	printf("  encode_imm12(0xFFF, &shift) = %d, shift=%d\n", encode_imm12(0xFFF, &shift), shift);
	printf("  encode_imm12(0xFFF000, &shift) = %d, shift=%d\n", encode_imm12(0xFFF000, &shift), shift);
	printf("  encode_imm12(0x1001, &shift) = %d (invalid)\n", encode_imm12(0x1001, &shift));

	printf("\nBranch offset encoding:\n");
	printf("  encode_branch26(8) = 0x%X\n", encode_branch26(8));
	printf("  encode_branch26(-4) = 0x%X\n", encode_branch26(-4));
	printf("  encode_branch19(12) = 0x%X\n", encode_branch19(12));
	printf("  encode_branch19(-8) = 0x%X\n", encode_branch19(-8));

	printf("\n========================================\n");
	printf("Results: %d/%d tests passed\n", test_pass, test_count);

	if (test_pass != test_count) {
		exit(1);
	}
}
