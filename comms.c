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

/* Routines for the socket communication between master and apprentice. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "risu.h"

int apprentice_connect(const char *hostname, int port)
{
   /* We are the client end of the TCP connection */
   int sock;
   struct sockaddr_in sa;
   sock = socket(PF_INET, SOCK_STREAM, 0);
   if (sock < 0)
   {
      perror("socket");
      exit(1);
   }
   struct hostent *hostinfo;
   sa.sin_family = AF_INET;
   sa.sin_port = htons(port);
   hostinfo = gethostbyname(hostname);
   if (!hostinfo)
   {
      fprintf(stderr, "Unknown host %s\n", hostname);
      exit(1);
   }
   sa.sin_addr = *(struct in_addr*)hostinfo->h_addr;
   if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0)
   {
      perror("connect");
      exit(1);
   }
   return sock;
}

int master_connect(int port)
{
   int sock;
   struct sockaddr_in sa;
   sock = socket(PF_INET, SOCK_STREAM, 0);
   if (sock < 0)
   {
      perror("socket");
      exit(1);
   }
   int sora = 1;
   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sora, sizeof(sora)) != 0)
   {
      perror("setsockopt(SO_REUSEADDR)");
      exit(1);
   }

   sa.sin_family = AF_INET;
   sa.sin_port = htons(port);
   sa.sin_addr.s_addr = htonl(INADDR_ANY);
   if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0)
   {
      perror("bind");
      exit(1);
   }
   if (listen(sock, 1) < 0)
   {
      perror("listen");
      exit(1);
   }
   /* Just block until we get a connection */
   fprintf(stderr, "master: waiting for connection on port %d...\n", port);
   struct sockaddr_in csa;
   socklen_t csasz = sizeof(csa);
   int nsock = accept(sock, (struct sockaddr*)&csa, &csasz);
   if (nsock < 0)
   {
      perror("accept");
      exit(1);
   }
   /* We're done with the server socket now */
   close(sock);
   return nsock;
}

/* Utility functions which are just wrappers around read and writev
 * to catch errors and retry on short reads/writes.
 */
static void recv_bytes(int sock, void *pkt, int pktlen)
{
   char *p = pkt;
   while (pktlen)
   {
      int i = read(sock, p, pktlen);
      if (i <= 0)
      {
         if (errno == EINTR)
         {
            continue;
         }
         perror("read failed");
         exit(1);
      }
      pktlen -= i;
      p += i;
   }
}

static void recv_and_discard_bytes(int sock, int pktlen)
{
   /* Read and discard bytes */
   char dumpbuf[64];
   while (pktlen)
   {
      int i;
      int len = sizeof(dumpbuf);
      if (len > pktlen)
      {
         len = pktlen;
      }
      i = read(sock, dumpbuf, len);
      if (i <= 0)
      {
         if (errno == EINTR)
         {
            continue;
         }
         perror("read failed");
         exit(1);
      }
      pktlen -= i;
   }
}

ssize_t safe_writev(int fd, struct iovec *iov_in, int iovcnt)
{
   /* writev, retrying for EINTR and short writes */
   int r = 0;
   struct iovec *iov = iov_in;
   for (;;)
   {
      ssize_t i = writev(fd, iov, iovcnt);
      if (i == -1)
      {
         if (errno == EINTR)
            continue;
         return -1;
      }
      r += i;
      /* Move forward through iov to account for data transferred */
      while (i >= iov->iov_len)
      {
         i -= iov->iov_len;
         iov++;
         iovcnt--;
         if (iovcnt == 0) {
            return r;
         }
      }
      iov->iov_len -= i;
   }
}

/* Low level comms routines:
 * send_data_pkt sends a block of data and waits for
 * a single byte response code.
 * recv_data_pkt receives a block of data.
 * send_response_byte sends the response code.
 * Note that both ends must agree on the length of the
 * block of data.
 */
int send_data_pkt(int sock, void *pkt, int pktlen)
{
   unsigned char resp;
   /* First we send the packet length as a network-order 32 bit value.
    * This avoids silent deadlocks if the two sides disagree over
    * what size data packet they are transferring. We use writev()
    * so that both length and packet are sent in one packet; otherwise
    * we get 300x slowdown because we hit Nagle's algorithm.
    */
   uint32_t net_pktlen = htonl(pktlen);
   struct iovec iov[2];
   iov[0].iov_base = &net_pktlen;
   iov[0].iov_len = sizeof(net_pktlen);
   iov[1].iov_base = pkt;
   iov[1].iov_len = pktlen;

   if (safe_writev(sock, iov, 2) == -1)
   {
      perror("writev failed");
      exit(1);
   }

   if (read(sock, &resp, 1) != 1)
   {
      perror("read failed");
      exit(1);
   }
   return resp;
}

int recv_data_pkt(int sock, void *pkt, int pktlen)
{
   uint32_t net_pktlen;
   recv_bytes(sock, &net_pktlen, sizeof(net_pktlen));
   net_pktlen = ntohl(net_pktlen);
   if (pktlen != net_pktlen)
   {
      /* Mismatch. Read the data anyway so we can send
       * a response back.
       */
      recv_and_discard_bytes(sock, net_pktlen);
      return 1;
   }
   recv_bytes(sock, pkt, pktlen);
   return 0;
}

void send_response_byte(int sock, int resp)
{
   unsigned char r = resp;
   if (write(sock, &r, 1) != 1)
   {
      perror("write failed");
      exit(1);
   }
}
