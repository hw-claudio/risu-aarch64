/*******************************************************************************
 * Copyright (c) 2010 Linaro Limited
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     Peter Maydell (Linaro) - initial implementation
 *******************************************************************************/

#include <stdio.h>
#include <ucontext.h>
#include <string.h>

#include "risu.h"

/* This is the data structure we pass over the socket.
 * It is a simplified and reduced subset of what can
 * be obtained with a ucontext_t*
 */
struct reginfo
{
    uint64_t fpregs[32];
    uint32_t faulting_insn;
    uint32_t faulting_insn_size;
    uint32_t gpreg[16];
    uint32_t cpsr;
    uint32_t fpscr;
};

struct reginfo master_ri, apprentice_ri;

uint8_t apprentice_memblock[MEMBLOCKLEN];

static int mem_used = 0;
static int packet_mismatch = 0;

static int insnsize(ucontext_t *uc)
{
   /* Return instruction size in bytes of the
    * instruction at PC
    */
   if (uc->uc_mcontext.arm_cpsr & 0x20) 
   {
      uint16_t faulting_insn = *((uint16_t*)uc->uc_mcontext.arm_pc);
      switch (faulting_insn & 0xF800)
      {
         case 0xE800:
         case 0xF000:
         case 0xF800:
            /* 32 bit Thumb2 instruction */
            return 4;
         default:
            /* 16 bit Thumb instruction */
            return 2;
      }
   }
   /* ARM instruction */
   return 4;
}

void advance_pc(void *vuc)
{
   ucontext_t *uc = vuc;
   uc->uc_mcontext.arm_pc += insnsize(uc);
}

static void set_r0(void *vuc, uint32_t r0)
{
   ucontext_t *uc = vuc;
   uc->uc_mcontext.arm_r0 = r0;
}

static int get_risuop(uint32_t insn, int isz)
{
   /* Return the risuop we have been asked to do
    * (or -1 if this was a SIGILL for a non-risuop insn)
    */
   uint32_t op = insn & 0xf;
   uint32_t key = insn & ~0xf;
   uint32_t risukey = (isz == 2) ? 0xdee0 : 0xe7fe5af0;
   return (key != risukey) ? -1 : op;
}

static void fill_reginfo_vfp(struct reginfo *ri, ucontext_t *uc)
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
            /* Ignore the UNK/SBZP bits. Also ignore the cumulative exception
             * flags for the moment, because nobody actually cares about them
             * and qemu certainly gets them wrong...
             */
            ri->fpscr = (*rs) & 0xffff9f00;
            return;
         }
         default:
            /* Some other kind of block, ignore it */
            rs += (*rs / 4);
            break;
      }
   }
}

static void fill_reginfo(struct reginfo *ri, ucontext_t *uc)
{
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
   
   fill_reginfo_vfp(ri, uc);
}

int send_register_info(int sock, void *uc)
{
   struct reginfo ri;
   int op;
   fill_reginfo(&ri, uc);
   op = get_risuop(ri.faulting_insn, ri.faulting_insn_size);

   switch (op)
   {
      case OP_COMPARE:
      case OP_TESTEND:
      default:
         /* Do a simple register compare on (a) explicit request
          * (b) end of test (c) a non-risuop UNDEF
          */
         return send_data_pkt(sock, &ri, sizeof(ri));
      case OP_SETMEMBLOCK:
         memblock = (uint8_t*)ri.gpreg[0];
         break;
      case OP_GETMEMBLOCK:
         set_r0(uc, ri.gpreg[0] + (uint32_t)memblock);
         break;
      case OP_COMPAREMEM:
         return send_data_pkt(sock, memblock, MEMBLOCKLEN);
         break;
   }
   return 0;
}

/* Read register info from the socket and compare it with that from the
 * ucontext. Return 0 for match, 1 for end-of-test, 2 for mismatch.
 * NB: called from a signal handler.
 *
 * We don't have any kind of identifying info in the incoming data
 * that says whether it's register or memory data, so if the two
 * sides get out of sync then we will fail obscurely.
 */
