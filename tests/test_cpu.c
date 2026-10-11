#include "fixtures/harness.h"
#include "test.h"

void test_cpu_alu(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);
    th_emit(&t, ADD(X_A0, X_T0, X_T1));
    th_emit(&t, SUB(X_A1, X_T1, X_T0));
    th_emit(&t, AND(X_A2, X_T2, X_T0));
    th_emit(&t, OR(X_A3, X_T2, X_T0));
    th_emit(&t, XOR(X_A4, X_T2, X_T0));
    th_emit(&t, SLT(X_A5, X_S0, X_S1));
    th_emit(&t, SLTU(X_A7, X_S0, X_S1));
    th_emit(&t, SLTU(X_T2, X_S1, X_S0));
    th_emit(&t, ADD(X_ZERO, X_S0, X_S1));
    t.cpu.x[X_T0] = 5;
    t.cpu.x[X_T1] = 7;
    t.cpu.x[X_T2] = 0xF0F0;
    t.cpu.x[X_S0] = (u64)-1;
    t.cpu.x[X_S1] = 1;
    th_run_all(&t);

    CHECK_U64(t.cpu.x[X_A0], 12);
    CHECK_U64(t.cpu.x[X_A1], 2);
    CHECK_U64(t.cpu.x[X_A2], 0xF0F0 & 5);
    CHECK_U64(t.cpu.x[X_A3], 0xF0F0 | 5);
    CHECK_U64(t.cpu.x[X_A4], 0xF0F0 ^ 5);
    CHECK_U64(t.cpu.x[X_A5], 1);
    CHECK_U64(t.cpu.x[X_A7], 0);
    CHECK_U64(t.cpu.x[X_T2], 1);
    CHECK_U64(t.cpu.x[X_ZERO], 0);
    th_free(&t);
}

void test_cpu_imm(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, ADDI(X_A0, X_T0, 100));
        th_emit(&t, ADDI(X_A1, X_T0, -100));
        th_emit(&t, ANDI(X_A2, X_T0, 0x0FF));
        th_emit(&t, ORI(X_A3, X_T0, 0x00F));
        th_emit(&t, XORI(X_A4, X_T0, 0x0FF));
        th_emit(&t, SLLI(X_T2, X_T0, 4));
        th_emit(&t, SRLI(X_S0, X_T0, 4));
        t.cpu.x[X_T0] = 0x1000;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0x1000 + 100);
        CHECK_U64(t.cpu.x[X_A1], 0x1000 - 100);
        CHECK_U64(t.cpu.x[X_A2], 0x1000 & 0x0FF);
        CHECK_U64(t.cpu.x[X_A3], 0x1000 | 0x00F);
        CHECK_U64(t.cpu.x[X_A4], 0x1000 ^ 0x0FF);
        CHECK_U64(t.cpu.x[X_T2], 0x10000);
        CHECK_U64(t.cpu.x[X_S0], 0x100);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SLTI(X_A0, X_T0, 1));
        th_emit(&t, SLTIU(X_A1, X_T0, 1));
        t.cpu.x[X_T0] = (u64)-5;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 1);
        CHECK_U64(t.cpu.x[X_A1], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SRAI(X_A0, X_T0, 62));
        th_emit(&t, SRLI(X_A1, X_T0, 62));
        t.cpu.x[X_T0] = (u64)INT64_MIN;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], (u64)(INT64_MIN >> 62));
        CHECK_U64(t.cpu.x[X_A1], (u64)INT64_MIN >> 62);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SLLI(X_A0, X_T0, 63));
        th_emit(&t, SRLI(X_A1, X_A0, 63));
        th_emit(&t, SLLI(X_A2, X_T0, 40));
        t.cpu.x[X_T0] = 1;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0x8000000000000000ULL);
        CHECK_U64(t.cpu.x[X_A1], 1);
        CHECK_U64(t.cpu.x[X_A2], 1ULL << 40);
        th_free(&t);
    }
}

void test_cpu_shifts(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SRL(X_A0, X_T0, X_T1));
        th_emit(&t, SRA(X_A1, X_T0, X_T1));
        t.cpu.x[X_T0] = 0x8000000000000000ULL;
        t.cpu.x[X_T1] = 63;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 1);
        CHECK_U64(t.cpu.x[X_A1], UINT64_MAX);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SLL(X_A0, X_T0, X_T1));
        th_emit(&t, SRL(X_A1, X_T0, X_T1));
        t.cpu.x[X_T0] = 0x8000000000000000ULL;
        t.cpu.x[X_T1] = 1;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0);
        CHECK_U64(t.cpu.x[X_A1], 0x4000000000000000ULL);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SLL(X_A0, X_T0, X_T1));
        t.cpu.x[X_T0] = 1;
        t.cpu.x[X_T1] = 64;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 1);
        th_free(&t);
    }
}

void test_cpu_loads_stores(void) {
    const u64 buf = TH_DATA_ADDR;

    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_poke64(&t, buf, 0x1122334455667788ULL);
        th_poke64(&t, buf + 8, 0xFFFFFFFFFFFFFFFFULL);
        th_li(&t, X_T0, (u32)buf);
        th_emit(&t, LB(X_A0, X_T0, 0));
        th_emit(&t, LBU(X_A1, X_T0, 0));
        th_emit(&t, LH(X_A2, X_T0, 0));
        th_emit(&t, LHU(X_A3, X_T0, 0));
        th_emit(&t, LW(X_A4, X_T0, 0));
        th_emit(&t, LWU(X_A5, X_T0, 0));
        th_emit(&t, LD(X_T1, X_T0, 0));
        th_emit(&t, LB(X_T2, X_T0, 8));
        th_emit(&t, LH(X_S0, X_T0, 8));
        th_emit(&t, LW(X_S1, X_T0, 8));
        th_run_all(&t);

        CHECK_S64(t.cpu.x[X_A0], (u64)(s64)(s8)0x88);
        CHECK_U64(t.cpu.x[X_A1], 0x88);
        CHECK_S64(t.cpu.x[X_A2], (u64)(s64)(s16)0x7788);
        CHECK_U64(t.cpu.x[X_A3], 0x7788);
        CHECK_S64(t.cpu.x[X_A4], (u64)(s64)(s32)0x55667788);
        CHECK_U64(t.cpu.x[X_A5], 0x55667788);
        CHECK_U64(t.cpu.x[X_T1], 0x1122334455667788ULL);
        CHECK_S64(t.cpu.x[X_T2], (u64)(s64)(s8)0xFF);
        CHECK_S64(t.cpu.x[X_S0], (u64)(s64)(s16)0xFFFF);
        CHECK_S64(t.cpu.x[X_S1], (u64)(s64)(s32)0xFFFFFFFF);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_li(&t, X_T0, (u32)buf);
        th_li(&t, X_T1, 0x7F);
        th_emit(&t, SB(X_T1, X_T0, 16));
        th_li(&t, X_T1, 0x12345000);
        th_emit(&t, SH(X_T1, X_T0, 18));
        th_emit(&t, SW(X_T1, X_T0, 20));
        th_li(&t, X_T2, 0x89ABC123);
        th_emit(&t, SD(X_T2, X_T0, 24));
        th_run_all(&t);

        CHECK_U64(th_peek64(&t, buf + 16) & 0xFF, 0x7F);
        CHECK_U64(th_peek32(&t, buf + 18) & 0xFFFF, 0x5000);
        CHECK_U64(th_peek32(&t, buf + 20), 0x12345000u);
        CHECK_U64(th_peek64(&t, buf + 24), 0x89ABC123ULL);
        CHECK_U64((th_peek64(&t, buf + 24) >> 32) & 0xFFFFFFFF, 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MTVEC] = TH_CODE_ADDR + 0x400;
        th_li(&t, X_T0, 0x30000000);
        th_emit(&t, LW(X_A0, X_T0, 0));
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_LOAD_FAULT);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 0x400);
        th_free(&t);
    }
}

