/*-
 * Copyright (c) 2014 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Client main function
 * ====================
 *
 * Steer system time based on NTP servers
 *
 */

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

#include "ntimed.h"
#include "ntp.h"
#include "udp.h"

#define PARAM_CLIENT PARAM_INSTANCE
#define PARAM_TABLE_NAME client_param_table
#include "param_instance.h"
#undef PARAM_TABLE_NAME
#undef PARAM_CLIENT

int
main_client(int argc, char *const *argv)
{
	int ch;
	struct ntp_peer *np;
	struct ntp_peerset *npl;
	struct todolist *tdl;
	struct combine_delta *cd;
	int fd;
	int npeer = 0;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	tdl = TODO_NewList();
	Time_Unix(tdl);

	PLL_Init();

	npl = NTP_PeerSet_New(NULL);

	Param_Register(client_param_table);
	NF_Init();

	while ((ch = getopt(argc, argv, "p:t:")) != -1) {
		switch(ch) {
		case 'p':
			Param_Tweak(NULL, optarg);
			break;
		case 't':
			ArgTracefile(optarg);
			break;
		default:
			Fail(NULL, 0,
			    "Usage %s [-p param] [-t tracefile] servers...",
			    argv[0]);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (ch = 0; ch < argc; ch++)
		npeer += NTP_PeerSet_Add(NULL, npl, argv[ch]);
	if (npeer == 0)
		Fail(NULL, 0, "No NTP peers found");

	Put(NULL, OCX_TRACE, "# NTIMED Format client 1.0\n");
	Put(NULL, OCX_TRACE, "# Found %d peers\n", npeer);

	Param_Report(NULL, OCX_TRACE);

#ifndef NOIPV6
	/* Attempt IPv6, fall back to IPv4 */
	fd = UdpTimedSocket(NULL, AF_INET6);
	if (fd < 0)
		fd = UdpTimedSocket(NULL, AF_INET);
#else
	fd = UdpTimedSocket(NULL, AF_INET);
#endif
	if (fd < 0)
		Fail(NULL, errno, "Could not open UDP socket");

	cd = CD_New();

	NTP_PeerSet_Foreach(np, npl) {
		NF_New(np);
		np->combiner = CD_AddSource(cd, np->hostname, np->ip);
	}

	NTP_PeerSet_Poll(NULL, npl, fd, tdl);

	(void)TODO_Run(NULL, tdl);

	return (0);
}
