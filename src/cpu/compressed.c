#include "cpu.h"

#define OP_LUI     0x37
#define OP_AUIPC   0x17
#define OP_JAL     0x6F
#define OP_JALR    0x67
#define OP_BRANCH  0x63
#define OP_LOAD    0x03
#define OP_STORE   0x23
#define OP_OPIMM   0x13
#define OP_OP      0x33
#define OP_OPIMM32 0x1B
#define OP_OP32    0x3B
#define OP_SYSTEM  0x73
#define OP_LOADFP  0x07
#define OP_STOREFP 0x27

static inline u32 enc_r(u32 f7, u32 rs2, u32 rs1, u32 f3, u32 rd, u32 op) {
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
static inline u32 enc_i(s32 imm, u32 rs1, u32 f3, u32 rd, u32 op) {
    return (((u32)imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
static inline u32 enc_s(s32 imm, u32 rs2, u32 rs1, u32 f3, u32 op) {
    u32 u = (u32)imm & 0xFFF;
    return ((u >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((u & 0x1F) << 7) | op;
}
static inline u32 enc_b(s32 imm, u32 rs2, u32 rs1, u32 f3, u32 op) {
    u32 u = (u32)imm & 0x1FFF;
    u32 b12 = (u >> 12) & 1, b11 = (u >> 11) & 1, b10_5 = (u >> 5) & 0x3F, b4_1 = (u >> 1) & 0xF;
    return (b12 << 31) | (b10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (b4_1 << 8) |
           (b11 << 7) | op;
}
static inline u32 enc_j(s32 imm, u32 rd, u32 op) {
    u32 u = (u32)imm & 0x1FFFFF;
    u32 b20 = (u >> 20) & 1, b19_12 = (u >> 12) & 0xFF, b11 = (u >> 11) & 1,
        b10_1 = (u >> 1) & 0x3FF;
    return (b20 << 31) | (b19_12 << 12) | (b11 << 20) | (b10_1 << 21) | (rd << 7) | op;
}
static inline u32 enc_u(s32 imm, u32 rd, u32 op) {
    return ((u32)imm & 0xFFFFF000u) | (rd << 7) | op;
}

static inline u32 q(u16 c) {
    return c & 3;
}
static inline u32 f3(u16 c) {
    return (c >> 13) & 7;
}
static inline u32 crd(u16 c) {
    return (c >> 7) & 0x1F;
}
static inline u32 crs2(u16 c) {
    return (c >> 2) & 0x1F;
}
static inline u32 prd(u16 c) {
    return 8 + ((c >> 2) & 7);
}
static inline u32 prs1(u16 c) {
    return 8 + ((c >> 7) & 7);
}

static inline s32 sx(u32 v, u32 bits) {
    return (s32)(v << (32 - bits)) >> (32 - bits);
}

static inline s32 imm_ci(u16 c) {
    return sx((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F), 6);
}

static inline s32 imm_cj(u16 c) {
    u32 v = (((c >> 12) & 1) << 11) | (((c >> 11) & 1) << 4) | (((c >> 9) & 3) << 8) |
            (((c >> 8) & 1) << 10) | (((c >> 7) & 1) << 6) | (((c >> 6) & 1) << 7) |
            (((c >> 3) & 7) << 1) | (((c >> 2) & 1) << 5);
    return sx(v, 12);
}

static inline s32 imm_cb(u16 c) {
    u32 v = (((c >> 12) & 1) << 8) | (((c >> 10) & 3) << 3) | (((c >> 5) & 3) << 6) |
            (((c >> 3) & 3) << 1) | (((c >> 2) & 1) << 5);
    return sx(v, 9);
}

static inline s32 imm_16sp(u16 c) {
    u32 v = (((c >> 12) & 1) << 9) | (((c >> 6) & 1) << 4) | (((c >> 5) & 1) << 6) |
            (((c >> 3) & 3) << 7) | (((c >> 2) & 1) << 5);
    return sx(v, 10);
}

static inline s32 imm_clui(u16 c) {
    s32 v = sx((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F), 6);
    return (s32)((u32)v << 12);
}

static inline u32 imm_4spn(u16 c) {
    return (((c >> 7) & 0xF) << 6) | (((c >> 11) & 3) << 4) | (((c >> 5) & 1) << 3) |
           (((c >> 6) & 1) << 2);
}

static inline u32 off_clw(u16 c) {
    return (((c >> 10) & 7) << 3) | (((c >> 6) & 1) << 2) | (((c >> 5) & 1) << 6);
}
static inline u32 off_cld(u16 c) {
    return (((c >> 10) & 7) << 3) | (((c >> 5) & 3) << 6);
}
static inline u32 off_clwsp(u16 c) {
    return (((c >> 12) & 1) << 5) | (((c >> 4) & 7) << 2) | (((c >> 2) & 3) << 6);
}
static inline u32 off_cldsp(u16 c) {
    return (((c >> 12) & 1) << 5) | (((c >> 5) & 3) << 3) | (((c >> 2) & 7) << 6);
}
static inline u32 off_cswsp(u16 c) {
    return (((c >> 9) & 0xF) << 2) | (((c >> 7) & 3) << 6);
}
static inline u32 off_csdsp(u16 c) {
    return (((c >> 10) & 7) << 3) | (((c >> 7) & 7) << 6);
}

u32 c_expand(u16 c, bool *illegal) {
    *illegal = false;

    switch (q(c)) {
    case 0:
        switch (f3(c)) {
        case 0x0: {
            u32 nzuimm = imm_4spn(c);
            if (nzuimm == 0) {
                *illegal = true;
                return 0;
            }
            return enc_i((s32)nzuimm, 2, 0, prd(c), OP_OPIMM);
        }
        case 0x1:
            return enc_i((s32)off_cld(c), prs1(c), 3, prd(c), OP_LOADFP);
        case 0x2:
            return enc_i((s32)off_clw(c), prs1(c), 2, prd(c), OP_LOAD);
        case 0x3:
            return enc_i((s32)off_cld(c), prs1(c), 3, prd(c), OP_LOAD);
        case 0x5:
            return enc_s((s32)off_cld(c), prd(c), prs1(c), 3, OP_STOREFP);
        case 0x6:
            return enc_s((s32)off_clw(c), prd(c), prs1(c), 2, OP_STORE);
        case 0x7:
            return enc_s((s32)off_cld(c), prd(c), prs1(c), 3, OP_STORE);
        default:
            *illegal = true;
            return 0;
        }

    case 1:
        switch (f3(c)) {
        case 0x0:
            return enc_i(imm_ci(c), crd(c), 0, crd(c), OP_OPIMM);
        case 0x1:
            if (crd(c) == 0) {
                *illegal = true;
                return 0;
            }
            return enc_i(imm_ci(c), crd(c), 0, crd(c), OP_OPIMM32);
        case 0x2:
            return enc_i(imm_ci(c), 0, 0, crd(c), OP_OPIMM);
        case 0x3:
            if (crd(c) == 2) {
                s32 imm = imm_16sp(c);
                if (imm == 0) {
                    *illegal = true;
                    return 0;
                }
                return enc_i(imm, 2, 0, 2, OP_OPIMM);
            }
            if (crd(c) == 0) {
                *illegal = true;
                return 0;
            }
            {
                s32 imm = imm_clui(c);
                if (imm == 0) {
                    *illegal = true;
                    return 0;
                }
                return enc_u(imm, crd(c), OP_LUI);
            }
        case 0x4: {
            u32 sub = (c >> 10) & 3;
            u32 rd = prs1(c), rs2 = prd(c);
            if (sub == 0x0) {
                u32 sh = ((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F));
                return enc_i((s32)sh, rd, 5, rd, OP_OPIMM);
            }
            if (sub == 0x1) {
                u32 sh = ((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F));
                return enc_i((s32)(0x400 | sh), rd, 5, rd, OP_OPIMM);
            }
            if (sub == 0x2) {
                s32 imm = sx((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F), 6);
                return enc_i(imm, rd, 7, rd, OP_OPIMM);
            }
            u32 hi = (c >> 12) & 1;
            u32 op2 = (c >> 5) & 3;
            if (hi == 0) {
                switch (op2) {
                case 0:
                    return enc_r(0x20, rs2, rd, 0, rd, OP_OP);
                case 1:
                    return enc_r(0x00, rs2, rd, 4, rd, OP_OP);
                case 2:
                    return enc_r(0x00, rs2, rd, 6, rd, OP_OP);
                case 3:
                    return enc_r(0x00, rs2, rd, 7, rd, OP_OP);
                }
            } else {
                switch (op2) {
                case 0:
                    return enc_r(0x20, rs2, rd, 0, rd, OP_OP32);
                case 1:
                    return enc_r(0x00, rs2, rd, 0, rd, OP_OP32);
                default:
                    *illegal = true;
                    return 0;
                }
            }
            *illegal = true;
            return 0;
        }
        case 0x5:
            return enc_j(imm_cj(c), 0, OP_JAL);
        case 0x6:
            return enc_b(imm_cb(c), 0, prs1(c), 0, OP_BRANCH);
        case 0x7:
            return enc_b(imm_cb(c), 0, prs1(c), 1, OP_BRANCH);
        default:
            *illegal = true;
            return 0;
        }

    case 2:
        switch (f3(c)) {
        case 0x0: {
            u32 sh = ((((c >> 12) & 1) << 5) | ((c >> 2) & 0x1F));
            if (crd(c) == 0)
                return enc_r(0, 0, 0, 0, 0, OP_OPIMM);
            return enc_i((s32)sh, crd(c), 1, crd(c), OP_OPIMM);
        }
        case 0x1:
            if (crd(c) == 0) {
                *illegal = true;
                return 0;
            }
            return enc_i((s32)off_cldsp(c), 2, 3, crd(c), OP_LOADFP);
        case 0x2:
            if (crd(c) == 0) {
                *illegal = true;
                return 0;
            }
            return enc_i((s32)off_clwsp(c), 2, 2, crd(c), OP_LOAD);
        case 0x3:
            if (crd(c) == 0) {
                *illegal = true;
                return 0;
            }
            return enc_i((s32)off_cldsp(c), 2, 3, crd(c), OP_LOAD);
        case 0x4: {
            u32 bit12 = (c >> 12) & 1;
            u32 rd = crd(c), rs2 = crs2(c);
            if (bit12 == 0) {
                if (rs2 == 0) {
                    if (rd == 0) {
                        *illegal = true;
                        return 0;
                    }
                    return enc_i(0, rd, 0, 0, OP_JALR);
                }
                if (rd == 0)
                    return enc_r(0, rs2, 0, 0, 0, OP_OP);
                return enc_r(0, rs2, 0, 0, rd, OP_OP);
            }
            if (rd == 0 && rs2 == 0)
                return 0x00100073u;
            if (rs2 == 0) {
                return enc_i(0, rd, 0, 1, OP_JALR);
            }
            if (rd == 0)
                return enc_r(0, rs2, 0, 0, 0, OP_OP);
            return enc_r(0, rs2, rd, 0, rd, OP_OP);
        }
        case 0x5:
            return enc_s((s32)off_csdsp(c), crs2(c), 2, 3, OP_STOREFP);
        case 0x6:
            return enc_s((s32)off_cswsp(c), crs2(c), 2, 2, OP_STORE);
        case 0x7:
            return enc_s((s32)off_csdsp(c), crs2(c), 2, 3, OP_STORE);
        default:
            *illegal = true;
            return 0;
        }
    }
    *illegal = true;
    return 0;
}
