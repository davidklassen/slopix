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

// Encode 9-bit signed immediate for pre/post-index addressing (-256 to 255)
// Returns the 9-bit value (sign-extended), or -1 if out of range
int encode_imm9(int64_t val) {
	if (val < -256 || val > 255) {
		return -1;
	}
	return (int)(val & 0x1FF);
}

// Encode 7-bit signed scaled immediate for STP/LDP
// scale: 3 for 64-bit (8 bytes), 2 for 32-bit (4 bytes)
// Returns the 7-bit value, or -1 if out of range or misaligned
int encode_imm7(int64_t val, int scale) {
	int64_t unit = 1LL << scale;
	if (val % unit != 0) {
		return -1;
	}
	int64_t imm = val >> scale;
	if (imm < -64 || imm > 63) {
		return -1;
	}
	return (int)(imm & 0x7F);
}

// Encode 12-bit unsigned scaled immediate
// scale: 0 for byte, 1 for halfword, 2 for word, 3 for doubleword
// Returns the 12-bit value, or -1 if out of range or misaligned
int encode_imm12_unsigned(int64_t val, int scale) {
	if (val < 0) {
		return -1;
	}
	int64_t unit = 1LL << scale;
	if (val % unit != 0) {
		return -1;
	}
	int64_t imm = val >> scale;
	if (imm > 0xFFF) {
		return -1;
	}
	return (int)imm;
}

// Invert condition code for CSET encoding (flip LSB)
int invert_cond(int cond) {
	return cond ^ 1;
}

