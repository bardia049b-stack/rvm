#include "fixtures/harness.h"
#include "test.h"

#define PLIC_PRIORITY_OFF_VAL(s)      (4ULL * (s))
#define PLIC_ENABLE_OFF_VAL(ctx, src) (0x2000ULL + 0x80ULL * (ctx))
#define PLIC_CONTEXT_OFF_VAL(ctx)     (0x200000ULL + 0x1000ULL * (ctx))

void test_plic_claim(void) {
    th t;
    CHECK(th_init(&t) == RVM_OK);

    const u32 SRC = 5;
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, 0);
    u64 v = 0;
    CHECK(plic_load(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_S) + 4, 4, &v));
    CHECK_U64(v, 0);

    plic_raise(&t.plic, SRC);
    CHECK_U64(t.plic.pending[0] & (1u << SRC), 1u << SRC);
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, 0);

    CHECK(plic_store(&t.plic, PLIC_PRIORITY_OFF_VAL(SRC), 4, 1));
    CHECK(plic_store(&t.plic, PLIC_ENABLE_OFF_VAL(PLIC_CTX_S, SRC), 4, 1u << SRC));
    CHECK(plic_store(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_S) + 0, 4, 0));
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, MIP_SEIP);

    CHECK(plic_load(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_S) + 4, 4, &v));
    CHECK_U64(v, SRC);
    CHECK_U64(t.plic.pending[0] & (1u << SRC), 0);
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, 0);

    CHECK(plic_store(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_S) + 4, 4, SRC));
    CHECK_U64(t.plic.claimed[PLIC_CTX_S], 0);
    plic_raise(&t.plic, SRC);
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, MIP_SEIP);
    CHECK_U64(t.plic.n_claim, 1);

    CHECK(plic_store(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_S) + 0, 4, 7));
    CHECK_U64(plic_update(&t.plic) & MIP_SEIP, 0);

    CHECK(plic_store(&t.plic, PLIC_ENABLE_OFF_VAL(PLIC_CTX_M, SRC), 4, 1u << SRC));
    CHECK(plic_store(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_M) + 0, 4, 0));
    CHECK_U64(plic_update(&t.plic) & MIP_MEIP, MIP_MEIP);
    CHECK(plic_load(&t.plic, PLIC_CONTEXT_OFF_VAL(PLIC_CTX_M) + 4, 4, &v));
    CHECK_U64(v, SRC);

    CHECK(plic_store(&t.plic, PLIC_PRIORITY_OFF_VAL(SRC), 4, 0xFFFFFFFF));
    CHECK_U64(t.plic.priority[SRC], 7);
    th_free(&t);
}
