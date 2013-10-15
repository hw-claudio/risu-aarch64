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

#include <stdio.h>
#include <ucontext.h>
#include <string.h>

#include "risu.h"
#include "risu_reginfo_aarch64.h"

/* reginfo_init: initialize with a ucontext */
void reginfo_init(struct reginfo *ri, ucontext_t *uc)
{
    int i;
    /* necessary to be able to compare with memcmp later */
    memset(ri, 0, sizeof(*ri));

    for (i = 0; i < 31; i++)
        ri->regs[i] = uc->uc_mcontext.regs[i];

    ri->sp = 0xdeadbeefdeadbeef;
    ri->pc = uc->uc_mcontext.pc - image_start_address;
    ri->flags = uc->uc_mcontext.pstate & 0xf0000000; /* get only flags */

    ri->fault_address = uc->uc_mcontext.fault_address;
    ri->faulting_insn = *((uint32_t *)uc->uc_mcontext.pc);
}

/* reginfo_is_eq: compare the reginfo structs, returns nonzero if equal */
int reginfo_is_eq(struct reginfo *r1, struct reginfo *r2)
{
    return memcmp(r1, r2, sizeof(*r1)) == 0;
}

/* reginfo_dump: print state to a stream, returns nonzero on success */
int reginfo_dump(struct reginfo *ri, FILE *f)
{
    int i;
    fprintf(f, "  faulting insn %08x\n", ri->faulting_insn);

    for (i = 0; i < 31; i++)
        fprintf(f, "  x%2d   : %016" PRIx64 "\n", i, ri->regs[i]);

    fprintf(f, "  sp    : %016" PRIx64 "\n", ri->sp);
    fprintf(f, "  pc    : %016" PRIx64 "\n", ri->pc);
    fprintf(f, "  flags : %08x\n", ri->flags);

    return !ferror(f);
}

/* reginfo_dump_mismatch: print mismatch details to a stream, ret nonzero=ok */
int reginfo_dump_mismatch(struct reginfo *m, struct reginfo *a, FILE *f)
{
    int i;
    fprintf(f, "mismatch detail (master : apprentice):\n");
    if (m->faulting_insn != a->faulting_insn) {
        fprintf(f, "  faulting insn mismatch %08x vs %08x\n",
                m->faulting_insn, a->faulting_insn);
    }
    for (i = 0; i < 31; i++) {
        if (m->regs[i] != a->regs[i])
            fprintf(f, "  x%2d   : %016" PRIx64 " vs %016" PRIx64 "\n",
                    i, m->regs[i], a->regs[i]);
    }

    if (m->sp != a->sp)
        fprintf(f, "  sp    : %016" PRIx64 " vs %016" PRIx64 "\n",
                m->sp, a->sp);

    if (m->pc != a->pc)
        fprintf(f, "  pc    : %016" PRIx64 " vs %016" PRIx64 "\n",
                m->pc, a->pc);

    if (m->flags != a->flags)
        fprintf(f, "  flags : %08x vs %08x\n", m->flags, a->flags);

    return !ferror(f);
}