// Encode 21-bit PC-relative offset for ADR/ADRP
// For ADRP: offset is page-aligned (4KB), encodes immhi:immlo
// Returns -1 if out of range
int encode_imm21(int64_t val) {
	if (val < -(1LL << 20) || val >= (1LL << 20)) {
		return -1;
	}
	return (int)(val & 0x1FFFFF);
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

// Count trailing zeros in a 64-bit value
static int ctz64(uint64_t val) {
	if (val == 0) {
		return 64;
	}
	int count = 0;
	while ((val & 1) == 0) {
		val >>= 1;
		count++;
	}
	return count;
}

// Rotate right a 64-bit value by amount bits
static uint64_t ror64(uint64_t val, int amount) {
	amount &= 63;
	if (amount == 0) {
		return val;
	}
	return (val >> amount) | (val << (64 - amount));
}

// Encode a logical immediate value into N/imms/immr fields
// sf: 1 for 64-bit, 0 for 32-bit
// Returns 1 on success, 0 if not encodable
int encode_logical_imm(int sf, uint64_t val, LogicalImm *out) {
	int width = sf ? 64 : 32;

	// For 32-bit, replicate to 64-bit for uniform handling
	if (!sf) {
		val &= 0xFFFFFFFF;
		val |= val << 32;
	}

	// Cannot encode all-zeros or all-ones
	if (val == 0 || val == ~0ULL) {
		return 0;
	}

	// Find element size: smallest power of 2 where pattern repeats
	int size;
	for (size = 2; size < width; size *= 2) {
		uint64_t mask = (1ULL << size) - 1;
		uint64_t elem = val & mask;
		uint64_t test = val;
		int match = 1;
		for (int i = 0; i < 64; i += size) {
			if ((test & mask) != elem) {
				match = 0;
				break;
			}
			test >>= size;
		}
		if (match) {
			break;
		}
	}
	if (size == width && !sf) {
		size = 64;
	}

	// Extract element
	uint64_t mask = (size == 64) ? ~0ULL : (1ULL << size) - 1;
	uint64_t elem = val & mask;

	// Rotate element to put leading ones at MSB end
	// Find the rotation that makes the element a contiguous run of ones at LSB
	int rotation = ctz64(elem);
	uint64_t rotated = ror64(elem, rotation);
	if (size < 64) {
		rotated &= mask;
	}

	// Count consecutive ones from bit 0
	int ones = 0;
	uint64_t tmp = rotated;
	while (tmp & 1) {
		ones++;
		tmp >>= 1;
	}

	// After the ones, rest should be zeros
	if ((rotated >> ones) != 0) {
		return 0;
	}

	// Validate the encoding
	if (ones == 0 || ones == size) {
		return 0;
	}

	// Encode fields
	int n, imms, immr;

	if (size == 64) {
		n = 1;
		imms = ones - 1;
		immr = (size - rotation) & (size - 1);
	} else {
		n = 0;
		// imms encodes element size in upper bits
		int size_bits;
		switch (size) {
		case 2:
			size_bits = 0b111100;
			break;
		case 4:
			size_bits = 0b111000;
			break;
		case 8:
			size_bits = 0b110000;
			break;
		case 16:
			size_bits = 0b100000;
			break;
		case 32:
			size_bits = 0b000000;
			break;
		default:
			return 0;
		}
		imms = size_bits | (ones - 1);
		immr = (size - rotation) & (size - 1);
	}

	out->n = n;
	out->imms = imms;
	out->immr = immr;
	return 1;
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

// BR Xn (branch to register)
uint32_t encode_br(int rn) {
	return 0xD61F0000 | ((uint32_t)rn << 5);
}

// LDR Xt, [Xn, #imm] (unsigned offset, scaled)
// sf: 1 for 64-bit, 0 for 32-bit
uint32_t encode_ldr_uoff(int sf, int rt, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm12 = encode_imm12_unsigned(imm, scale);
	if (imm12 < 0) {
		error("load offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF9400000 : 0xB9400000;
	return base | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// STR Xt, [Xn, #imm] (unsigned offset, scaled)
uint32_t encode_str_uoff(int sf, int rt, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm12 = encode_imm12_unsigned(imm, scale);
	if (imm12 < 0) {
		error("store offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF9000000 : 0xB9000000;
	return base | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// LDRB Wt, [Xn, #imm] (unsigned offset, unscaled)
uint32_t encode_ldrb_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 0);
	if (imm12 < 0) {
		error("load offset out of range: %lld", (long long)imm);
	}
	return 0x39400000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// STRB Wt, [Xn, #imm] (unsigned offset, unscaled)
uint32_t encode_strb_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 0);
	if (imm12 < 0) {
		error("store offset out of range: %lld", (long long)imm);
	}
	return 0x39000000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRH Wt, [Xn, #imm] (unsigned offset, scaled by 2)
uint32_t encode_ldrh_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 1);
	if (imm12 < 0) {
		error("load offset out of range or misaligned: %lld", (long long)imm);
	}
	return 0x79400000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// STRH Wt, [Xn, #imm] (unsigned offset, scaled by 2)
uint32_t encode_strh_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 1);
	if (imm12 < 0) {
		error("store offset out of range or misaligned: %lld", (long long)imm);
	}
	return 0x79000000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRSW Xt, [Xn, #imm] (signed word load, unsigned offset, scaled by 4)
uint32_t encode_ldrsw_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 2);
	if (imm12 < 0) {
		error("load offset out of range or misaligned: %lld", (long long)imm);
	}
	return 0xB9800000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRSB Xt, [Xn, #imm] (signed byte load to 64-bit, unsigned offset)
uint32_t encode_ldrsb64_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 0);
	if (imm12 < 0) {
		error("load offset out of range: %lld", (long long)imm);
	}
	return 0x39800000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRSB Wt, [Xn, #imm] (signed byte load to 32-bit, unsigned offset)
uint32_t encode_ldrsb32_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 0);
	if (imm12 < 0) {
		error("load offset out of range: %lld", (long long)imm);
	}
	return 0x39C00000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRSH Xt, [Xn, #imm] (signed halfword load to 64-bit, unsigned offset)
uint32_t encode_ldrsh64_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 1);
	if (imm12 < 0) {
		error("load offset out of range or misaligned: %lld", (long long)imm);
	}
	return 0x79800000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDRSH Wt, [Xn, #imm] (signed halfword load to 32-bit, unsigned offset)
uint32_t encode_ldrsh32_uoff(int rt, int rn, int64_t imm) {
	int imm12 = encode_imm12_unsigned(imm, 1);
	if (imm12 < 0) {
		error("load offset out of range or misaligned: %lld", (long long)imm);
	}
	return 0x79C00000 | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDR Xt, [Xn, #imm]! (pre-index)
uint32_t encode_ldr_pre(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("pre-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8400C00 : 0xB8400C00;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// STR Xt, [Xn, #imm]! (pre-index)
uint32_t encode_str_pre(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("pre-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8000C00 : 0xB8000C00;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// LDR Xt, [Xn], #imm (post-index)
uint32_t encode_ldr_post(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("post-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8400400 : 0xB8400400;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// STR Xt, [Xn], #imm (post-index)
uint32_t encode_str_post(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("post-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8000400 : 0xB8000400;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// STUR Xt, [Xn, #imm] (unscaled offset)
uint32_t encode_stur(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8000000 : 0xB8000000;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// LDUR Xt, [Xn, #imm] (unscaled offset)
uint32_t encode_ldur(int sf, int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xF8400000 : 0xB8400000;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)rt;
}

// STUR Dt/St, [Xn, #imm] (unscaled FP store)
uint32_t encode_stur_fp(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC000000 : 0xBC000000;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// LDUR Dt/St, [Xn, #imm] (unscaled FP load)
uint32_t encode_ldur_fp(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC400000 : 0xBC400000;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// STURB Wt, [Xn, #imm] (unscaled byte store)
uint32_t encode_sturb(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x38000000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURB Wt, [Xn, #imm] (unscaled byte load)
uint32_t encode_ldurb(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x38400000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// STURH Wt, [Xn, #imm] (unscaled halfword store)
uint32_t encode_sturh(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x78000000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURH Wt, [Xn, #imm] (unscaled halfword load)
uint32_t encode_ldurh(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x78400000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURSW Xt, [Xn, #imm] (unscaled signed word load)
uint32_t encode_ldursw(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0xB8800000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURSB Xt, [Xn, #imm] (unscaled signed byte load to 64-bit)
uint32_t encode_ldursb64(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x38800000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURSB Wt, [Xn, #imm] (unscaled signed byte load to 32-bit)
uint32_t encode_ldursb32(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x38C00000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURSH Xt, [Xn, #imm] (unscaled signed halfword load to 64-bit)
uint32_t encode_ldursh64(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x78800000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// LDURSH Wt, [Xn, #imm] (unscaled signed halfword load to 32-bit)
uint32_t encode_ldursh32(int rt, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("unscaled offset out of range: %lld", (long long)imm);
	}
	return 0x78C00000 | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) |
	       (uint32_t)rt;
}

// STP Xt1, Xt2, [Xn, #imm]! (pre-index pair store)
uint32_t encode_stp_pre(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA9800000 : 0x29800000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// LDP Xt1, Xt2, [Xn], #imm (post-index pair load)
uint32_t encode_ldp_post(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA8C00000 : 0x28C00000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// STP Xt1, Xt2, [Xn, #imm] (signed offset, no writeback)
uint32_t encode_stp_off(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA9000000 : 0x29000000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// LDP Xt1, Xt2, [Xn, #imm] (signed offset, no writeback)
uint32_t encode_ldp_off(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA9400000 : 0x29400000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// STP Xt1, Xt2, [Xn], #imm (post-index pair store)
uint32_t encode_stp_post(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA8800000 : 0x28800000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// LDP Xt1, Xt2, [Xn, #imm]! (pre-index pair load)
uint32_t encode_ldp_pre(int sf, int rt1, int rt2, int rn, int64_t imm) {
	int scale = sf ? 3 : 2;
	int imm7 = encode_imm7(imm, scale);
	if (imm7 < 0) {
		error("pair offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = sf ? 0xA9C00000 : 0x29C00000;
	return base | ((uint32_t)imm7 << 15) | ((uint32_t)rt2 << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rt1;
}

// LDR Xt, label (literal)
// offset is PC-relative in bytes, must be aligned to 4
uint32_t encode_ldr_literal(int sf, int rt, int64_t offset) {
	if (offset & 3) {
		error("literal offset must be 4-byte aligned: %lld", (long long)offset);
	}
	int64_t imm19 = offset >> 2;
	if (imm19 < -(1 << 18) || imm19 >= (1 << 18)) {
		error("literal offset out of range: %lld", (long long)offset);
	}
	uint32_t base = sf ? 0x58000000 : 0x18000000;
	return base | ((uint32_t)(imm19 & 0x7FFFF) << 5) | (uint32_t)rt;
}

// MUL Xd, Xn, Xm (alias for MADD Xd, Xn, Xm, XZR)
uint32_t encode_mul(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9B007C00 : 0x1B007C00;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SDIV Xd, Xn, Xm
uint32_t encode_sdiv(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9AC00C00 : 0x1AC00C00;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// UDIV Xd, Xn, Xm
uint32_t encode_udiv(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9AC00800 : 0x1AC00800;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// MSUB Xd, Xn, Xm, Xa (Xa - Xn*Xm)
uint32_t encode_msub(int sf, int rd, int rn, int rm, int ra) {
	uint32_t base = sf ? 0x9B008000 : 0x1B008000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)ra << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// MADD Xd, Xn, Xm, Xa (Xa + Xn*Xm)
uint32_t encode_madd(int sf, int rd, int rn, int rm, int ra) {
	uint32_t base = sf ? 0x9B000000 : 0x1B000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)ra << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// NEG Xd, Xm (alias for SUB Xd, XZR, Xm)
uint32_t encode_neg(int sf, int rd, int rm) {
	return encode_sub_reg(sf, rd, 31, rm);
}

// NEGS Xd, Xm (alias for SUBS Xd, XZR, Xm)
uint32_t encode_negs(int sf, int rd, int rm) {
	uint32_t base = sf ? 0xEB0003E0 : 0x6B0003E0;
	return base | ((uint32_t)rm << 16) | (uint32_t)rd;
}

// SUBS Xd, Xn, Xm (register form, sets flags)
uint32_t encode_subs_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xEB000000 : 0x6B000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SUBS Xd, Xn, #imm (immediate form, sets flags)
uint32_t encode_subs_imm(int sf, int rd, int rn, int imm12, int shift) {
	uint32_t base = sf ? 0xF1000000 : 0x71000000;
	uint32_t sh = shift ? 1 : 0;
	return base | (sh << 22) | ((uint32_t)(imm12 & 0xFFF) << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ADDS Xd, Xn, Xm (register form, sets flags)
uint32_t encode_adds_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xAB000000 : 0x2B000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ADDS Xd, Xn, #imm (immediate form, sets flags)
uint32_t encode_adds_imm(int sf, int rd, int rn, int imm12, int shift) {
	uint32_t base = sf ? 0xB1000000 : 0x31000000;
	uint32_t sh = shift ? 1 : 0;
	return base | (sh << 22) | ((uint32_t)(imm12 & 0xFFF) << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// AND Xd, Xn, Xm (register form)
uint32_t encode_and_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x8A000000 : 0x0A000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ORR Xd, Xn, Xm (register form)
uint32_t encode_orr_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xAA000000 : 0x2A000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// EOR Xd, Xn, Xm (register form)
uint32_t encode_eor_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xCA000000 : 0x4A000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// MVN Xd, Xm (alias for ORN Xd, XZR, Xm)
uint32_t encode_mvn(int sf, int rd, int rm) {
	uint32_t base = sf ? 0xAA2003E0 : 0x2A2003E0;
	return base | ((uint32_t)rm << 16) | (uint32_t)rd;
}

// BIC Xd, Xn, Xm (bitwise clear)
uint32_t encode_bic(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x8A200000 : 0x0A200000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ANDS Xd, Xn, Xm (sets flags)
uint32_t encode_ands_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0xEA000000 : 0x6A000000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// TST Xn, Xm (alias for ANDS XZR, Xn, Xm)
uint32_t encode_tst_reg(int sf, int rn, int rm) {
	return encode_ands_reg(sf, 31, rn, rm);
}

// AND Xd, Xn, #imm (immediate form)
// Opcodes: 0x92400000 (64-bit) / 0x12000000 (32-bit)
uint32_t encode_and_imm(int sf, int rd, int rn, int n, int imms, int immr) {
	uint32_t base = sf ? 0x92000000 : 0x12000000;
	return base | ((uint32_t)n << 22) | ((uint32_t)immr << 16) |
	       ((uint32_t)imms << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ORR Xd, Xn, #imm (immediate form)
// Opcodes: 0xB2400000 (64-bit) / 0x32000000 (32-bit)
uint32_t encode_orr_imm(int sf, int rd, int rn, int n, int imms, int immr) {
	uint32_t base = sf ? 0xB2000000 : 0x32000000;
	return base | ((uint32_t)n << 22) | ((uint32_t)immr << 16) |
	       ((uint32_t)imms << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// EOR Xd, Xn, #imm (immediate form)
// Opcodes: 0xD2400000 (64-bit) / 0x52000000 (32-bit)
uint32_t encode_eor_imm(int sf, int rd, int rn, int n, int imms, int immr) {
	uint32_t base = sf ? 0xD2000000 : 0x52000000;
	return base | ((uint32_t)n << 22) | ((uint32_t)immr << 16) |
	       ((uint32_t)imms << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ANDS Xd, Xn, #imm (immediate form, sets flags)
// Opcodes: 0xF2400000 (64-bit) / 0x72000000 (32-bit)
uint32_t encode_ands_imm(int sf, int rd, int rn, int n, int imms, int immr) {
	uint32_t base = sf ? 0xF2000000 : 0x72000000;
	return base | ((uint32_t)n << 22) | ((uint32_t)immr << 16) |
	       ((uint32_t)imms << 10) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// TST Xn, #imm (alias for ANDS XZR, Xn, #imm)
uint32_t encode_tst_imm(int sf, int rn, int n, int imms, int immr) {
	return encode_ands_imm(sf, 31, rn, n, imms, immr);
}

// LSL Xd, Xn, Xm (LSLV, variable shift)
uint32_t encode_lsl_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9AC02000 : 0x1AC02000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// LSR Xd, Xn, Xm (LSRV, variable shift)
uint32_t encode_lsr_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9AC02400 : 0x1AC02400;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ASR Xd, Xn, Xm (ASRV, variable shift)
uint32_t encode_asr_reg(int sf, int rd, int rn, int rm) {
	uint32_t base = sf ? 0x9AC02800 : 0x1AC02800;
	return base | ((uint32_t)rm << 16) | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// LSL Xd, Xn, #shift (alias for UBFM Xd, Xn, #(-shift MOD 64), #(63-shift))
// sf: 1 for 64-bit, 0 for 32-bit
uint32_t encode_lsl_imm(int sf, int rd, int rn, int shift) {
	int width = sf ? 64 : 32;
	int immr = (-shift) & (width - 1);
	int imms = width - 1 - shift;
	uint32_t base = sf ? 0xD3400000 : 0x53000000;
	return base | ((uint32_t)immr << 16) | ((uint32_t)imms << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// LSR Xd, Xn, #shift (alias for UBFM Xd, Xn, #shift, #63)
uint32_t encode_lsr_imm(int sf, int rd, int rn, int shift) {
	int imms = sf ? 63 : 31;
	uint32_t base = sf ? 0xD3400000 : 0x53000000;
	return base | ((uint32_t)shift << 16) | ((uint32_t)imms << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ASR Xd, Xn, #shift (alias for SBFM Xd, Xn, #shift, #63)
uint32_t encode_asr_imm(int sf, int rd, int rn, int shift) {
	int imms = sf ? 63 : 31;
	uint32_t base = sf ? 0x93400000 : 0x13000000;
	return base | ((uint32_t)shift << 16) | ((uint32_t)imms << 10) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// CMP Xn, Xm (alias for SUBS XZR, Xn, Xm)
uint32_t encode_cmp_reg(int sf, int rn, int rm) {
	return encode_subs_reg(sf, 31, rn, rm);
}

// CMP Xn, #imm (alias for SUBS XZR, Xn, #imm)
uint32_t encode_cmp_imm(int sf, int rn, int imm12, int shift) {
	return encode_subs_imm(sf, 31, rn, imm12, shift);
}

// CMN Xn, Xm (alias for ADDS XZR, Xn, Xm)
uint32_t encode_cmn_reg(int sf, int rn, int rm) {
	return encode_adds_reg(sf, 31, rn, rm);
}

// CMN Xn, #imm (alias for ADDS XZR, Xn, #imm)
uint32_t encode_cmn_imm(int sf, int rn, int imm12, int shift) {
	return encode_adds_imm(sf, 31, rn, imm12, shift);
}

// CSET Xd, cond (alias for CSINC Xd, XZR, XZR, invert(cond))
uint32_t encode_cset(int sf, int rd, int cond) {
	int inv_cond = invert_cond(cond);
	uint32_t base = sf ? 0x9A9F07E0 : 0x1A9F07E0;
	return base | ((uint32_t)inv_cond << 12) | (uint32_t)rd;
}

// CSETM Xd, cond (alias for CSINV Xd, XZR, XZR, invert(cond))
uint32_t encode_csetm(int sf, int rd, int cond) {
	int inv_cond = invert_cond(cond);
	uint32_t base = sf ? 0xDA9F03E0 : 0x5A9F03E0;
	return base | ((uint32_t)inv_cond << 12) | (uint32_t)rd;
}

// CSINC Xd, Xn, Xm, cond
uint32_t encode_csinc(int sf, int rd, int rn, int rm, int cond) {
	uint32_t base = sf ? 0x9A800400 : 0x1A800400;
	return base | ((uint32_t)rm << 16) | ((uint32_t)cond << 12) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// CSEL Xd, Xn, Xm, cond
uint32_t encode_csel(int sf, int rd, int rn, int rm, int cond) {
	uint32_t base = sf ? 0x9A800000 : 0x1A800000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)cond << 12) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// CSINV Xd, Xn, Xm, cond
uint32_t encode_csinv(int sf, int rd, int rn, int rm, int cond) {
	uint32_t base = sf ? 0xDA800000 : 0x5A800000;
	return base | ((uint32_t)rm << 16) | ((uint32_t)cond << 12) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// CSNEG Xd, Xn, Xm, cond
uint32_t encode_csneg(int sf, int rd, int rn, int rm, int cond) {
	uint32_t base = sf ? 0xDA800400 : 0x5A800400;
	return base | ((uint32_t)rm << 16) | ((uint32_t)cond << 12) |
	       ((uint32_t)rn << 5) | (uint32_t)rd;
}

// CINC Xd, Xn, cond (alias for CSINC Xd, Xn, Xn, invert(cond))
uint32_t encode_cinc(int sf, int rd, int rn, int cond) {
	int inv_cond = invert_cond(cond);
	return encode_csinc(sf, rd, rn, rn, inv_cond);
}

// SXTB Xd, Wn (sign-extend byte to 64-bit)
// SBFM Xd, Xn, #0, #7
uint32_t encode_sxtb(int rd, int rn) {
	return 0x93401C00 | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SXTH Xd, Wn (sign-extend halfword to 64-bit)
// SBFM Xd, Xn, #0, #15
uint32_t encode_sxth(int rd, int rn) {
	return 0x93403C00 | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// SXTW Xd, Wn (sign-extend word to 64-bit)
// SBFM Xd, Xn, #0, #31
uint32_t encode_sxtw(int rd, int rn) {
	return 0x93407C00 | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// UXTB Wd, Wn (zero-extend byte to 32-bit)
// UBFM Wd, Wn, #0, #7
uint32_t encode_uxtb(int rd, int rn) {
	return 0x53001C00 | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// UXTH Wd, Wn (zero-extend halfword to 32-bit)
// UBFM Wd, Wn, #0, #15
uint32_t encode_uxth(int rd, int rn) {
	return 0x53003C00 | ((uint32_t)rn << 5) | (uint32_t)rd;
}

// ADRP Xd, label (page address)
// immhi:immlo encode the 21-bit page offset
uint32_t encode_adrp(int rd, int64_t offset) {
	int64_t imm = offset >> 12;
	if (imm < -(1LL << 20) || imm >= (1LL << 20)) {
		error("ADRP offset out of range: %lld", (long long)offset);
	}
	uint32_t immlo = (imm & 0x3) << 29;
	uint32_t immhi = ((imm >> 2) & 0x7FFFF) << 5;
	return 0x90000000 | immlo | immhi | (uint32_t)rd;
}

// ADR Xd, label (PC-relative address)
uint32_t encode_adr(int rd, int64_t offset) {
	if (offset < -(1LL << 20) || offset >= (1LL << 20)) {
		error("ADR offset out of range: %lld", (long long)offset);
	}
	uint32_t immlo = (offset & 0x3) << 29;
	uint32_t immhi = ((offset >> 2) & 0x7FFFF) << 5;
	return 0x10000000 | immlo | immhi | (uint32_t)rd;
}

// NOP (no operation)
uint32_t encode_nop(void) {
	return 0xD503201F;
}

// FMOV Dd, Xn or FMOV Sd, Wn (GPR to FP register)
// sf=1: 64-bit (X to D), sf=0: 32-bit (W to S)
uint32_t encode_fmov_gpr_to_fpr(int sf, int fd, int rn) {
	uint32_t base = sf ? 0x9E670000 : 0x1E270000;
	return base | ((uint32_t)rn << 5) | (uint32_t)fd;
}

// FADD Dd, Dn, Dm or FADD Sd, Sn, Sm
// ftype=1: double, ftype=0: single
uint32_t encode_fadd(int ftype, int fd, int fn, int fm) {
	uint32_t base = ftype ? 0x1E602800 : 0x1E202800;
	return base | ((uint32_t)fm << 16) | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FSUB Dd, Dn, Dm or FSUB Sd, Sn, Sm
uint32_t encode_fsub(int ftype, int fd, int fn, int fm) {
	uint32_t base = ftype ? 0x1E603800 : 0x1E203800;
	return base | ((uint32_t)fm << 16) | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FMUL Dd, Dn, Dm or FMUL Sd, Sn, Sm
uint32_t encode_fmul(int ftype, int fd, int fn, int fm) {
	uint32_t base = ftype ? 0x1E600800 : 0x1E200800;
	return base | ((uint32_t)fm << 16) | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FDIV Dd, Dn, Dm or FDIV Sd, Sn, Sm
uint32_t encode_fdiv(int ftype, int fd, int fn, int fm) {
	uint32_t base = ftype ? 0x1E601800 : 0x1E201800;
	return base | ((uint32_t)fm << 16) | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FNEG Dd, Dn or FNEG Sd, Sn
uint32_t encode_fneg(int ftype, int fd, int fn) {
	uint32_t base = ftype ? 0x1E614000 : 0x1E214000;
	return base | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FCMP Dn, Dm or FCMP Sn, Sm (register)
uint32_t encode_fcmp_reg(int ftype, int fn, int fm) {
	uint32_t base = ftype ? 0x1E602000 : 0x1E202000;
	return base | ((uint32_t)fm << 16) | ((uint32_t)fn << 5);
}

// FCMP Dn, #0.0 or FCMP Sn, #0.0 (zero)
uint32_t encode_fcmp_zero(int ftype, int fn) {
	uint32_t base = ftype ? 0x1E602008 : 0x1E202008;
	return base | ((uint32_t)fn << 5);
}

// SCVTF Dd, Xn / SCVTF Sd, Xn / SCVTF Dd, Wn / SCVTF Sd, Wn
// sf: 1 for X register, 0 for W register
// ftype: 1 for D register, 0 for S register
uint32_t encode_scvtf(int sf, int ftype, int fd, int rn) {
	uint32_t base;
	if (sf && ftype) {
		base = 0x9E620000;
	} else if (!sf && ftype) {
		base = 0x1E620000;
	} else if (sf && !ftype) {
		base = 0x9E220000;
	} else {
		base = 0x1E220000;
	}
	return base | ((uint32_t)rn << 5) | (uint32_t)fd;
}

// UCVTF Dd, Xn / UCVTF Sd, Xn / UCVTF Dd, Wn / UCVTF Sd, Wn
uint32_t encode_ucvtf(int sf, int ftype, int fd, int rn) {
	uint32_t base;
	if (sf && ftype) {
		base = 0x9E630000;
	} else if (!sf && ftype) {
		base = 0x1E630000;
	} else if (sf && !ftype) {
		base = 0x9E230000;
	} else {
		base = 0x1E230000;
	}
	return base | ((uint32_t)rn << 5) | (uint32_t)fd;
}

// FCVTZS Xd, Dn / FCVTZS Xd, Sn / FCVTZS Wd, Dn / FCVTZS Wd, Sn
// sf: 1 for X register, 0 for W register
// ftype: 1 for D register, 0 for S register
uint32_t encode_fcvtzs(int sf, int ftype, int rd, int fn) {
	uint32_t base;
	if (sf && ftype) {
		base = 0x9E780000;
	} else if (!sf && ftype) {
		base = 0x1E780000;
	} else if (sf && !ftype) {
		base = 0x9E380000;
	} else {
		base = 0x1E380000;
	}
	return base | ((uint32_t)fn << 5) | (uint32_t)rd;
}

// FCVTZU Xd, Dn / FCVTZU Xd, Sn / FCVTZU Wd, Dn / FCVTZU Wd, Sn
uint32_t encode_fcvtzu(int sf, int ftype, int rd, int fn) {
	uint32_t base;
	if (sf && ftype) {
		base = 0x9E790000;
	} else if (!sf && ftype) {
		base = 0x1E790000;
	} else if (sf && !ftype) {
		base = 0x9E390000;
	} else {
		base = 0x1E390000;
	}
	return base | ((uint32_t)fn << 5) | (uint32_t)rd;
}

// FCVT Dd, Sn (single to double)
uint32_t encode_fcvt_d_s(int fd, int fn) {
	return 0x1E22C000 | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// FCVT Sd, Dn (double to single)
uint32_t encode_fcvt_s_d(int fd, int fn) {
	return 0x1E624000 | ((uint32_t)fn << 5) | (uint32_t)fd;
}

// LDR Dt, [Xn, #imm] or LDR St, [Xn, #imm] (unsigned offset)
// ftype=1: double (scale=3), ftype=0: single (scale=2)
uint32_t encode_ldr_fp_uoff(int ftype, int ft, int rn, int64_t imm) {
	int scale = ftype ? 3 : 2;
	int imm12 = encode_imm12_unsigned(imm, scale);
	if (imm12 < 0) {
		error("FP load offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFD400000 : 0xBD400000;
	return base | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// STR Dt, [Xn, #imm] or STR St, [Xn, #imm] (unsigned offset)
uint32_t encode_str_fp_uoff(int ftype, int ft, int rn, int64_t imm) {
	int scale = ftype ? 3 : 2;
	int imm12 = encode_imm12_unsigned(imm, scale);
	if (imm12 < 0) {
		error("FP store offset out of range or misaligned: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFD000000 : 0xBD000000;
	return base | ((uint32_t)imm12 << 10) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// LDR Dt, [Xn, #imm]! or LDR St, [Xn, #imm]! (pre-index)
uint32_t encode_ldr_fp_pre(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("FP pre-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC400C00 : 0xBC400C00;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// STR Dt, [Xn, #imm]! or STR St, [Xn, #imm]! (pre-index)
uint32_t encode_str_fp_pre(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("FP pre-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC000C00 : 0xBC000C00;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// LDR Dt, [Xn], #imm or LDR St, [Xn], #imm (post-index)
uint32_t encode_ldr_fp_post(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("FP post-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC400400 : 0xBC400400;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// STR Dt, [Xn], #imm or STR St, [Xn], #imm (post-index)
uint32_t encode_str_fp_post(int ftype, int ft, int rn, int64_t imm) {
	int imm9 = encode_imm9(imm);
	if (imm9 < 0) {
		error("FP post-index offset out of range: %lld", (long long)imm);
	}
	uint32_t base = ftype ? 0xFC000400 : 0xBC000400;
	return base | ((uint32_t)imm9 << 12) | ((uint32_t)rn << 5) | (uint32_t)ft;
}

// SVC #imm16 (supervisor call)
uint32_t encode_svc(int imm16) {
	return 0xD4000001 | ((uint32_t)(imm16 & 0xFFFF) << 5);
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

	printf("\nRET/BLR/BR:\n");
	check_encoding("RET (x30)", encode_ret(30), 0xD65F03C0);
	check_encoding("RET x0", encode_ret(0), 0xD65F0000);
	check_encoding("BLR x0", encode_blr(0), 0xD63F0000);
	check_encoding("BLR x30", encode_blr(30), 0xD63F03C0);
	check_encoding("BR x0", encode_br(0), 0xD61F0000);
	check_encoding("BR x16", encode_br(16), 0xD61F0200);

	printf("\nLDR unsigned offset:\n");
	check_encoding("LDR x0, [x1]", encode_ldr_uoff(1, 0, 1, 0), 0xF9400020);
	check_encoding("LDR x0, [x1, #8]", encode_ldr_uoff(1, 0, 1, 8), 0xF9400420);
	check_encoding("LDR x0, [x1, #32760]", encode_ldr_uoff(1, 0, 1, 32760), 0xF97FFC20);
	check_encoding("LDR w0, [x1, #4]", encode_ldr_uoff(0, 0, 1, 4), 0xB9400420);

	printf("\nSTR unsigned offset:\n");
	check_encoding("STR x0, [x1]", encode_str_uoff(1, 0, 1, 0), 0xF9000020);
	check_encoding("STR x0, [x1, #16]", encode_str_uoff(1, 0, 1, 16), 0xF9000820);
	check_encoding("STR w0, [x1, #8]", encode_str_uoff(0, 0, 1, 8), 0xB9000820);

	printf("\nLDRB/STRB unsigned offset:\n");
	check_encoding("LDRB w0, [x1]", encode_ldrb_uoff(0, 1, 0), 0x39400020);
	check_encoding("LDRB w0, [x1, #1]", encode_ldrb_uoff(0, 1, 1), 0x39400420);
	check_encoding("STRB w0, [x1, #2]", encode_strb_uoff(0, 1, 2), 0x39000820);

	printf("\nLDRH/STRH unsigned offset:\n");
	check_encoding("LDRH w0, [x1]", encode_ldrh_uoff(0, 1, 0), 0x79400020);
	check_encoding("LDRH w0, [x1, #2]", encode_ldrh_uoff(0, 1, 2), 0x79400420);
	check_encoding("STRH w0, [x1, #4]", encode_strh_uoff(0, 1, 4), 0x79000820);

	printf("\nLDRSW/LDRSB/LDRSH unsigned offset:\n");
	check_encoding("LDRSW x0, [x1, #4]", encode_ldrsw_uoff(0, 1, 4), 0xB9800420);
	check_encoding("LDRSB x0, [x1, #1]", encode_ldrsb64_uoff(0, 1, 1), 0x39800420);
	check_encoding("LDRSB w0, [x1, #1]", encode_ldrsb32_uoff(0, 1, 1), 0x39C00420);
	check_encoding("LDRSH x0, [x1, #2]", encode_ldrsh64_uoff(0, 1, 2), 0x79800420);
	check_encoding("LDRSH w0, [x1, #2]", encode_ldrsh32_uoff(0, 1, 2), 0x79C00420);

	printf("\nLDR/STR pre-index:\n");
	check_encoding("LDR x0, [x1, #16]!", encode_ldr_pre(1, 0, 1, 16), 0xF8410C20);
	check_encoding("LDR x0, [x1, #-16]!", encode_ldr_pre(1, 0, 1, -16), 0xF85F0C20);
	check_encoding("STR x0, [x1, #-8]!", encode_str_pre(1, 0, 1, -8), 0xF81F8C20);
	check_encoding("LDR w0, [x1, #4]!", encode_ldr_pre(0, 0, 1, 4), 0xB8404C20);
	check_encoding("STR w0, [x1, #-4]!", encode_str_pre(0, 0, 1, -4), 0xB81FCC20);

	printf("\nLDR/STR post-index:\n");
	check_encoding("LDR x0, [x1], #16", encode_ldr_post(1, 0, 1, 16), 0xF8410420);
	check_encoding("LDR x0, [x1], #-16", encode_ldr_post(1, 0, 1, -16), 0xF85F0420);
	check_encoding("STR x0, [x1], #8", encode_str_post(1, 0, 1, 8), 0xF8008420);
	check_encoding("LDR w0, [x1], #4", encode_ldr_post(0, 0, 1, 4), 0xB8404420);
	check_encoding("STR w0, [x1], #-4", encode_str_post(0, 0, 1, -4), 0xB81FC420);

	printf("\nSTUR/LDUR (unscaled):\n");
	check_encoding("STUR x0, [x1, #1]", encode_stur(1, 0, 1, 1), 0xF8001020);
	check_encoding("STUR x0, [x1, #-8]", encode_stur(1, 0, 1, -8), 0xF81F8020);
	check_encoding("LDUR x0, [x1, #3]", encode_ldur(1, 0, 1, 3), 0xF8403020);
	check_encoding("LDUR w0, [x1, #-1]", encode_ldur(0, 0, 1, -1), 0xB85FF020);
	check_encoding("STURB w0, [x1, #5]", encode_sturb(0, 1, 5), 0x38005020);
	check_encoding("LDURB w0, [x1, #-3]", encode_ldurb(0, 1, -3), 0x385FD020);
	check_encoding("STURH w0, [x1, #7]", encode_sturh(0, 1, 7), 0x78007020);
	check_encoding("LDURH w0, [x1, #-5]", encode_ldurh(0, 1, -5), 0x785FB020);
	check_encoding("LDURSW x0, [x1, #-4]", encode_ldursw(0, 1, -4), 0xB89FC020);
	check_encoding("LDURSB x0, [x1, #2]", encode_ldursb64(0, 1, 2), 0x38802020);
	check_encoding("LDURSB w0, [x1, #2]", encode_ldursb32(0, 1, 2), 0x38C02020);
	check_encoding("LDURSH x0, [x1, #-2]", encode_ldursh64(0, 1, -2), 0x789FE020);
	check_encoding("LDURSH w0, [x1, #-2]", encode_ldursh32(0, 1, -2), 0x78DFE020);

	printf("\nSTP/LDP:\n");
	check_encoding("STP x29, x30, [sp, #-16]!", encode_stp_pre(1, 29, 30, 31, -16), 0xA9BF7BFD);
	check_encoding("LDP x29, x30, [sp], #16", encode_ldp_post(1, 29, 30, 31, 16), 0xA8C17BFD);
	check_encoding("STP x0, x1, [x2, #16]", encode_stp_off(1, 0, 1, 2, 16), 0xA9010440);
	check_encoding("LDP x0, x1, [x2, #-16]", encode_ldp_off(1, 0, 1, 2, -16), 0xA97F0440);
	check_encoding("STP w0, w1, [x2], #8", encode_stp_post(0, 0, 1, 2, 8), 0x28810440);
	check_encoding("LDP w0, w1, [x2, #-8]!", encode_ldp_pre(0, 0, 1, 2, -8), 0x29FF0440);

	printf("\nLDR literal:\n");
	check_encoding("LDR x0, .+8", encode_ldr_literal(1, 0, 8), 0x58000040);
	check_encoding("LDR x0, .-4", encode_ldr_literal(1, 0, -4), 0x58FFFFE0);
	check_encoding("LDR w0, .+12", encode_ldr_literal(0, 0, 12), 0x18000060);

	printf("\nMUL/SDIV/UDIV:\n");
	check_encoding("MUL x0, x1, x2", encode_mul(1, 0, 1, 2), 0x9B027C20);
	check_encoding("MUL w0, w1, w2", encode_mul(0, 0, 1, 2), 0x1B027C20);
	check_encoding("SDIV x0, x1, x2", encode_sdiv(1, 0, 1, 2), 0x9AC20C20);
	check_encoding("SDIV w0, w1, w2", encode_sdiv(0, 0, 1, 2), 0x1AC20C20);
	check_encoding("UDIV x0, x1, x2", encode_udiv(1, 0, 1, 2), 0x9AC20820);
	check_encoding("UDIV w0, w1, w2", encode_udiv(0, 0, 1, 2), 0x1AC20820);

	printf("\nMADD/MSUB:\n");
	check_encoding("MADD x0, x1, x2, x3", encode_madd(1, 0, 1, 2, 3), 0x9B020C20);
	check_encoding("MSUB x0, x1, x2, x3", encode_msub(1, 0, 1, 2, 3), 0x9B028C20);

	printf("\nNEG/NEGS:\n");
	check_encoding("NEG x0, x1", encode_neg(1, 0, 1), 0xCB0103E0);
	check_encoding("NEG w0, w1", encode_neg(0, 0, 1), 0x4B0103E0);
	check_encoding("NEGS x0, x1", encode_negs(1, 0, 1), 0xEB0103E0);

	printf("\nSUBS/ADDS register:\n");
	check_encoding("SUBS x0, x1, x2", encode_subs_reg(1, 0, 1, 2), 0xEB020020);
	check_encoding("SUBS w0, w1, w2", encode_subs_reg(0, 0, 1, 2), 0x6B020020);
	check_encoding("ADDS x0, x1, x2", encode_adds_reg(1, 0, 1, 2), 0xAB020020);

	printf("\nSUBS/ADDS immediate:\n");
	check_encoding("SUBS x0, x1, #42", encode_subs_imm(1, 0, 1, 42, 0), 0xF100A820);
	check_encoding("ADDS x0, x1, #42", encode_adds_imm(1, 0, 1, 42, 0), 0xB100A820);

	printf("\nAND/ORR/EOR register:\n");
	check_encoding("AND x0, x1, x2", encode_and_reg(1, 0, 1, 2), 0x8A020020);
	check_encoding("AND w0, w1, w2", encode_and_reg(0, 0, 1, 2), 0x0A020020);
	check_encoding("ORR x0, x1, x2", encode_orr_reg(1, 0, 1, 2), 0xAA020020);
	check_encoding("EOR x0, x1, x2", encode_eor_reg(1, 0, 1, 2), 0xCA020020);

	printf("\nAND/ORR/EOR/ANDS immediate:\n");
	// 0x3FF = 1023 = 10 ones at bit 0, N=1, imms=9, immr=0
	check_encoding("AND x0, x1, #0x3FF", encode_and_imm(1, 0, 1, 1, 9, 0), 0x92402420);
	// 0xFF = 8 ones at bit 0, N=1, imms=7, immr=0
	check_encoding("AND x0, x1, #0xFF", encode_and_imm(1, 0, 1, 1, 7, 0), 0x92401C20);
	check_encoding("ORR x0, x1, #0xFF", encode_orr_imm(1, 0, 1, 1, 7, 0), 0xB2401C20);
	check_encoding("EOR x0, x1, #0xFF", encode_eor_imm(1, 0, 1, 1, 7, 0), 0xD2401C20);
	check_encoding("ANDS x0, x1, #0xFF", encode_ands_imm(1, 0, 1, 1, 7, 0), 0xF2401C20);
	// 32-bit: 0xFF = 8 ones, N=0, imms=7, immr=0
	check_encoding("AND w0, w1, #0xFF", encode_and_imm(0, 0, 1, 0, 7, 0), 0x12001C20);

	printf("\nMVN/BIC:\n");
	check_encoding("MVN x0, x1", encode_mvn(1, 0, 1), 0xAA2103E0);
	check_encoding("MVN w0, w1", encode_mvn(0, 0, 1), 0x2A2103E0);
	check_encoding("BIC x0, x1, x2", encode_bic(1, 0, 1, 2), 0x8A220020);

	printf("\nANDS/TST:\n");
	check_encoding("ANDS x0, x1, x2", encode_ands_reg(1, 0, 1, 2), 0xEA020020);
	check_encoding("TST x1, x2", encode_tst_reg(1, 1, 2), 0xEA02003F);

	printf("\nLSL/LSR/ASR variable:\n");
	check_encoding("LSL x0, x1, x2", encode_lsl_reg(1, 0, 1, 2), 0x9AC22020);
	check_encoding("LSL w0, w1, w2", encode_lsl_reg(0, 0, 1, 2), 0x1AC22020);
	check_encoding("LSR x0, x1, x2", encode_lsr_reg(1, 0, 1, 2), 0x9AC22420);
	check_encoding("ASR x0, x1, x2", encode_asr_reg(1, 0, 1, 2), 0x9AC22820);

	printf("\nLSL/LSR/ASR immediate:\n");
	check_encoding("LSL x0, x1, #5", encode_lsl_imm(1, 0, 1, 5), 0xD37BE820);
	check_encoding("LSL w0, w1, #5", encode_lsl_imm(0, 0, 1, 5), 0x531B6820);
	check_encoding("LSR x0, x1, #5", encode_lsr_imm(1, 0, 1, 5), 0xD345FC20);
	check_encoding("LSR w0, w1, #5", encode_lsr_imm(0, 0, 1, 5), 0x53057C20);
	check_encoding("ASR x0, x1, #5", encode_asr_imm(1, 0, 1, 5), 0x9345FC20);
	check_encoding("ASR w0, w1, #5", encode_asr_imm(0, 0, 1, 5), 0x13057C20);

	printf("\nCMP/CMN:\n");
	check_encoding("CMP x0, x1", encode_cmp_reg(1, 0, 1), 0xEB01001F);
	check_encoding("CMP w0, w1", encode_cmp_reg(0, 0, 1), 0x6B01001F);
	check_encoding("CMP x0, #42", encode_cmp_imm(1, 0, 42, 0), 0xF100A81F);
	check_encoding("CMN x0, x1", encode_cmn_reg(1, 0, 1), 0xAB01001F);
	check_encoding("CMN x0, #42", encode_cmn_imm(1, 0, 42, 0), 0xB100A81F);

	printf("\nCSET/CSETM:\n");
	check_encoding("CSET x0, eq", encode_cset(1, 0, COND_EQ), 0x9A9F17E0);
	check_encoding("CSET w0, eq", encode_cset(0, 0, COND_EQ), 0x1A9F17E0);
	check_encoding("CSET x0, ne", encode_cset(1, 0, COND_NE), 0x9A9F07E0);
	check_encoding("CSET x0, lt", encode_cset(1, 0, COND_LT), 0x9A9FA7E0);
	check_encoding("CSET x0, ge", encode_cset(1, 0, COND_GE), 0x9A9FB7E0);
	check_encoding("CSETM x0, eq", encode_csetm(1, 0, COND_EQ), 0xDA9F13E0);

	printf("\nCSEL/CSINC/CSINV/CSNEG:\n");
	check_encoding("CSEL x0, x1, x2, eq", encode_csel(1, 0, 1, 2, COND_EQ), 0x9A820020);
	check_encoding("CSINC x0, x1, x2, ne", encode_csinc(1, 0, 1, 2, COND_NE), 0x9A821420);
	check_encoding("CSINV x0, x1, x2, lt", encode_csinv(1, 0, 1, 2, COND_LT), 0xDA82B020);
	check_encoding("CSNEG x0, x1, x2, ge", encode_csneg(1, 0, 1, 2, COND_GE), 0xDA82A420);
	check_encoding("CINC x0, x1, eq", encode_cinc(1, 0, 1, COND_EQ), 0x9A811420);

	printf("\nSXTB/SXTH/SXTW:\n");
	check_encoding("SXTB x0, w1", encode_sxtb(0, 1), 0x93401C20);
	check_encoding("SXTH x0, w1", encode_sxth(0, 1), 0x93403C20);
	check_encoding("SXTW x0, w1", encode_sxtw(0, 1), 0x93407C20);

	printf("\nUXTB/UXTH:\n");
	check_encoding("UXTB w0, w1", encode_uxtb(0, 1), 0x53001C20);
	check_encoding("UXTH w0, w1", encode_uxth(0, 1), 0x53003C20);

	printf("\nADRP/ADR:\n");
	check_encoding("ADRP x0, .+4096", encode_adrp(0, 4096), 0xB0000000);
	check_encoding("ADRP x1, .-4096", encode_adrp(1, -4096), 0xF0FFFFE1);
	check_encoding("ADRP x0, .+8192", encode_adrp(0, 8192), 0xD0000000);
	check_encoding("ADR x0, .+4", encode_adr(0, 4), 0x10000020);
	check_encoding("ADR x0, .-4", encode_adr(0, -4), 0x10FFFFE0);

	printf("\nNOP:\n");
	check_encoding("NOP", encode_nop(), 0xD503201F);

	printf("\nFMOV (GPR to FP):\n");
	check_encoding("FMOV d0, x0", encode_fmov_gpr_to_fpr(1, 0, 0), 0x9E670000);
	check_encoding("FMOV d1, x2", encode_fmov_gpr_to_fpr(1, 1, 2), 0x9E670041);
	check_encoding("FMOV s0, w0", encode_fmov_gpr_to_fpr(0, 0, 0), 0x1E270000);
	check_encoding("FMOV s3, w4", encode_fmov_gpr_to_fpr(0, 3, 4), 0x1E270083);

	printf("\nFP Arithmetic (double):\n");
	check_encoding("FADD d0, d0, d1", encode_fadd(1, 0, 0, 1), 0x1E612800);
	check_encoding("FADD d2, d3, d4", encode_fadd(1, 2, 3, 4), 0x1E642862);
	check_encoding("FSUB d0, d0, d1", encode_fsub(1, 0, 0, 1), 0x1E613800);
	check_encoding("FMUL d0, d0, d1", encode_fmul(1, 0, 0, 1), 0x1E610800);
	check_encoding("FDIV d0, d0, d1", encode_fdiv(1, 0, 0, 1), 0x1E611800);
	check_encoding("FNEG d0, d0", encode_fneg(1, 0, 0), 0x1E614000);
	check_encoding("FNEG d1, d2", encode_fneg(1, 1, 2), 0x1E614041);

	printf("\nFP Arithmetic (single):\n");
	check_encoding("FADD s0, s0, s1", encode_fadd(0, 0, 0, 1), 0x1E212800);
	check_encoding("FSUB s0, s0, s1", encode_fsub(0, 0, 0, 1), 0x1E213800);
	check_encoding("FMUL s0, s0, s1", encode_fmul(0, 0, 0, 1), 0x1E210800);
	check_encoding("FDIV s0, s0, s1", encode_fdiv(0, 0, 0, 1), 0x1E211800);
	check_encoding("FNEG s0, s0", encode_fneg(0, 0, 0), 0x1E214000);

	printf("\nFCMP:\n");
	check_encoding("FCMP d0, d1", encode_fcmp_reg(1, 0, 1), 0x1E612000);
	check_encoding("FCMP d2, d3", encode_fcmp_reg(1, 2, 3), 0x1E632040);
	check_encoding("FCMP d0, #0.0", encode_fcmp_zero(1, 0), 0x1E602008);
	check_encoding("FCMP d5, #0.0", encode_fcmp_zero(1, 5), 0x1E6020A8);
	check_encoding("FCMP s0, s1", encode_fcmp_reg(0, 0, 1), 0x1E212000);
	check_encoding("FCMP s0, #0.0", encode_fcmp_zero(0, 0), 0x1E202008);

	printf("\nSCVTF (signed int to FP):\n");
	check_encoding("SCVTF d0, x0", encode_scvtf(1, 1, 0, 0), 0x9E620000);
	check_encoding("SCVTF d1, x2", encode_scvtf(1, 1, 1, 2), 0x9E620041);
	check_encoding("SCVTF d0, w0", encode_scvtf(0, 1, 0, 0), 0x1E620000);
	check_encoding("SCVTF s0, x0", encode_scvtf(1, 0, 0, 0), 0x9E220000);
	check_encoding("SCVTF s0, w0", encode_scvtf(0, 0, 0, 0), 0x1E220000);

	printf("\nUCVTF (unsigned int to FP):\n");
	check_encoding("UCVTF d0, x0", encode_ucvtf(1, 1, 0, 0), 0x9E630000);
	check_encoding("UCVTF d0, w0", encode_ucvtf(0, 1, 0, 0), 0x1E630000);
	check_encoding("UCVTF s0, x0", encode_ucvtf(1, 0, 0, 0), 0x9E230000);
	check_encoding("UCVTF s0, w0", encode_ucvtf(0, 0, 0, 0), 0x1E230000);

	printf("\nFCVTZS (FP to signed int, truncate toward zero):\n");
	check_encoding("FCVTZS x0, d0", encode_fcvtzs(1, 1, 0, 0), 0x9E780000);
	check_encoding("FCVTZS x1, d2", encode_fcvtzs(1, 1, 1, 2), 0x9E780041);
	check_encoding("FCVTZS w0, d0", encode_fcvtzs(0, 1, 0, 0), 0x1E780000);
	check_encoding("FCVTZS x0, s0", encode_fcvtzs(1, 0, 0, 0), 0x9E380000);
	check_encoding("FCVTZS w0, s0", encode_fcvtzs(0, 0, 0, 0), 0x1E380000);

	printf("\nFCVTZU (FP to unsigned int, truncate toward zero):\n");
	check_encoding("FCVTZU x0, d0", encode_fcvtzu(1, 1, 0, 0), 0x9E790000);
	check_encoding("FCVTZU w0, d0", encode_fcvtzu(0, 1, 0, 0), 0x1E790000);
	check_encoding("FCVTZU x0, s0", encode_fcvtzu(1, 0, 0, 0), 0x9E390000);
	check_encoding("FCVTZU w0, s0", encode_fcvtzu(0, 0, 0, 0), 0x1E390000);

	printf("\nFCVT (precision conversion):\n");
	check_encoding("FCVT d0, s0", encode_fcvt_d_s(0, 0), 0x1E22C000);
	check_encoding("FCVT d1, s2", encode_fcvt_d_s(1, 2), 0x1E22C041);
	check_encoding("FCVT s0, d0", encode_fcvt_s_d(0, 0), 0x1E624000);
	check_encoding("FCVT s3, d4", encode_fcvt_s_d(3, 4), 0x1E624083);

	printf("\nFP Load unsigned offset:\n");
	check_encoding("LDR d0, [x0]", encode_ldr_fp_uoff(1, 0, 0, 0), 0xFD400000);
	check_encoding("LDR d0, [x1, #8]", encode_ldr_fp_uoff(1, 0, 1, 8), 0xFD400420);
	check_encoding("LDR d0, [x1, #32760]", encode_ldr_fp_uoff(1, 0, 1, 32760), 0xFD7FFC20);
	check_encoding("LDR s0, [x0]", encode_ldr_fp_uoff(0, 0, 0, 0), 0xBD400000);
	check_encoding("LDR s0, [x1, #4]", encode_ldr_fp_uoff(0, 0, 1, 4), 0xBD400420);

	printf("\nFP Store unsigned offset:\n");
	check_encoding("STR d0, [x0]", encode_str_fp_uoff(1, 0, 0, 0), 0xFD000000);
	check_encoding("STR d0, [x1, #16]", encode_str_fp_uoff(1, 0, 1, 16), 0xFD000820);
	check_encoding("STR s0, [x0]", encode_str_fp_uoff(0, 0, 0, 0), 0xBD000000);
	check_encoding("STR s0, [x1, #8]", encode_str_fp_uoff(0, 0, 1, 8), 0xBD000820);

	printf("\nFP Load/Store pre-index:\n");
	check_encoding("LDR d0, [x1, #16]!", encode_ldr_fp_pre(1, 0, 1, 16), 0xFC410C20);
	check_encoding("LDR d0, [x1, #-16]!", encode_ldr_fp_pre(1, 0, 1, -16), 0xFC5F0C20);
	check_encoding("STR d0, [sp, #-16]!", encode_str_fp_pre(1, 0, 31, -16), 0xFC1F0FE0);
	check_encoding("LDR s0, [x1, #8]!", encode_ldr_fp_pre(0, 0, 1, 8), 0xBC408C20);
	check_encoding("STR s0, [x1, #-8]!", encode_str_fp_pre(0, 0, 1, -8), 0xBC1F8C20);

	printf("\nFP Load/Store post-index:\n");
	check_encoding("LDR d0, [x1], #16", encode_ldr_fp_post(1, 0, 1, 16), 0xFC410420);
	check_encoding("LDR d0, [x1], #-16", encode_ldr_fp_post(1, 0, 1, -16), 0xFC5F0420);
	check_encoding("STR d0, [x1], #8", encode_str_fp_post(1, 0, 1, 8), 0xFC008420);
	check_encoding("LDR s0, [x1], #4", encode_ldr_fp_post(0, 0, 1, 4), 0xBC404420);
	check_encoding("STR s0, [x1], #-4", encode_str_fp_post(0, 0, 1, -4), 0xBC1FC420);

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
	printf("  encode_imm9(255) = 0x%X\n", encode_imm9(255));
	printf("  encode_imm9(-256) = 0x%X\n", encode_imm9(-256));
	printf("  encode_imm9(256) = %d (invalid)\n", encode_imm9(256));
	printf("  encode_imm7(-64, 3) = 0x%X\n", encode_imm7(-64, 3));
	printf("  encode_imm7(504, 3) = 0x%X\n", encode_imm7(504, 3));
	printf("  encode_imm12_unsigned(32760, 3) = 0x%X\n", encode_imm12_unsigned(32760, 3));
	printf("  invert_cond(COND_EQ) = %d (expected 1)\n", invert_cond(COND_EQ));
	printf("  invert_cond(COND_LT) = %d (expected 10)\n", invert_cond(COND_LT));

	printf("\nBranch offset encoding:\n");
	printf("  encode_branch26(8) = 0x%X\n", encode_branch26(8));
	printf("  encode_branch26(-4) = 0x%X\n", encode_branch26(-4));
	printf("  encode_branch19(12) = 0x%X\n", encode_branch19(12));
	printf("  encode_branch19(-8) = 0x%X\n", encode_branch19(-8));

	printf("\nLogical immediate encoding:\n");
	LogicalImm imm;
	// 0xFF = 8 ones at bit 0
	int ok = encode_logical_imm(1, 0xFF, &imm);
	printf("  0xFF: ok=%d n=%d imms=%d immr=%d\n", ok, imm.n, imm.imms, imm.immr);
	// 0x3FF = 10 ones at bit 0
	ok = encode_logical_imm(1, 0x3FF, &imm);
	printf("  0x3FF: ok=%d n=%d imms=%d immr=%d\n", ok, imm.n, imm.imms, imm.immr);
	// 0x5555555555555555 = alternating 01 pattern
	ok = encode_logical_imm(1, 0x5555555555555555ULL, &imm);
	printf("  0x5555...5555: ok=%d n=%d imms=%d immr=%d\n", ok, imm.n, imm.imms, imm.immr);
	// All ones - not encodable
	ok = encode_logical_imm(1, ~0ULL, &imm);
	printf("  ~0: ok=%d (should be 0)\n", ok);
	// All zeros - not encodable
	ok = encode_logical_imm(1, 0, &imm);
	printf("  0: ok=%d (should be 0)\n", ok);

	printf("\n========================================\n");
	printf("Results: %d/%d tests passed\n", test_pass, test_count);

	if (test_pass != test_count) {
		exit(1);
	}
}
