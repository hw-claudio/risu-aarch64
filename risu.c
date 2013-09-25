/******************************************************************************
 * Copyright (c) 2010 Linaro Limited
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *     Peter Maydell (Linaro) - initial implementation
 *****************************************************************************/


/* Random Instruction Sequences for Userspace */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <getopt.h>
#include <setjmp.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "risu.h"

void *memblock = 0;

int apprentice_socket, master_socket;

sigjmp_buf jmpbuf;

/* Should we test for FP exception status bits? */
int test_fp_exc = 0;

void master_sigill(int sig, siginfo_t *si, void *uc)
{
   switch (recv_and_compare_register_info(master_socket, uc))
   {
      case 0:
         /* match OK */
         advance_pc(uc);
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
         advance_pc(uc);
         return;
      case 1:
         /* end of test */
         exit(0);
      default:
         /* mismatch */
         exit(1);
   }
}

static void set_sigill_handler(void (*fn)(int, siginfo_t *, void *))
{
   struct sigaction sa;
   memset(&sa, 0, sizeof(struct sigaction));

   sa.sa_sigaction = fn;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIGILL, &sa, 0) != 0)
   {
      perror("sigaction");
      exit(1);
   }
}

typedef void entrypoint_fn(void);

uintptr_t image_start_address;
entrypoint_fn *image_start;

void load_image(const char *imgfile)
{
   /* Load image file into memory as executable */
   struct stat st;
   fprintf(stderr, "loading test image %s...\n", imgfile);
   int fd = open(imgfile, O_RDONLY);
   if (fd < 0)
   {
      fprintf(stderr, "failed to open image file %s\n", imgfile);
      exit(1);
   }
   if (fstat(fd, &st) != 0)
   {
      perror("fstat");
      exit(1);
   }
   size_t len = st.st_size;
   void *addr;

   /* Map writable because we include the memory area for store
    * testing in the image.
    */
   addr = mmap(0, len, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE, fd, 0);
   if (!addr)
   {
      perror("mmap");
      exit(1);
   }
   close(fd);
   image_start = addr;
   image_start_address = (uintptr_t)addr;
}

int master(int sock)
{
   if (sigsetjmp(jmpbuf, 1))
   {
      return report_match_status();
   }
   master_socket = sock;
   set_sigill_handler(&master_sigill);
   fprintf(stderr, "starting image\n");
   image_start();
   fprintf(stderr, "image returned unexpectedly\n");
   exit(1);
}

int apprentice(int sock)
{
   apprentice_socket = sock;
   set_sigill_handler(&apprentice_sigill);
   fprintf(stderr, "starting image\n");
   image_start();
   fprintf(stderr, "image returned unexpectedly\n");
   exit(1);
}

int ismaster;

int main(int argc, char **argv)
{
   // some handy defaults to make testing easier
   uint16_t port = 9191;
   char *hostname = "localhost";
   char *imgfile;
   int sock;

   // TODO clean this up later
   
   for (;;)
   {
      static struct option longopts[] = 
         {
            { "master", no_argument, &ismaster, 1 },
            { "host", required_argument, 0, 'h' },
            { "port", required_argument, 0, 'p' },
            { "test-fp-exc", no_argument, &test_fp_exc, 1 },
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

   imgfile = argv[optind];
   if (!imgfile)
   {
      fprintf(stderr, "must specify image file name\n");
      exit(1);
   }

   load_image(imgfile);
   
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

   