void test_cpu_branches(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);
    th_emit(&t, BEQ(X_T0, X_T1, 8));
    th_emit(&t, ADDI(X_A0, X_ZERO, 111));
    th_emit(&t, BNE(X_T0, X_T1, 8));
    th_emit(&t, ADDI(X_A0, X_ZERO, 222));
    th_emit(&t, BLT(X_T2, X_T0, 8));
    th_emit(&t, ADDI(X_A1, X_ZERO, 111));
    th_emit(&t, BGE(X_T0, X_T2, 8));
    th_emit(&t, ADDI(X_A1, X_ZERO, 222));
    th_emit(&t, BLTU(X_T0, X_T2, 8));
    th_emit(&t, ADDI(X_A2, X_ZERO, 111));
    th_emit(&t, BGEU(X_T2, X_T0, 8));
    th_emit(&t, ADDI(X_A2, X_ZERO, 222));
    th_emit(&t, ADDI(X_A3, X_ZERO, 0));
    u64 loop = t.code;
    th_emit(&t, ADDI(X_A3, X_A3, 1));
    th_emit(&t, BNE(X_A3, X_S1, (s32)(loop - t.code)));
    th_emit(&t, ADDI(X_A4, X_ZERO, 99));
    t.cpu.x[X_T0] = 10;
    t.cpu.x[X_T1] = 10;
    t.cpu.x[X_T2] = (u64)-1;
    t.cpu.x[X_S1] = 3;
    th_run_all(&t);

    CHECK_U64(t.cpu.x[X_A0], 222);
    CHECK_U64(t.cpu.x[X_A1], 0);
    CHECK_U64(t.cpu.x[X_A2], 0);
    CHECK_U64(t.cpu.x[X_A3], 3);
    CHECK_U64(t.cpu.x[X_A4], 99);
    th_free(&t);
}

void test_cpu_jumps(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);
    const u64 target = TH_CODE_ADDR + 400;

    u64 jal = th_emit(&t, JAL(X_RA, 12));
    th_emit(&t, ADDI(X_A0, X_ZERO, 111));
    th_emit(&t, ADDI(X_A1, X_ZERO, 111));
    u64 after = t.code;
    th_emit(&t, ADDI(X_A2, X_ZERO, 7));
    th_emit(&t, JALR(X_T1, X_T0, 4));
    th_emit(&t, ADDI(X_A3, X_ZERO, 111));
    while (t.code < target)
        th_emit(&t, NOP());
    CHECK_U64(t.code, target);
    th_emit(&t, ADDI(X_A4, X_ZERO, 9));

    t.cpu.x[X_T0] = target - 4;
    th_run_all(&t);

    CHECK_U64(t.cpu.x[X_RA], jal + 4);
    CHECK_U64(t.cpu.x[X_A0], 0);
    CHECK_U64(t.cpu.x[X_A1], 0);
    CHECK_U64(after, jal + 12);
    CHECK_U64(t.cpu.x[X_A2], 7);
    CHECK_U64(t.cpu.x[X_T1], after + 8);
    CHECK_U64(t.cpu.x[X_A3], 0);
    CHECK_U64(t.cpu.x[X_A4], 9);
    CHECK_U64(t.cpu.pc, target + 4);
    th_free(&t);
}

void test_cpu_upper(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);
    u64 pc0 = t.cpu.pc;
    th_emit(&t, LUI(X_A0, 0xDEADB000));
    th_emit(&t, LUI(X_A1, 0x80000000));
    th_emit(&t, AUIPC(X_A2, 0x1000));
    th_emit(&t, LUI(X_A3, 0x10000000));
    th_emit(&t, ADDI(X_A3, X_A3, 0x40));
    th_run_all(&t);

    CHECK_U64(t.cpu.x[X_A0], (u64)(s64)(s32)0xDEADB000);
    CHECK_U64(t.cpu.x[X_A1], 0xFFFFFFFF80000000ULL);
    CHECK_U64(t.cpu.x[X_A2], pc0 + 8 + 0x1000);
    CHECK_U64(t.cpu.x[X_A3], RVM_UART_BASE + 0x40);
    th_free(&t);
}

