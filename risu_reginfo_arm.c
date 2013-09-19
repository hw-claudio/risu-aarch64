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

#include <stdio.h>
#include <ucontext.h>
#include <string.h>

#include "risu.h"
#include "risu_reginfo_arm.h"

extern int insnsize(ucontext_t *uc);

/* This is the data structure we pass over the socket.
 * It is a simplified and reduced subset of what can
 * be obtained with a ucontext_t*
 */

static void reginfo_init_vfp(struct reginfo *ri, ucontext_t *uc)
{
   // Read VFP registers. These live in uc->uc_regspace, which is
   // a sequence of
   //   u32 magic
   //   u32 size
   //   data....
   // blocks. We have to skip through to find the one for VFP.
   unsigned long *rs = uc->uc_regspace;

   for (;;)
   {
      switch (*rs++)
      {
         case 0:
         {
            /* We didn't find any VFP at all (probably a no-VFP
             * kernel). Zero out all the state to avoid mismatches.
             */
            int j;
            for (j = 0; j < 32; j++)
               ri->fpregs[j] = 0;
            ri->fpscr = 0;
            return;
         }
         case 0x56465001: /* VFP_MAGIC */
         {
            /* This is the one we care about. The format (after the size word)
             * is 32 * 64 bit registers, then the 32 bit fpscr, then some stuff
             * we don't care about.
             */
            int i;
            /* Skip if it's smaller than we expected (should never happen!) */
            if (*rs < ((32*2)+1))
            {
               rs += (*rs / 4);
               break;
            }
            rs++;
            for (i = 0; i < 32; i++)
            {
               ri->fpregs[i] = *rs++;
               ri->fpregs[i] |= (uint64_t)(*rs++) << 32;
            }
            /* Ignore the UNK/SBZP bits. We also ignore the cumulative
             * exception bits unless we were specifically asked to test
             * them on the risu command line -- too much of qemu gets
             * them wrong and they aren't actually very important.
             */
            ri->fpscr = (*rs) & 0xffff9f9f;
            if (!test_fp_exc) {
               ri->fpscr &= ~0x9f;
            }
            /* Clear the cumulative exception flags. This is a bit
             * unclean, but makes sense because otherwise we'd have to
             * insert explicit bit-clearing code in the generated code
             * to avoid the test becoming useless once all the bits
             * get set.
             */
            (*rs) &= ~0x9f;
            return;
         }
         default:
            /* Some other kind of block, ignore it */
            rs += (*rs / 4);
            break;
      }
   }
}

void reginfo_init(struct reginfo *ri, ucontext_t *uc)
{
   memset(ri, 0, sizeof(*ri)); /* necessary for memcmp later */

   ri->gpreg[0] = uc->uc_mcontext.arm_r0;
   ri->gpreg[1] = uc->uc_mcontext.arm_r1;
   ri->gpreg[2] = uc->uc_mcontext.arm_r2;
   ri->gpreg[3] = uc->uc_mcontext.arm_r3;
   ri->gpreg[4] = uc->uc_mcontext.arm_r4;
   ri->gpreg[5] = uc->uc_mcontext.arm_r5;
   ri->gpreg[6] = uc->uc_mcontext.arm_r6;
   ri->gpreg[7] = uc->uc_mcontext.arm_r7;
   ri->gpreg[8] = uc->uc_mcontext.arm_r8;
   ri->gpreg[9] = uc->uc_mcontext.arm_r9;
   ri->gpreg[10] = uc->uc_mcontext.arm_r10;
   ri->gpreg[11] = uc->uc_mcontext.arm_fp;
   ri->gpreg[12] = uc->uc_mcontext.arm_ip;
   ri->gpreg[14] = uc->uc_mcontext.arm_lr;
   ri->gpreg[13] = 0xdeadbeef;
   ri->gpreg[15] = uc->uc_mcontext.arm_pc - image_start_address;
   // Mask out everything except NZCVQ GE
   // In theory we should be OK to compare everything
   // except the reserved bits, but valgrind for one
   // doesn't fill in enough fields yet.
   ri->cpsr = uc->uc_mcontext.arm_cpsr & 0xF80F0000;

   ri->faulting_insn = *((uint16_t*)uc->uc_mcontext.arm_pc);
   ri->faulting_insn_size = insnsize(uc);
   if (ri->faulting_insn_size != 2)
   {
      ri->faulting_insn |= (*((uint16_t*)uc->uc_mcontext.arm_pc+1)) << 16;
   }

   reginfo_init_vfp(ri, uc);
}

/* reginfo_is_eq: compare the reginfo structs, returns nonzero if equal */
int reginfo_is_eq(struct reginfo *r1, struct reginfo *r2)
{
    return memcmp(r1, r2, sizeof(*r1)) == 0; /* ok since we memset 0 */
}

/* reginfo_dump: print the state to a stream, returns nonzero on success */
int reginfo_dump(struct reginfo *ri, FILE *f)
{
   int i;
   if (ri->faulting_insn_size == 2)
      fprintf(f, "  faulting insn %04x\n", ri->faulting_insn);
   else
      fprintf(f, "  faulting insn %08x\n", ri->faulting_insn);
   for (i = 0; i < 16; i++)
   {
      fprintf(f, "  r%d: %08x\n", i, ri->gpreg[i]);
   }
   fprintf(f, "  cpsr: %08x\n", ri->cpsr);
   for (i = 0; i < 32; i++)
   {
      fprintf(f, "  d%d: %016llx\n",
              i, (unsigned long long)ri->fpregs[i]);
   }
   fprintf(f, "  fpscr: %08x\n", ri->fpscr);

   return !ferror(f);
}

int reginfo_dump_mismatch(struct reginfo *m, struct reginfo *a,
                          FILE *f)
{
   int i;
   fprintf(f, "mismatch detail (master : apprentice):\n");

   if (m->faulting_insn_size != a->faulting_insn_size)
      fprintf(f, "  faulting insn size mismatch %d vs %d\n",
              m->faulting_insn_size, a->faulting_insn_size);
   else if (m->faulting_insn != a->faulting_insn)
   {
      if (m->faulting_insn_size == 2)
         fprintf(f, "  faulting insn mismatch %04x vs %04x\n",
                 m->faulting_insn, a->faulting_insn);
      else
         fprintf(f, "  faulting insn mismatch %08x vs %08x\n",
                 m->faulting_insn, a->faulting_insn);
   }
   for (i = 0; i < 16; i++)
   {
      if (m->gpreg[i] != a->gpreg[i])
         fprintf(f, "  r%d: %08x vs %08x\n", i, m->gpreg[i], a->gpreg[i]);
   }
   if (m->cpsr != a->cpsr)
      fprintf(f, "  cpsr: %08x vs %08x\n", m->cpsr, a->cpsr);
   for (i = 0; i < 32; i++)
   {
      if (m->fpregs[i] != a->fpregs[i])
         fprintf(f, "  d%d: %016llx vs %016llx\n", i,
                 (unsigned long long)m->fpregs[i],
                 (unsigned long long)a->fpregs[i]);
   }
   if (m->fpscr != a->fpscr)
      fprintf(f, "  fpscr: %08x vs %08x\n", m->fpscr, a->fpscr);

   return !ferror(f);
}
