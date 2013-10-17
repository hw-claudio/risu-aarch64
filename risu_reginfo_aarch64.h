/******************************************************************************
 * Copyright (c) 2013 Linaro Limited
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     Claudio Fontana (Linaro) - initial implementation
 *     based on Peter Maydell's risu_arm.c
 *****************************************************************************/

#ifndef RISU_REGINFO_AARCH64_H
#define RISU_REGINFO_AARCH64_H

struct reginfo
{
    uint64_t fault_address;
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint32_t flags;
    uint32_t faulting_insn;

    /* FP/SIMD */
    uint32_t fpsr;
    uint32_t fpcr;
    __uint128_t vregs[32];
};

/* initialize structure from a ucontext */
void reginfo_init(struct reginfo *ri, ucontext_t *uc);

/* return 1 if structs are equal, 0 otherwise. */
int reginfo_is_eq(struct reginfo *r1, struct reginfo *r2);

/* print reginfo state to a stream, returns 1 on success, 0 on failure */
int reginfo_dump(struct reginfo *ri, FILE *f);

/* reginfo_dump_mismatch: print mismatch details to a stream, ret nonzero=ok */
int reginfo_dump_mismatch(struct reginfo *m, struct reginfo *a, FILE *f);

#endif /* RISU_REGINFO_AARCH64_H */