void test_cpu_word_ops(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, ADDW(X_A0, X_T0, X_T1));
        th_emit(&t, SUBW(X_A1, X_T0, X_T1));
        th_emit(&t, ADDIW(X_A2, X_T0, 1));
        th_emit(&t, MULW(X_A3, X_T0, X_T1));
        t.cpu.x[X_T0] = 0x7FFFFFFFULL;
        t.cpu.x[X_T1] = 1;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0xFFFFFFFF80000000ULL);
        CHECK_U64(t.cpu.x[X_A1], 0x7FFFFFFEULL);
        CHECK_U64(t.cpu.x[X_A2], 0xFFFFFFFF80000000ULL);
        CHECK_U64(t.cpu.x[X_A3], (u64)(s64)(s32)((s32)0x7FFFFFFF * 1));
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, SLLIW(X_A0, X_T0, 4));
        th_emit(&t, SRLIW(X_A1, X_T0, 4));
        th_emit(&t, SRAIW(X_A2, X_T0, 4));
        th_emit(&t, SLLW(X_A3, X_T0, X_T1));
        th_emit(&t, SRLW(X_A4, X_T0, X_T1));
        th_emit(&t, SRAW(X_A5, X_T0, X_T1));
        th_emit(&t, ADDW(X_A7, X_T0, X_T1));
        t.cpu.x[X_T0] = 0xDEADBEEF80000000ULL;
        t.cpu.x[X_T1] = 0x0000000F00000003ULL;
        th_run_all(&t);
        const u32 w = 0x80000000u;
        const u32 sll4 = (u32)(((u64)w << 4) & 0xFFFFFFFFu);
        const u32 sll3 = (u32)(((u64)w << 3) & 0xFFFFFFFFu);
        CHECK_U64(t.cpu.x[X_A0], (u64)(s64)(s32)sll4);
        CHECK_U64(t.cpu.x[X_A1], (u64)(s64)(s32)(w >> 4));
        CHECK_U64(t.cpu.x[X_A2], (u64)(s64)(s32)((s32)w >> 4));
        CHECK_U64(t.cpu.x[X_A3], (u64)(s64)(s32)sll3);
        CHECK_U64(t.cpu.x[X_A4], (u64)(s64)(s32)(w >> 3));
        CHECK_U64(t.cpu.x[X_A5], (u64)(s64)(s32)((s32)w >> 3));
        CHECK_U64(t.cpu.x[X_A7], (u64)(s64)(s32)((s32)w + 3));
        th_free(&t);
    }
}

void test_cpu_mul_div(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, MUL(X_A0, X_T0, X_T1));
        th_emit(&t, MULHU(X_A1, X_T0, X_T1));
        th_emit(&t, MULH(X_A2, X_T0, X_T1));
        th_emit(&t, MULHSU(X_A3, X_T0, X_T1));
        t.cpu.x[X_T0] = (u64)-3;
        t.cpu.x[X_T1] = 5;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], (u64)(-15));
        CHECK_U64(t.cpu.x[X_A1], (u64)(((unsigned __int128)(u64)-3 * (unsigned __int128)5) >> 64));
        CHECK_U64(t.cpu.x[X_A2], (u64)-1);
        CHECK_U64(t.cpu.x[X_A3], (u64)-1);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, DIV(X_A0, X_T0, X_T1));
        th_emit(&t, REM(X_A1, X_T0, X_T1));
        th_emit(&t, DIVU(X_A2, X_T0, X_T1));
        th_emit(&t, REMU(X_A3, X_T0, X_T1));
        t.cpu.x[X_T0] = 100;
        t.cpu.x[X_T1] = 7;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 14);
        CHECK_U64(t.cpu.x[X_A1], 2);
        CHECK_U64(t.cpu.x[X_A2], 14);
        CHECK_U64(t.cpu.x[X_A3], 2);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, DIV(X_A0, X_T0, X_T1));
        th_emit(&t, REM(X_A1, X_T0, X_T1));
        th_emit(&t, DIVU(X_A2, X_T0, X_T1));
        th_emit(&t, REMU(X_A3, X_T0, X_T1));
        t.cpu.x[X_T0] = (u64)-100;
        t.cpu.x[X_T1] = 7;
        th_run_all(&t);
        CHECK_S64(t.cpu.x[X_A0], (u64)(s64)-14);
        CHECK_S64(t.cpu.x[X_A1], (u64)(s64)-2);
        CHECK_U64(t.cpu.x[X_A2], (UINT64_MAX - 99) / 7);
        CHECK_U64(t.cpu.x[X_A3], (UINT64_MAX - 99) % 7);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, DIV(X_A0, X_T0, X_T1));
        th_emit(&t, DIVU(X_A1, X_T0, X_T1));
        th_emit(&t, REM(X_A2, X_T0, X_T1));
        th_emit(&t, REMU(X_A3, X_T0, X_T1));
        t.cpu.x[X_T0] = 42;
        t.cpu.x[X_T1] = 0;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], UINT64_MAX);
        CHECK_U64(t.cpu.x[X_A1], UINT64_MAX);
        CHECK_U64(t.cpu.x[X_A2], 42);
        CHECK_U64(t.cpu.x[X_A3], 42);
        CHECK_U64(t.cpu.n_traps, 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, DIV(X_A0, X_T0, X_T1));
        th_emit(&t, REM(X_A1, X_T0, X_T1));
        t.cpu.x[X_T0] = (u64)INT64_MIN;
        t.cpu.x[X_T1] = (u64)-1;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], (u64)INT64_MIN);
        CHECK_U64(t.cpu.x[X_A1], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit(&t, MULW(X_A0, X_T0, X_T1));
        th_emit(&t, DIVW(X_A1, X_T0, X_T1));
        th_emit(&t, REMW(X_A2, X_T0, X_T1));
        th_emit(&t, DIVUW(X_A3, X_T0, X_T1));
        th_emit(&t, REMUW(X_A4, X_T0, X_T1));
        t.cpu.x[X_T0] = 0xFFFFFFFFFFFFFF9CULL;
        t.cpu.x[X_T1] = 7;
        th_run_all(&t);
        CHECK_S64(t.cpu.x[X_A0], (u64)(s64)(s32)((s32)-100 * 7));
        CHECK_S64(t.cpu.x[X_A1], (u64)(s64)(s32)(-100 / 7));
        CHECK_S64(t.cpu.x[X_A2], (u64)(s64)(s32)(-100 % 7));
        CHECK_S64(t.cpu.x[X_A3], (u64)(s64)(s32)(0xFFFFFF9Cu / 7u));
        CHECK_S64(t.cpu.x[X_A4], (u64)(s64)(s32)(0xFFFFFF9Cu % 7u));
        th_free(&t);
    }
}

