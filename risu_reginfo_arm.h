/*****************************************************************************
 * Copyright (c) 2013 Linaro Limited
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     Peter Maydell (Linaro) - initial implementation
 *     Claudio Fontana (Linaro) - minor refactoring
 *****************************************************************************/

#ifndef RISU_REGINFO_ARM_H
#define RISU_REGINFO_ARM_H

struct reginfo
{
    uint64_t fpregs[32];
    uint32_t faulting_insn;
    uint32_t faulting_insn_size;
    uint32_t gpreg[16];
    uint32_t cpsr;
    uint32_t fpscr;
};

/* initialize a reginfo structure with data from uc */
void reginfo_init(struct reginfo *ri, ucontext_t *uc);

/* returns 1 if structs are equal, zero otherwise */
int reginfo_is_eq(struct reginfo *r1, struct reginfo *r2);

/* print struct values to a stream, return 0 on stream err, 1 on success */
int reginfo_dump(struct reginfo *ri, FILE *f);

/* print a detailed mismatch report, return 0 on stream err, 1 on success */
int reginfo_dump_mismatch(struct reginfo *m, struct reginfo *a, FILE *f);

#endif /* RISU_REGINFO_ARM_H */
