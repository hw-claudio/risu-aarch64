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
      gregset_t gregs;
};

#ifndef REG_GS
/* Assume that either we get all these defines or none */
#define REG_GS 0
#define REG_FS 1
#define REG_ES 2
#define REG_DS 3
#define REG_ESP 7
#define REG_TRAPNO 12
#define REG_EIP 14
#define REG_EFL 16
#define REG_UESP 17
#endif

struct reginfo master_ri, apprentice_ri;

static int insn_is_ud2(uint32_t insn)
{
   return ((insn & 0xffff) == 0x0b0f);
}

void advance_pc(void *vuc)
{
   /* We assume that this is either UD1 or UD2.
    * This would need tweaking if we want to test
    * expected undefs on x86.
    */
   ucontext_t *uc = vuc;
   uc->uc_mcontext.gregs[REG_EIP] += 2;
}

static void fill_reginfo(struct reginfo *ri, ucontext_t *uc)
{
   int i;
   for (i = 0; i < NGREG; i++)
   {
      switch(i)
      {
         case REG_ESP:
         case REG_UESP:
         case REG_GS:
         case REG_FS:
         case REG_ES:
         case REG_DS:
         case REG_TRAPNO:
         case REG_EFL:
            /* Don't store these registers as it results in mismatches.
             * In particular valgrind has different values for some
             * segment registers, and they're boring anyway.
             * We really shouldn't be ignoring EFL but valgrind doesn't
             * seem to set it right and I don't care to investigate.
             */
            ri->gregs[i] = 0xDEADBEEF;
            break;
         case REG_EIP:
            /* Store the offset from the start of the test image */
            ri->gregs[i] = uc->uc_mcontext.gregs[i] - image_start_address;
            break;
         default:
            ri->gregs[i] = uc->uc_mcontext.gregs[i];
            break;
      }
   }
   /* x86 insns aren't 32 bit but we're not really testing x86 so
    * this is just to distinguish 'do compare' from 'stop'
    */
   ri->faulting_insn = *((uint32_t*)uc->uc_mcontext.gregs[REG_EIP]);
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
   else if (insn_is_ud2(master_ri.faulting_insn))
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

static char *regname[] = 
{
   "GS", "FS", "ES" ,"DS", "EDI", "ESI", "EBP", "ESP",
   "EBX", "EDX", "ECX", "EAX", "TRAPNO", "ERR", "EIP",
   "CS", "EFL", "UESP", "SS", 0
};

static void dump_reginfo(struct reginfo *ri)
{
   int i;
   fprintf(stderr, "  faulting insn %x\n", ri->faulting_insn);
   for (i = 0; i < NGREG; i++)
   {
      fprintf(stderr, "  %s: %x\n", regname[i] ? regname[i] : "???", ri->gregs[i]);
   }
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