void test_cpu_amo(void) {
    const u64 w32 = TH_DATA_ADDR;
    const u64 w64 = TH_DATA_ADDR + 0x40;

    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_li(&t, X_T0, (u32)w32);
        th_li(&t, X_T1, 77);
        th_emit(&t, LR_W(X_A0, X_T0));
        th_emit(&t, SC_W(X_A1, X_T1, X_T0));
        th_emit(&t, SC_W(X_A2, X_T1, X_T0));
        th_emit(&t, LW(X_A3, X_T0, 0));
        th_poke32(&t, w32, 100);
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 100);
        CHECK_U64(t.cpu.x[X_A1], 0);
        CHECK_U64(t.cpu.x[X_A2], 1);
        CHECK_U64(t.cpu.x[X_A3], 77);
        CHECK_U64(th_peek32(&t, w32), 77);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_li(&t, X_T0, (u32)w64);
        th_emit(&t, ADDI(X_T1, X_ZERO, -1));
        th_emit(&t, LR_D(X_A0, X_T0));
        th_emit(&t, SC_D(X_A1, X_T1, X_T0));
        th_emit(&t, LD(X_A2, X_T0, 0));
        th_poke64(&t, w64, 0x1122334455667788ULL);
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0x1122334455667788ULL);
        CHECK_U64(t.cpu.x[X_A1], 0);
        CHECK_U64(t.cpu.x[X_A2], UINT64_MAX);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u32 base = (u32)(TH_DATA_ADDR);
        const u32 operand = 0x0F0FF;
        th_li(&t, X_T1, operand);
        th_li(&t, X_T0, base + 0);
        th_emit(&t, AMOSWAP_W(X_A0, X_T1, X_T0));
        th_li(&t, X_T0, base + 8);
        th_emit(&t, AMOADD_W(X_A1, X_T1, X_T0));
        th_li(&t, X_T0, base + 16);
        th_emit(&t, AMOXOR_W(X_A2, X_T1, X_T0));
        th_li(&t, X_T0, base + 24);
        th_emit(&t, AMOAND_W(X_A3, X_T1, X_T0));
        th_li(&t, X_T0, base + 32);
        th_emit(&t, AMOOR_W(X_A4, X_T1, X_T0));
        th_li(&t, X_T0, base + 40);
        th_emit(&t, ADDI(X_T1, X_ZERO, 3));
        th_emit(&t, AMOMIN_W(X_A5, X_T1, X_T0));
        th_li(&t, X_T0, base + 48);
        th_emit(&t, AMOMAX_W(X_T2, X_T1, X_T0));
        th_li(&t, X_T0, base + 56);
        th_emit(&t, AMOMINU_W(X_S0, X_T1, X_T0));
        th_li(&t, X_T0, base + 64);
        th_emit(&t, AMOMAXU_W(X_S1, X_T1, X_T0));

        th_poke32(&t, base + 0, 0x0F0F);
        th_poke32(&t, base + 8, 100);
        th_poke32(&t, base + 16, 0x0F0F);
        th_poke32(&t, base + 24, 0x0F0F);
        th_poke32(&t, base + 32, 0x0F0F);
        th_poke32(&t, base + 40, 0xFFFFFFF6u);
        th_poke32(&t, base + 48, 0xFFFFFFF6u);
        th_poke32(&t, base + 56, 0xFFFFFFF6u);
        th_poke32(&t, base + 64, 0xFFFFFFF6u);
        th_run_all(&t);

        CHECK_U64(t.cpu.x[X_A0], 0x0F0F);
        CHECK_U64(th_peek32(&t, base + 0), operand);
        CHECK_U64(t.cpu.x[X_A1], 100);
        CHECK_U64(th_peek32(&t, base + 8), 100 + operand);
        CHECK_U64(t.cpu.x[X_A2], 0x0F0F);
        CHECK_U64(th_peek32(&t, base + 16), 0x0F0F ^ operand);
        CHECK_U64(t.cpu.x[X_A3], 0x0F0F);
        CHECK_U64(th_peek32(&t, base + 24), 0x0F0F & operand);
        CHECK_U64(t.cpu.x[X_A4], 0x0F0F);
        CHECK_U64(th_peek32(&t, base + 32), 0x0F0F | operand);
        const u64 oldv = (u64)(s64)(s32)0xFFFFFFF6u;
        CHECK_S64(t.cpu.x[X_A5], oldv);
        CHECK_U64(th_peek32(&t, base + 40), 0xFFFFFFF6u);
        CHECK_S64(t.cpu.x[X_T2], oldv);
        CHECK_U64(th_peek32(&t, base + 48), 3u);
        CHECK_S64(t.cpu.x[X_S0], oldv);
        CHECK_U64(th_peek32(&t, base + 56), 3u);
        CHECK_S64(t.cpu.x[X_S1], oldv);
        CHECK_U64(th_peek32(&t, base + 64), 0xFFFFFFF6u);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_li(&t, X_T0, (u32)w64);
        th_emit(&t, ADDI(X_T1, X_ZERO, 0x23));
        th_emit(&t, AMOADD_D(X_A0, X_T1, X_T0));
        th_emit(&t, AMOSWAP_D(X_A1, X_T1, X_T0));
        th_emit(&t, LD(X_A2, X_T0, 0));
        th_poke64(&t, w64, 0x1000);
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0x1000);
        CHECK_U64(t.cpu.x[X_A1], 0x1023);
        CHECK_U64(t.cpu.x[X_A2], 0x23);
        CHECK_U64(th_peek64(&t, w64), 0x23);
        th_free(&t);
    }
}

