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

#ifndef RISU_H
#define RISU_H

#include <stdint.h>

/* Socket related routines */
int master_connect(uint16_t port);
int apprentice_connect(const char *hostname, uint16_t port);
int send_data_pkt(int sock, void *pkt, int pktlen);
int recv_data_pkt(int sock, void *pkt, int pktlen);
void send_response_byte(int sock, int resp);

extern uint32_t image_start_address;

extern uint8_t *memblock;

extern int test_fp_exc;

/* Ops code under test can request from risu: */
#define OP_COMPARE 0
#define OP_TESTEND 1
#define OP_SETMEMBLOCK 2
#define OP_GETMEMBLOCK 3
#define OP_COMPAREMEM 4

/* The memory block should be this long */
#define MEMBLOCKLEN 8192

/* Interface provided by CPU-specific code: */

/* Send the register information from the struct ucontext down the socket.
 * Return the response code from the master.
 * NB: called from a signal handler.
 */
int send_register_info(int sock, void *uc);

/* Read register info from the socket and compare it with that from the
 * ucontext. Return 0 for match, 1 for end-of-test, 2 for mismatch.
 * NB: called from a signal handler.
 */
int recv_and_compare_register_info(int sock, void *uc);

/* Print a useful report on the status of the last comparison
 * done in recv_and_compare_register_info(). This is called on
 * exit, so need not restrict itself to signal-safe functions.
 * Should return 0 if it was a good match (ie end of test)
 * and 1 for a mismatch.
 */
int report_match_status(void);

/* Move the PC past this faulting insn by adjusting ucontext
 */
void advance_pc(void *uc);

#endif /* RISU_H */
