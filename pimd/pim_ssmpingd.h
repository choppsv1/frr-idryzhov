// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PIM for Quagga
 * Copyright (C) 2008  Everton da Silva Marques
 */

#ifndef PIM_SSMPINGD_H
#define PIM_SSMPINGD_H

#include <zebra.h>

#include "if.h"

#include "pim_iface.h"

struct ssmpingd_sock {
	struct pim_instance *pim;

	int sock_fd;		    /* socket */
	struct event *t_sock_read;  /* thread for reading socket */
	pim_addr source_addr;       /* source address */
	int64_t creation;	   /* timestamp of socket creation */
	int64_t requests;	   /* counter */
};

void pim_ssmpingd_init(struct pim_instance *pim);
void pim_ssmpingd_destroy(struct pim_instance *pim);

struct ssmpingd_sock *ssmpingd_new(struct pim_instance *pim,
					  pim_addr source_addr);
void ssmpingd_delete(struct ssmpingd_sock *ss);

void pim_ssmpingd_start(struct ssmpingd_sock *ss);
void pim_ssmpingd_stop(struct ssmpingd_sock *ss);

#endif /* PIM_SSMPINGD_H */
