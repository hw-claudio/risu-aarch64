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
      gregset_t gregs;
};

struct reginfo master_ri, apprentice_ri;

static void fill_reginfo(struct reginfo *ri, ucontext_t *uc)
{
   int i;
   for (i = 0; i < NGREG; i++)
   {
      ri->gregs[i] = uc->uc_mcontext.gregs[i];
   }
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
   fill_reginfo(&master_ri, uc);
   recv_data_pkt(sock, &apprentice_ri, sizeof(apprentice_ri));
   int resp = 1;
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
