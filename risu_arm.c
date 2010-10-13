/* Copyright 2010 Linaro Limited */

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
    uint32_t faulting_insn;
    uint32_t faulting_insn_size;
    uint32_t gpreg[16];
    uint32_t cpsr;
};

struct reginfo master_ri, apprentice_ri;

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
   fprintf(stderr, "pc at %lx\n", uc->uc_mcontext.arm_pc);
   uc->uc_mcontext.arm_pc += insnsize(uc);
   fprintf(stderr, "advancing pc to %lx\n", uc->uc_mcontext.arm_pc);
}

static int insn_is_eot_marker(uint32_t insn, int isz)
{
   if (isz == 2) 
   {
      return (insn == 0xdee1);
   }
   return (insn == 0xe7fe5af1);
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
   fprintf(stderr,"faulting insn %x size %d\n", ri->faulting_insn, ri->faulting_insn_size);
}

int send_register_info(int sock, void *uc)
{
   struct reginfo ri;
   fill_reginfo(&ri, uc);
   return send_data_pkt(sock, &ri, sizeof(ri));
}

/* Read register info from the socket and compare it with that from the
 * ucontext. Return 0 for match, 1 for end-of-test, 2 for mismatch.
 * NB: called from a signal handler.
 */
int recv_and_compare_register_info(int sock, void *uc)
{
   int resp;

   fill_reginfo(&master_ri, uc);
   recv_data_pkt(sock, &apprentice_ri, sizeof(apprentice_ri));
   if (memcmp(&master_ri, &apprentice_ri, sizeof(master_ri)) != 0)
   {
      /* mismatch */
      resp = 2;
   }
   else if (insn_is_eot_marker(master_ri.faulting_insn, master_ri.faulting_insn_size))
   {
      /* end of test */
      resp = 1;
   }
   else
   {
      /* either successful match or expected undef */
      resp = 0;
   }
   send_response_byte(sock, resp);
   return resp;
}

static void dump_reginfo(struct reginfo *ri)
{
   int i;
   fprintf(stderr, "  faulting insn %x\n", ri->faulting_insn);
   for (i = 0; i < 16; i++)
   {
      fprintf(stderr, "  r%d: %x\n", i, ri->gpreg[i]);
   }
   fprintf(stderr, "  cpsr: %x\n", ri->cpsr);
}

/* Print a useful report on the status of the last comparison
 * done in recv_and_compare_register_info(). This is called on
 * exit, so need not restrict itself to signal-safe functions.
 * Should return 0 if it was a good match (ie end of test)
 * and 1 for a mismatch.
 */
int report_match_status(void)
{
   fprintf(stderr, "match status...\n");
   fprintf(stderr, "master reginfo:\n");
   dump_reginfo(&master_ri);
   fprintf(stderr, "apprentice reginfo:\n");
   dump_reginfo(&apprentice_ri);
   if (memcmp(&master_ri, &apprentice_ri, sizeof(master_ri)) == 0)
   {
      fprintf(stderr, "match!\n");
      return 0;
   }
   fprintf(stderr, "mismatch!\n");
   return 1;
}


