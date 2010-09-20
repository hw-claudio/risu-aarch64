/* Random Instruction Sequences for Userspace 
 * Copyright 2010 Linaro Limited
 * TODO: license
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <getopt.h>
#include <setjmp.h>
#include <assert.h>

#include "risu.h"

typedef void sighandler_fn_t(int sig, siginfo_t *si, void *vuc);

sighandler_fn_t master_sigill, apprentice_sigill;

int apprentice_socket, master_socket;

sigjmp_buf jmpbuf;

void master_sigill(int sig, siginfo_t *si, void *uc)
{
   switch (recv_and_compare_register_info(master_socket, uc))
   {
      case 0:
         /* match OK */
         return;
      default:
         /* mismatch, or end of test */
         siglongjmp(jmpbuf, 1);
   }
}

void apprentice_sigill(int sig, siginfo_t *si, void *uc)
{
   switch (send_register_info(apprentice_socket, uc))
   {
      case 0:
         /* match OK */
         return;
      case 1:
         /* end of test */
         exit(0);
      default:
         /* mismatch */
         exit(1);
   }
}

static void set_sigill_handler(sighandler_fn_t *fn)
{
   struct sigaction sa;
   sa.sa_sigaction = fn;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIGILL, &sa, 0) != 0)
   {
      perror("sigaction");
      exit(1);
   }
}

int master(int sock)
{
   if (sigsetjmp(jmpbuf, 1))
   {
      return report_match_status();
   }
   master_socket = sock;
   set_sigill_handler(master_sigill);
   fprintf(stderr, "raising SIGILL\n");
   raise(SIGILL);
   assert(!"should never get here");
}

int apprentice(int sock)
{
   apprentice_socket = sock;
   set_sigill_handler(apprentice_sigill);
   fprintf(stderr, "raising SIGILL\n");
   raise(SIGILL);
   assert(!"should never get here");
}

int ismaster;

int main(int argc, char **argv)
{
   // some handy defaults to make testing easier
   uint16_t port = 9191;
   char *hostname = "localhost";
   int sock;
   

   // TODO clean this up later
   
   for (;;)
   {
      static struct option longopts[] = 
         {
            { "master", no_argument, &ismaster, 1 },
            { "host", required_argument, 0, 'h' },
            { "port", required_argument, 0, 'p' },
            { 0,0,0,0 }
         };
      int optidx = 0;
      int c = getopt_long(argc, argv, "h:p:", longopts, &optidx);
      if (c == -1)
      {
         break;
      }
      
      switch (c)
      {
         case 0:
         {
            /* flag set by getopt_long, do nothing */
            break;
         }
         case 'h':
         {
            hostname = optarg;
            break;
         }
         case 'p':
         {
            // FIXME err handling
            port = strtol(optarg, 0, 10);
            break;
         }
         case '?':
         {
            /* error message printed by getopt_long */
            exit(1);
         }
         default:
            abort();
      }
   }

   if (ismaster)
   {
      fprintf(stderr, "master port %d\n", port);
      sock = master_connect(port);
      return master(sock);
   }
   else
   {
      fprintf(stderr, "apprentice host %s port %d\n", hostname, port);
      sock = apprentice_connect(hostname, port);
      return apprentice(sock);
   }
}

   