int recv_and_compare_register_info(int sock, void *uc)
{
   int resp = 0, op;

   fill_reginfo(&master_ri, uc);
   op = get_risuop(master_ri.faulting_insn, master_ri.faulting_insn_size);

   switch (op)
   {
      case OP_COMPARE:
      case OP_TESTEND:
      default:
         /* Do a simple register compare on (a) explicit request
          * (b) end of test (c) a non-risuop UNDEF
          */
         if (recv_data_pkt(sock, &apprentice_ri, sizeof(apprentice_ri)))
         {
            packet_mismatch = 1;
            resp = 2;
         }
         else if (memcmp(&master_ri, &apprentice_ri, sizeof(master_ri)) != 0)
         {
            /* register mismatch */
            resp = 2;
         }
         else if (op == OP_TESTEND)
         {
            resp = 1;
         }
         send_response_byte(sock, resp);
         break;
      case OP_SETMEMBLOCK:
         memblock = (uint8_t*)master_ri.gpreg[0];
         break;
      case OP_GETMEMBLOCK:
         set_r0(uc, master_ri.gpreg[0] + (uint32_t)memblock);
         break;
      case OP_COMPAREMEM:
         mem_used = 1;
         if (recv_data_pkt(sock, apprentice_memblock, MEMBLOCKLEN))
         {
            packet_mismatch = 1;
            resp = 2;
         }
         else if (memcmp(memblock, apprentice_memblock, MEMBLOCKLEN) != 0)
         {
            /* memory mismatch */
            resp = 2;
         }
         send_response_byte(sock, resp);
         break;
   }
   return resp;
}

static void dump_reginfo(struct reginfo *ri)
{
   int i;
   if (ri->faulting_insn_size == 2)
      fprintf(stderr, "  faulting insn %04x\n", ri->faulting_insn);
   else
      fprintf(stderr, "  faulting insn %08x\n", ri->faulting_insn);
   for (i = 0; i < 16; i++)
   {
      fprintf(stderr, "  r%d: %08x\n", i, ri->gpreg[i]);
   }
   fprintf(stderr, "  cpsr: %08x\n", ri->cpsr);
   for (i = 0; i < 32; i++)
   {
      fprintf(stderr, "  d%d: %016llx\n", i, ri->fpregs[i]);
   }
   fprintf(stderr, "  fpscr: %08x\n", ri->fpscr);
}

static void report_mismatch_detail(struct reginfo *m, struct reginfo *a)
{
   int i;
   fprintf(stderr, "mismatch detail (master : apprentice):\n");
   if (m->faulting_insn_size != a->faulting_insn_size)
      fprintf(stderr, "  faulting insn size mismatch %d vs %d\n", m->faulting_insn_size, a->faulting_insn_size);
   else if (m->faulting_insn != a->faulting_insn)
   {
      if (m->faulting_insn_size == 2)
         fprintf(stderr, "  faulting insn mismatch %04x vs %04x\n", m->faulting_insn, a->faulting_insn);
      else
         fprintf(stderr, "  faulting insn mismatch %08x vs %08x\n", m->faulting_insn, a->faulting_insn);
   }
   for (i = 0; i < 16; i++)
   {
      if (m->gpreg[i] != a->gpreg[i])
         fprintf(stderr, "  r%d: %08x vs %08x\n", i, m->gpreg[i], a->gpreg[i]);
   }
   if (m->cpsr != a->cpsr)
      fprintf(stderr, "  cpsr: %08x vs %08x\n", m->cpsr, a->cpsr);
   for (i = 0; i < 32; i++)
   {
      if (m->fpregs[i] != a->fpregs[i])
         fprintf(stderr, "  d%d: %016llx vs %016llx\n", i, m->fpregs[i], a->fpregs[i]);
   }
   if (m->fpscr != a->fpscr)
      fprintf(stderr, "  fpscr: %08x vs %08x\n", m->fpscr, a->fpscr);
}

/* Print a useful report on the status of the last comparison
 * done in recv_and_compare_register_info(). This is called on
 * exit, so need not restrict itself to signal-safe functions.
 * Should return 0 if it was a good match (ie end of test)
 * and 1 for a mismatch.
 */
int report_match_status(void)
{
   int resp = 0;
   fprintf(stderr, "match status...\n");
   if (packet_mismatch)
   {
      fprintf(stderr, "packet mismatch (probably disagreement "
              "about UNDEF on load/store)\n");
      /* We don't have valid reginfo from the apprentice side
       * so stop now rather than printing anything about it.
       */
      fprintf(stderr, "master reginfo:\n");
      dump_reginfo(&master_ri);
      return 1;
   }
   if (memcmp(&master_ri, &apprentice_ri, sizeof(master_ri)) != 0)
   {
      fprintf(stderr, "mismatch on regs!\n");
      resp = 1;
   }
   if (mem_used && memcmp(memblock, &apprentice_memblock, MEMBLOCKLEN) != 0)
   {
      fprintf(stderr, "mismatch on memory!\n");
      resp = 1;
   }
   if (!resp) 
   {
      fprintf(stderr, "match!\n");
      return 0;
   }

   fprintf(stderr, "master reginfo:\n");
   dump_reginfo(&master_ri);
   fprintf(stderr, "apprentice reginfo:\n");
   dump_reginfo(&apprentice_ri);

   report_mismatch_detail(&master_ri, &apprentice_ri);
   return resp;
}