static inline u16 c_addi(u32 rd, s32 imm) {
    u32 u = (u32)imm & 0x3F;
    return (u16)((((u >> 5) & 1) << 12) | (rd << 7) | ((u & 0x1F) << 2) | 1);
}
static inline u16 c_li(u32 rd, s32 imm) {
    u32 u = (u32)imm & 0x3F;
    return (u16)((2 << 13) | (((u >> 5) & 1) << 12) | (rd << 7) | ((u & 0x1F) << 2) | 1);
}
static inline u16 c_lui(u32 rd, s32 v6) {
    u32 u = (u32)v6 & 0x3F;
    return (u16)((3 << 13) | (((u >> 5) & 1) << 12) | (rd << 7) | ((u & 0x1F) << 2) | 1);
}
static inline u16 c_mv(u32 rd, u32 rs2) {
    return (u16)((4 << 13) | (rd << 7) | (rs2 << 2) | 2);
}
static inline u16 c_add(u32 rd, u32 rs2) {
    return (u16)((4 << 13) | (1 << 12) | (rd << 7) | (rs2 << 2) | 2);
}
static inline u16 c_ld(u32 rd3, u32 rs1_3, s32 off) {
    u32 u = (u32)off;
    return (u16)((3 << 13) | (((u >> 3) & 7) << 10) | (rs1_3 << 7) | (((u >> 6) & 3) << 5) |
                 (rd3 << 2));
}
static inline u16 c_sd(u32 rs2_3, u32 rs1_3, s32 off) {
    u32 u = (u32)off;
    return (u16)((7 << 13) | (((u >> 3) & 7) << 10) | (rs1_3 << 7) | (((u >> 6) & 3) << 5) |
                 (rs2_3 << 2));
}
static inline u16 c_slli(u32 rd, u32 sh) {
    return (u16)(((rd << 7) | (sh << 2)) | 2);
}
static inline u16 c_andi(u32 rd3, s32 imm) {
    u32 u = (u32)imm & 0x3F;
    return (u16)((4 << 13) | (2 << 10) | (((u >> 5) & 1) << 12) | (rd3 << 7) | ((u & 0x1F) << 2) |
                 1);
}
static inline u16 c_sub(u32 rd3, u32 rs2_3) {
    return (u16)((4 << 13) | (3 << 10) | (rd3 << 7) | (rs2_3 << 2) | 1);
}
static inline u16 c_j(s32 off) {
    u32 u = (u32)off;
    return (u16)((5 << 13) | (((u >> 11) & 1) << 12) | (((u >> 4) & 1) << 11) |
                 (((u >> 8) & 3) << 9) | (((u >> 10) & 1) << 8) | (((u >> 6) & 1) << 7) |
                 (((u >> 7) & 1) << 6) | (((u >> 1) & 7) << 3) | (((u >> 5) & 1) << 2) | 1);
}
static inline u16 c_beqz(u32 rs1_3, s32 off) {
    u32 u = (u32)off;
    return (u16)((6 << 13) | (((u >> 8) & 1) << 12) | (((u >> 3) & 3) << 10) | (rs1_3 << 7) |
                 (((u >> 6) & 3) << 5) | (((u >> 1) & 3) << 3) | (((u >> 5) & 1) << 2) | 1);
}
static inline u16 c_addi4spn(u32 rd3, u32 nzuimm) {
    return (u16)(((((nzuimm >> 4) & 3) << 11) | (((nzuimm >> 6) & 0xF) << 7) |
                  (((nzuimm >> 2) & 3) << 5) | (rd3 << 2)));
}

void test_cpu_compressed(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit16(&t, c_addi(X_T0, 5));
        th_emit16(&t, c_addi(X_T0, 7));
        th_emit16(&t, c_li(X_T1, -3));
        th_emit16(&t, c_lui(X_T2, 0x15));
        th_emit16(&t, c_mv(X_A0, X_T0));
        th_emit16(&t, c_add(X_A0, X_T1));
        th_emit16(&t, c_slli(X_T0, 4));
        th_emit16(&t, c_li(X_S1, 0x1F));
        th_emit16(&t, c_andi(X_S1 - 8, -1));
        th_emit16(&t, c_sub(X_S1 - 8, X_S1 - 8));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_T0], 192);
        CHECK_U64(t.cpu.x[X_T1], (u64)(s64)-3);
        CHECK_U64(t.cpu.x[X_T2], (u64)(s64)(s32)0x15000);
        CHECK_U64(t.cpu.x[X_A0], 9);
        CHECK_U64(t.cpu.x[X_S1], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 p = TH_DATA_ADDR;
        th_li(&t, X_S0, (u32)p);
        th_li(&t, X_S1, 0xDEAD0000);
        th_emit16(&t, c_sd(X_S1 - 8, X_S0 - 8, 8));
        th_emit16(&t, c_ld(X_A0 - 8, X_S0 - 8, 8));
        th_run_all(&t);
        CHECK_U64(th_peek64(&t, p + 8), 0xDEAD0000ULL);
        CHECK_U64(t.cpu.x[X_A0], 0xDEAD0000ULL);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        u64 jaddr = th_emit16(&t, c_j(6));
        th_emit(&t, ADDI(X_A0, X_ZERO, 111));
        CHECK_U64(t.code, jaddr + 6);
        th_emit16(&t, c_li(X_A1, 0));
        u64 baddr = t.code;
        th_emit16(&t, c_beqz(X_A1 - 8, 6));
        th_emit(&t, ADDI(X_A2, X_ZERO, 111));
        CHECK_U64(t.code, baddr + 6);
        th_emit16(&t, c_addi(X_A3, 1));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0);
        CHECK_U64(t.cpu.x[X_A2], 0);
        CHECK_U64(t.cpu.x[X_A3], 1);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_emit16(&t, 0x0001);
        th_emit16(&t, c_addi4spn(X_A0 - 8, 64));
        th_emit16(&t, c_addi4spn(X_A1 - 8, 80));
        th_emit16(&t, c_addi4spn(X_A2 - 8, 12));
        t.cpu.x[X_SP] = 0x1000;
        th_run_all(&t);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 8);
        CHECK_U64(t.cpu.x[X_SP], 0x1000);
        CHECK_U64(t.cpu.x[X_A0], 0x1000 + 64);
        CHECK_U64(t.cpu.x[X_A1], 0x1000 + 80);
        CHECK_U64(t.cpu.x[X_A2], 0x1000 + 12);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MTVEC] = TH_CODE_ADDR + 0x400;
        u64 cmv = th_emit16(&t, (u16)(0x8002 | (X_A0 << 7) | (X_T0 << 2)));
        CHECK_U64(cmv, TH_CODE_ADDR);
        u64 cj = th_emit16(&t, (u16)((4 << 13) | (1 << 12) | (X_T1 << 7) | 2));
        while (t.code < cj + 64)
            th_emit(&t, NOP());
        u64 land = th_emit16(&t, c_addi(X_A1, 7));

        t.cpu.x[X_T0] = 0x4242;
        t.cpu.x[X_T1] = land;
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 0x4242);
        CHECK_U64(t.cpu.x[X_RA], cj + 2);
        CHECK_U64(t.cpu.x[X_A1], 7);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MTVEC] = TH_CODE_ADDR + 0x400;
        th_emit16(&t, 0x9002);
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_BREAKPOINT);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 0x400);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MTVEC] = TH_CODE_ADDR + 0x400;
        th_emit16(&t, 0x0000);
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 0x400);
        th_free(&t);
    }
}

