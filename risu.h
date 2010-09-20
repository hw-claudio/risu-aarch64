/* Copyright 2010 Linaro Limited */

#ifndef RISU_H
#define RISU_H

#include <stdint.h>

/* Socket related routines */
int master_connect(uint16_t port);
int apprentice_connect(const char *hostname, uint16_t port);
int send_data_pkt(int sock, void *pkt, int pktlen);
void recv_data_pkt(int sock, void *pkt, int pktlen);
void send_response_byte(int sock, int resp);



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

#endif /* RISU_H */
