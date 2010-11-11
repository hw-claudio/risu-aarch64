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

int apprentice_connect(const char *hostname, uint16_t port)
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

int master_connect(uint16_t port)
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
   size_t csasz;
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

/* The communication protocol is very simple.
 * The apprentice sends a packet like this:
 *  4 bytes: faulting insn
 *  n bytes: register dump
 * The master replies with a single byte response:
 *  0: ok, continue  [registers matched]
 *  1: exit with status 0 [end of test]
 *  2: exit with status 1 [mismatch, fail]
 *
 * We can handle the variable-length packet at setup
 * (where the apprentice sends command line options,
 * random seed etc) by first sending the length alone
 * as a 4-byte packet, and then a packet of that length.
 */

int send_data_pkt(int sock, void *pkt, int pktlen)
{
   unsigned char resp;
   char *p = pkt;
   while (pktlen)
   {
      int i = write(sock, p, pktlen);
      if (i <= 0)
      {
         perror("write failed");
         exit(1);
      }
      pktlen -= i;
      p += i;
   }
   if (read(sock, &resp, 1) != 1)
   {
      perror("read failed");
      exit(1);
   }
   return resp;
}

void recv_data_pkt(int sock, void *pkt, int pktlen)
{
   /* We always read a fixed length packet */ 
   char *p = pkt;
   while (pktlen)
   {
      int i = read(sock, p, pktlen);
      if (i <= 0)
      {
         perror("read failed");
         exit(1);
      }
      pktlen -= i;
      p += i;
   }
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