void test_cpu_csr(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        th_li(&t, X_T0, 0x12345000);
        th_emit(&t, CSRRW(X_A0, CSR_MSCRATCH, X_T0));
        th_emit(&t, CSRRW(X_A1, CSR_MSCRATCH, X_ZERO));
        th_emit(&t, ADDI(X_T1, X_ZERO, 0x0F0));
        th_emit(&t, CSRRS(X_A2, CSR_MSCRATCH, X_T1));
        th_emit(&t, ADDI(X_T2, X_ZERO, 0x010));
        th_emit(&t, CSRRC(X_A3, CSR_MSCRATCH, X_T2));
        th_emit(&t, CSRRS(X_A4, CSR_MSCRATCH, X_ZERO));
        th_emit(&t, CSRRWI(X_A5, CSR_MSCRATCH, 5));
        th_emit(&t, CSRRSI(X_T2, CSR_MSCRATCH, 2));
        th_emit(&t, CSRRCI(X_S0, CSR_MSCRATCH, 1));
        th_emit(&t, CSRRS(X_S1, CSR_MARCHID, X_ZERO));
        th_emit(&t, CSRRS(X_A7, CSR_MHARTID, X_ZERO));
        th_run_all(&t);

        CHECK_U64(t.cpu.x[X_A0], 0);
        CHECK_U64(t.cpu.x[X_A1], 0x12345000u);
        CHECK_U64(t.cpu.x[X_A2], 0);
        CHECK_U64(t.cpu.x[X_A3], 0x0F0);
        CHECK_U64(t.cpu.x[X_A4], 0x0E0);
        CHECK_U64(t.cpu.x[X_A5], 0x0E0);
        CHECK_U64(t.cpu.x[X_T2], 5);
        CHECK_U64(t.cpu.x[X_S0], 7);
        CHECK_U64(t.cpu.csr[CSR_MSCRATCH], 6);
        CHECK_U64(t.cpu.x[X_S1], 0x72766dULL);
        CHECK_U64(t.cpu.x[X_A7], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MIDELEG] = MIP_SSIP | MIP_STIP | MIP_SEIP;
        t.cpu.csr[CSR_MSTATUS] = MSTATUS_SIE | MSTATUS_MIE | MSTATUS_SPP;
        t.cpu.csr[CSR_MIE] = 0;
        th_emit(&t, CSRRS(X_A0, CSR_SSTATUS, X_ZERO));
        th_emit(&t, ADDI(X_T1, X_ZERO, 0x222));
        th_emit(&t, CSRRW(X_ZERO, CSR_SIE, X_T1));
        th_emit(&t, CSRRS(X_A1, CSR_MIE, X_ZERO));
        th_emit(&t, CSRRS(X_A2, CSR_SIP, X_ZERO));
        th_run_all(&t);

        CHECK_U64(t.cpu.x[X_A0] & MSTATUS_SIE, MSTATUS_SIE);
        CHECK_U64(t.cpu.x[X_A0] & MSTATUS_SPP, MSTATUS_SPP);
        CHECK_U64(t.cpu.x[X_A0] & MSTATUS_MIE, 0);
        CHECK_U64(t.cpu.x[X_A1], 0x222);
        CHECK_U64(t.cpu.x[X_A1] & MIP_MTIP, 0);
        CHECK_U64(t.cpu.x[X_A2], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        th_li(&t, X_T0, 0x1234);
        th_emit(&t, CSRRW(X_A0, CSR_MVENDORID, X_T0));
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        CHECK_U64(t.cpu.pc, vec);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        t.cpu.priv = PRV_S;
        th_emit(&t, CSRRS(X_A0, CSR_MSCRATCH, X_ZERO));
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        CHECK_U64(t.cpu.pc, vec);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MSTATUS] = 3ULL << MSTATUS_FS_SHIFT;
        th_emit(&t, CSRRS(X_A0, CSR_MSTATUS, X_ZERO));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0] & MSTATUS_SD, MSTATUS_SD);
        th_free(&t);
    }
}

void test_cpu_trap_mret(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        u64 brk = th_emit(&t, EBREAK());
        while (t.code < vec)
            th_emit(&t, NOP());
        th_emit(&t, ADDI(X_A0, X_ZERO, 1));
        th_run_all(&t);

        CHECK_U64(t.cpu.x[X_A0], 1);
        CHECK_U64(t.cpu.csr[CSR_MEPC], brk);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_BREAKPOINT);
        CHECK_U64(t.cpu.csr[CSR_MTVAL], brk);
        CHECK_U64(t.cpu.priv, PRV_M);
        CHECK_U64((t.cpu.csr[CSR_MSTATUS] & MSTATUS_MPP) >> MSTATUS_MPP_SHIFT, PRV_M);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.priv = PRV_M;
        t.cpu.csr[CSR_MSTATUS] = (PRV_S << MSTATUS_MPP_SHIFT) | MSTATUS_MPIE;
        t.cpu.csr[CSR_MEPC] = TH_CODE_ADDR + 0x200;
        th_emit(&t, MRET());
        th_run_all(&t);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 0x200);
        CHECK_U64(t.cpu.priv, PRV_S);
        CHECK_U64(t.cpu.csr[CSR_MSTATUS] & MSTATUS_MIE, MSTATUS_MIE);
        CHECK_U64(t.cpu.csr[CSR_MSTATUS] & MSTATUS_MPP, 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 svec = TH_CODE_ADDR + 0x300;
        t.cpu.csr[CSR_STVEC] = svec;
        t.cpu.csr[CSR_MEDELEG] = (1ULL << EXC_ECALL_U);
        t.cpu.priv = PRV_U;
        u64 ec = th_emit(&t, ECALL());
        while (t.code < svec)
            th_emit(&t, NOP());
        th_emit(&t, ADDI(X_A0, X_ZERO, 42));
        th_run_all(&t);

        CHECK_U64(t.cpu.priv, PRV_S);
        CHECK_U64(t.cpu.csr[CSR_SCAUSE], EXC_ECALL_U);
        CHECK_U64(t.cpu.csr[CSR_SEPC], ec);
        CHECK_U64(t.cpu.x[X_A0], 42);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], 0);
        CHECK_U64(t.cpu.csr[CSR_MEPC], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.priv = PRV_S;
        t.cpu.csr[CSR_MSTATUS] = MSTATUS_SPP | MSTATUS_SPIE;
        t.cpu.csr[CSR_SEPC] = TH_CODE_ADDR + 0x100;
        th_emit(&t, SRET());
        th_run_all(&t);
        CHECK_U64(t.cpu.pc, TH_CODE_ADDR + 0x100);
        CHECK_U64(t.cpu.priv, PRV_S);
        CHECK_U64(t.cpu.csr[CSR_MSTATUS] & MSTATUS_SIE, MSTATUS_SIE);
        CHECK_U64(t.cpu.csr[CSR_MSTATUS] & MSTATUS_SPIE, MSTATUS_SPIE);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 svec = TH_CODE_ADDR + 0x300;
        t.cpu.csr[CSR_STVEC] = svec | 1;
        t.cpu.csr[CSR_MIDELEG] = MIP_STIP;
        t.cpu.csr[CSR_MIE] = MIP_STIP;
        t.cpu.csr[CSR_MSTATUS] |= MSTATUS_SIE;
        t.cpu.priv = PRV_S;
        t.cpu.hw_mip = MIP_STIP;
        th_emit(&t, NOP());
        step_result r = cpu_step(&t.cpu);
        CHECK_U64(r, STEP_TRAP);
        CHECK_U64(t.cpu.pc, svec + 4 * IRQ_S_TIMER);
        CHECK_U64(t.cpu.csr[CSR_SCAUSE], (1ULL << 63) | IRQ_S_TIMER);
        CHECK_U64(t.cpu.csr[CSR_SEPC], TH_CODE_ADDR);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.priv = PRV_M;
        th_emit(&t, ECALL());
        step_result r = cpu_step(&t.cpu);
        CHECK_U64(r, STEP_ECALL_M);
        CHECK_U64(t.cpu.last_from_priv, PRV_M);
        th_free(&t);
    }
}

