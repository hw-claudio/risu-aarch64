/* Copyright 2010 Linaro Limited */

#include <stdio.h>

#include "risu.h"

int send_register_info(int sock, void *uc)
{
   return send_data_pkt(sock, "S", 1);
}

static unsigned char cmd;

/* Read register info from the socket and compare it with that from the
 * ucontext. Return 0 for match, 1 for end-of-test, 2 for mismatch.
 * NB: called from a signal handler.
 */
int recv_and_compare_register_info(int sock, void *uc)
{
   recv_data_pkt(sock, &cmd, 1);
   int resp = (cmd == 'S') ? 1 : 2;
   send_response_byte(sock, resp);
   return resp;
}

/* Print a useful report on the status of the last comparison
 * done in recv_and_compare_register_info(). This is called on
 * exit, so need not restrict itself to signal-safe functions.
 * Should return 0 if it was a good match (ie end of test)
 * and 1 for a mismatch.
 */
int report_match_status(void)
{
   fprintf(stderr, "match status: command %c\n", cmd);
   return (cmd == 'S') ? 0 : 1;
}