void test_cpu_illegal(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        u32 bad = ENC_R(OP_OP, X_A0, 4, X_T0, X_T1, 0x02);
        th_emit(&t, bad);
        th_run(&t, 1);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        CHECK_U64(t.cpu.csr[CSR_MTVAL], bad);
        CHECK_U64(t.cpu.pc, vec);
        CHECK_U64(t.cpu.priv, PRV_M);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        u32 bad = ENC_I(OP_OPIMM, X_A0, 5, X_T0, 0x300);
        th_emit(&t, bad);
        th_run(&t, 1);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        th_li(&t, X_T0, (u32)RVM_UART_BASE);
        th_emit(&t, SD(X_T0, X_T0, 0));
        th_run_all(&t);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_STORE_FAULT);
        th_free(&t);
    }
}

static u64 d2b(double d) {
    u64 b;
    memcpy(&b, &d, 8);
    return b;
}
static double b2d(u64 b) {
    double d;
    memcpy(&d, &b, 8);
    return d;
}
static u32 f2b(float f) {
    u32 b;
    memcpy(&b, &f, 4);
    return b;
}
static float b2f(u32 b) {
    float f;
    memcpy(&f, &b, 4);
    return f;
}
static u64 box32c(u32 bits) {
    return 0xFFFFFFFF00000000ULL | bits;
}

void test_cpu_fp(void) {
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 p = TH_DATA_ADDR;
        th_li(&t, X_T0, (u32)p);
        th_emit(&t, FSD(0, X_T0, 0));
        th_emit(&t, FLD(1, X_T0, 0));
        th_emit(&t, FLW(2, X_T0, 8));
        th_emit(&t, FSW(2, X_T0, 16));
        t.cpu.f[0] = d2b(3.14159265358979);
        th_poke32(&t, p + 8, f2b(2.5f));
        th_run_all(&t);

        CHECK_U64(th_peek64(&t, p), d2b(3.14159265358979));
        CHECK(b2d(t.cpu.f[1]) == 3.14159265358979);
        CHECK_U64(t.cpu.f[2] >> 32, 0xFFFFFFFFu);
        CHECK(b2f((u32)t.cpu.f[2]) == 2.5f);
        CHECK_U64(th_peek32(&t, p + 16), f2b(2.5f));
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.f[0] = d2b(3.0);
        t.cpu.f[1] = d2b(4.0);
        t.cpu.f[6] = d2b(81.0);
        th_emit(&t, FADD_D(2, 0, 1, 0));
        th_emit(&t, FSUB_D(3, 1, 0, 0));
        th_emit(&t, FMUL_D(4, 0, 1, 0));
        th_emit(&t, FDIV_D(5, 1, 0, 0));
        th_emit(&t, FSQRT_D(7, 6, 0));
        th_emit(&t, FMIN_D(9, 0, 1, 0));
        th_emit(&t, FSGNJ_D(10, 0, 1, 1));
        th_emit(&t, FMADD_D(11, 0, 1, 6, 0));
        th_run_all(&t);
        CHECK(b2d(t.cpu.f[2]) == 7.0);
        CHECK(b2d(t.cpu.f[3]) == 1.0);
        CHECK(b2d(t.cpu.f[4]) == 12.0);
        CHECK(b2d(t.cpu.f[5]) == 4.0 / 3.0);
        CHECK(b2d(t.cpu.f[7]) == 9.0);
        CHECK(b2d(t.cpu.f[9]) == 3.0);
        CHECK(b2d(t.cpu.f[10]) == -3.0);
        CHECK(b2d(t.cpu.f[11]) == 93.0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.f[2] = d2b(7.0);
        th_emit(&t, FMV_X_D(X_A0, 2));
        th_emit(&t, FMV_D_X(8, X_A0));
        t.cpu.f[3] = box32c(f2b(-2.5f));
        th_emit(&t, FMV_X_W(X_A1, 3));
        th_emit(&t, FMV_W_X(9, X_A1));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], d2b(7.0));
        CHECK(b2d(t.cpu.f[8]) == 7.0);
        CHECK_S64(t.cpu.x[X_A1], (u64)(s64)(s32)f2b(-2.5f));
        CHECK_U64(t.cpu.f[9] >> 32, 0xFFFFFFFFu);
        CHECK(b2f((u32)t.cpu.f[9]) == -2.5f);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.x[X_T0] = 7;
        t.cpu.f[1] = d2b(-3.7);
        t.cpu.f[2] = d2b(1e30);
        th_emit(&t, FCVT_D_W(0, X_T0, 0));
        th_emit(&t, FCVT_W_D(X_A0, 1, 0));
        th_emit(&t, FCVT_W_D(X_A1, 1, 1));
        th_emit(&t, FCVT_W_D(X_A2, 2, 0));
        th_emit(&t, CSRRS(X_A3, CSR_FCSR, X_ZERO));
        th_run_all(&t);
        CHECK(b2d(t.cpu.f[0]) == 7.0);
        CHECK_S64(t.cpu.x[X_A0], (u64)(s64)(s32)-4);
        CHECK_S64(t.cpu.x[X_A1], (u64)(s64)(s32)-3);
        CHECK_S64(t.cpu.x[X_A2], (u64)(s64)INT32_MAX);
        CHECK_U64(t.cpu.x[X_A3] & 0x1F, 0x11);
        CHECK_U64((t.cpu.x[X_A3] >> 5) & 7, 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.f[3] = box32c(f2b(0.0f));
        t.cpu.f[4] = box32c(0x7F800000u);
        t.cpu.f[5] = box32c(0x7FC00000u);
        t.cpu.f[10] = box32c(0xFF800000u);
        t.cpu.f[11] = box32c(0x00000001u);
        t.cpu.f[6] = box32c(f2b(1.0f));
        t.cpu.f[7] = box32c(f2b(2.0f));
        th_emit(&t, FCLASS_S(X_A0, 3));
        th_emit(&t, FCLASS_S(X_A1, 4));
        th_emit(&t, FCLASS_S(X_A2, 5));
        th_emit(&t, FCLASS_S(X_A3, 10));
        th_emit(&t, FCLASS_S(X_A4, 11));
        th_emit(&t, FEQ_S(X_T1, 6, 7));
        th_emit(&t, FLT_S(X_T2, 6, 7));
        th_emit(&t, FLE_S(X_S0, 6, 6));
        th_emit(&t, FEQ_S(X_S1, 6, 6));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_A0], 1ULL << 4);
        CHECK_U64(t.cpu.x[X_A1], 1ULL << 7);
        CHECK_U64(t.cpu.x[X_A2], 1ULL << 9);
        CHECK_U64(t.cpu.x[X_A3], 1ULL << 0);
        CHECK_U64(t.cpu.x[X_A4], 1ULL << 5);
        CHECK_U64(t.cpu.x[X_T1], 0);
        CHECK_U64(t.cpu.x[X_T2], 1);
        CHECK_U64(t.cpu.x[X_S0], 1);
        CHECK_U64(t.cpu.x[X_S1], 1);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.f[0] = d2b(3.0);
        t.cpu.f[1] = d2b(4.0);
        th_emit(&t, FEQ_D(X_T0, 0, 1));
        th_emit(&t, FLT_D(X_T1, 0, 1));
        th_emit(&t, FLE_D(X_T2, 1, 0));
        th_emit(&t, FEQ_D(X_S0, 0, 0));
        th_emit(&t, FLE_D(X_S1, 0, 0));
        th_emit(&t, FLT_D(X_A0, 1, 0));
        th_run_all(&t);
        CHECK_U64(t.cpu.x[X_T0], 0);
        CHECK_U64(t.cpu.x[X_T1], 1);
        CHECK_U64(t.cpu.x[X_T2], 0);
        CHECK_U64(t.cpu.x[X_S0], 1);
        CHECK_U64(t.cpu.x[X_S1], 1);
        CHECK_U64(t.cpu.x[X_A0], 0);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.f[0] = f2b(1.0f);
        t.cpu.f[1] = box32c(f2b(1.0f));
        th_emit(&t, FADD_S(2, 0, 1, 0));
        th_emit(&t, FMV_X_W(X_A0, 2));
        th_run_all(&t);
        CHECK_U64((u32)t.cpu.x[X_A0], 0x7FC00000u);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        const u64 vec = TH_CODE_ADDR + 0x400;
        t.cpu.csr[CSR_MTVEC] = vec;
        t.cpu.csr[CSR_MSTATUS] &= ~MSTATUS_FS;
        th_emit(&t, FADD_D(2, 0, 1, 0));
        th_run(&t, 1);
        CHECK_U64(t.cpu.csr[CSR_MCAUSE], EXC_ILLEGAL_INST);
        CHECK_U64(t.cpu.pc, vec);
        th_free(&t);
    }
    {
        th t;
        CHECK(th_init(&t) == RVM_OK);
        t.cpu.csr[CSR_MSTATUS] =
            (t.cpu.csr[CSR_MSTATUS] & ~MSTATUS_FS) | (1ULL << MSTATUS_FS_SHIFT);
        t.cpu.f[0] = d2b(1.0);
        t.cpu.f[1] = d2b(2.0);
        th_emit(&t, FADD_D(2, 0, 1, 0));
        th_run_all(&t);
        bool bad = false;
        u64 ms = csr_read(&t.cpu, CSR_MSTATUS, &bad);
        CHECK(!bad);
        CHECK_U64((ms & MSTATUS_FS) >> MSTATUS_FS_SHIFT, 3);
        CHECK_U64(ms & MSTATUS_SD, MSTATUS_SD);
        th_free(&t);
    }
}

static int g_trace_calls;
static u64 g_trace_first_pc;

static void trace_count(void *ud, u64 pc, u32 insn, u32 len) {
    RVM_UNUSED(ud);
    RVM_UNUSED(insn);
    RVM_UNUSED(len);
    if (g_trace_calls == 0)
        g_trace_first_pc = pc;
    g_trace_calls++;
}

void test_cpu_trace_gate(void) {
    QUIET_BEGIN();

    rvm_trace_from(0x80001000);
    CHECK(!rvm_trace_armed(0x80000FFC));
    CHECK(rvm_trace_armed(0x80001000));
    CHECK(rvm_trace_armed(0x80000000));
    rvm_trace_from(0);
    CHECK(rvm_trace_armed(0xDEADBEEFULL));

    th t;
    CHECK(th_init(&t) == RVM_OK);
    u64 a0 = th_emit(&t, ADDI(X_A0, X_ZERO, 1));
    u64 a1 = th_emit(&t, ADDI(X_A1, X_ZERO, 2));
    th_emit(&t, ADDI(X_A2, X_ZERO, 3));

    rvm_trace_set(trace_count, NULL);

    g_trace_calls = 0;
    g_trace_first_pc = 0;
    rvm_trace_from(0);
    t.cpu.pc = a0;
    th_run_all(&t);
    CHECK_U64(g_trace_calls, 3);
    CHECK_U64(g_trace_first_pc, a0);
    CHECK_U64(t.cpu.x[X_A2], 3);

    g_trace_calls = 0;
    g_trace_first_pc = 0;
    rvm_trace_from(a1);
    t.cpu.pc = a0;
    th_run_all(&t);
    CHECK_U64(g_trace_calls, 2);
    CHECK_U64(g_trace_first_pc, a1);

    g_trace_calls = 0;
    rvm_trace_from(0x80FFFFFFULL);
    t.cpu.pc = a0;
    th_run_all(&t);
    CHECK_U64(g_trace_calls, 0);

    rvm_trace_set(NULL, NULL);
    rvm_trace_from(0);
    th_free(&t);
    QUIET_END();
}
