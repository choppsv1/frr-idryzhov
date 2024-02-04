// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PIM for FRR - PIM Instance
 * Copyright (C) 2017 Cumulus Networks, Inc.
 * Donald Sharp
 */
#include <zebra.h>

#include "hash.h"
#include "vrf.h"
#include "lib_errors.h"
#include "linklist.h"
#include "openbsd-tree.h"

#include "pimd.h"
#include "pim_instance.h"
#include "pim_ssm.h"
#include "pim_rpf.h"
#include "pim_rp.h"
#include "pim_mroute.h"
#include "pim_oil.h"
#include "pim_static.h"
#include "pim_ssmpingd.h"
#include "pim_vty.h"
#include "pim_bsm.h"
#include "pim_mlag.h"
#include "pim_sock.h"
#include "pim_msdp.h"
#include "pim_mlag.h"

static int pim_name_compare(const struct pim_instance *a, const struct pim_instance *b)
{
	return strcmp(a->name, b->name);
}

RB_GENERATE(pim_name_head, pim_instance, entry, pim_name_compare);

struct pim_name_head pim_instances = RB_INITIALIZER(&pim_instances);

static struct pim_instance *pim_lookup_by_name(const char *name)
{
	struct pim_instance pim;
	strlcpy(pim.name, name, sizeof(pim.name));
	return (RB_FIND(pim_name_head, &pim_instances, &pim));
}

static void pim_enable(struct pim_instance *pim)
{
	struct listnode *n;
	struct ssmpingd_sock *ss;
	struct pim_msdp_peer *mp;
	struct vrf *vrf = pim->vrf;
	struct interface *ifp;

	assert(vrf && vrf->vrf_id != VRF_UNKNOWN);

	pim_bsm_proc_init(pim);

	pim_upstream_init(pim);

	pim->reg_sock = pim_reg_sock();
	if (pim->reg_sock < 0)
		assert(0);

	pim_mroute_socket_enable(pim);

	pim_if_create_pimreg(pim);

	FOR_ALL_INTERFACES(vrf, ifp)
		if (if_is_operative(ifp))
			pim_if_start(ifp);

	for (ALL_LIST_ELEMENTS_RO(pim->ssmpingd_list, n, ss))
		pim_ssmpingd_start(ss);

	for (ALL_LIST_ELEMENTS_RO(pim->msdp.peer_list, n, mp))
		pim_msdp_peer_start(mp);
}

static void pim_disable(struct pim_instance *pim)
{
	struct listnode *n;
	struct ssmpingd_sock *ss;
	struct pim_msdp_peer *mp;
	struct vrf *vrf = pim->vrf;
	struct interface *ifp;

	assert(vrf && vrf->vrf_id != VRF_UNKNOWN);

	for (ALL_LIST_ELEMENTS_RO(pim->msdp.peer_list, n, mp))
		pim_msdp_peer_stop(mp);

	for (ALL_LIST_ELEMENTS_RO(pim->ssmpingd_list, n, ss))
		pim_ssmpingd_stop(ss);

	FOR_ALL_INTERFACES(vrf, ifp)
		if (if_is_operative(ifp))
			pim_if_stop(ifp);

	pim_if_delete_pimreg(pim);

	pim_mroute_socket_disable(pim);

	close(pim->reg_sock);

	pim_upstream_terminate(pim);

	pim_bsm_proc_free(pim);
}

void pim_instance_terminate(struct pim_instance *pim)
{
	struct vrf *vrf;

	vrf = pim->vrf;
	if (vrf) {
		if (vrf->vrf_id != VRF_UNKNOWN)
			pim_disable(pim);

		vrf->info = NULL;
		pim->vrf = NULL;
	}

	RB_REMOVE(pim_name_head, &pim_instances, pim);

	pim_ssmpingd_destroy(pim);

	pim_instance_mlag_terminate(pim);

	pim_oil_terminate(pim);

	pim_rp_free(pim);

	list_delete(&pim->static_routes);

	pim_ssm_terminate(pim->ssm_info);

	/* Traverse and cleanup rpf_hash */
	hash_clean_and_free(&pim->rpf_hash, (void *)pim_rp_list_hash_clean);

	pim_vxlan_exit(pim);

	pim_msdp_exit(pim);

	XFREE(MTYPE_PIM_PLIST_NAME, pim->spt.plist);
	XFREE(MTYPE_PIM_PLIST_NAME, pim->register_plist);

	pim_if_terminate(pim);

	XFREE(MTYPE_PIM_PIM_INSTANCE, pim);
}

struct pim_instance *pim_instance_init(const char *name)
{
	struct pim_instance *pim;
	struct vrf *vrf;
	char hash_name[64];

	pim = XCALLOC(MTYPE_PIM_PIM_INSTANCE, sizeof(struct pim_instance));

	strlcpy(pim->name, name, sizeof(pim->name));

	pim_if_init(pim);

	pim->mcast_if_count = 0;
	pim->keep_alive_time = PIM_KEEPALIVE_PERIOD;
	pim->rp_keep_alive_time = PIM_RP_KEEPALIVE_PERIOD;

	pim->ecmp_enable = false;
	pim->ecmp_rebalance_enable = false;

	pim->spt.switchover = PIM_SPT_IMMEDIATE;
	pim->spt.plist = NULL;

	pim_msdp_init(pim, router->master);
	pim_vxlan_init(pim);

	snprintf(hash_name, sizeof(hash_name), "PIM %s RPF Hash", name);
	pim->rpf_hash = hash_create_size(256, pim_rpf_hash_key, pim_rpf_equal,
					 hash_name);

	if (PIM_DEBUG_ZEBRA)
		zlog_debug("%s: NHT rpf hash init ", __func__);

	pim->ssm_info = pim_ssm_init();

	pim->static_routes = list_new();
	pim->static_routes->del = (void (*)(void *))pim_static_route_free;

	pim->send_v6_secondary = 1;

	pim->gm_socket = -1;

	pim_rp_init(pim);

	pim_oil_init(pim);

	pim_instance_mlag_init(pim);

	pim->last_route_change_time = -1;

	/* MSDP global timer defaults. */
	pim->msdp.hold_time = PIM_MSDP_PEER_HOLD_TIME;
	pim->msdp.keep_alive = PIM_MSDP_PEER_KA_TIME;
	pim->msdp.connection_retry = PIM_MSDP_PEER_CONNECT_RETRY_TIME;

	pim_ssmpingd_init(pim);

	RB_INSERT(pim_name_head, &pim_instances, pim);

	vrf = vrf_lookup_by_name(name);
	if (vrf) {
		pim->vrf = vrf;
		vrf->info = pim;

		if (vrf->vrf_id != VRF_UNKNOWN)
			pim_enable(pim);
	}

	return pim;
}

struct pim_instance *pim_get_pim_instance(vrf_id_t vrf_id)
{
	struct vrf *vrf = vrf_lookup_by_id(vrf_id);

	if (vrf)
		return vrf->info;

	return NULL;
}

static int pim_vrf_new(struct vrf *vrf)
{
	struct pim_instance *pim;

	zlog_debug("VRF Created: %s(%u)", vrf->name, vrf->vrf_id);

	pim = pim_lookup_by_name(vrf->name);
	if (pim) {
		vrf->info = pim;
		pim->vrf = vrf;
	}
	
	return 0;
}

static int pim_vrf_delete(struct vrf *vrf)
{
	struct pim_instance *pim = vrf->info;

	if (pim) {
		pim->vrf = NULL;
		vrf->info = NULL;
	}

	zlog_debug("VRF Deletion: %s(%u)", vrf->name, vrf->vrf_id);

	return 0;
}

/*
 * Code to turn on the pim instance that
 * we have created with new
 */
static int pim_vrf_enable(struct vrf *vrf)
{
	struct pim_instance *pim = (struct pim_instance *)vrf->info;

	zlog_debug("%s: for %s %u", __func__, vrf->name, vrf->vrf_id);

	if (pim)
		pim_enable(pim);

	return 0;
}

static int pim_vrf_disable(struct vrf *vrf)
{
	struct pim_instance *pim = (struct pim_instance *)vrf->info;

	if (pim)
		pim_disable(pim);

	zlog_debug("%s: for %s %u", __func__, vrf->name, vrf->vrf_id);

	return 0;
}

static int pim_vrf_config_write(struct vty *vty)
{
	struct pim_instance *pim;
	bool is_default;

	RB_FOREACH (pim, pim_name_head, &pim_instances) {
		is_default =  strmatch(pim->name, VRF_DEFAULT_NAME);

		if (!is_default)
			vty_frame(vty, "vrf %s\n", pim->name);

		pim_global_config_write_worker(pim, vty);

		if (!is_default)
			vty_endframe(vty, "exit-vrf\n!\n");
	}

	return 0;
}

void pim_vrf_init(void)
{
	vrf_init(pim_vrf_new, pim_vrf_enable, pim_vrf_disable, pim_vrf_delete);

	vrf_cmd_init(pim_vrf_config_write);
}

void pim_vrf_terminate(void)
{
	struct pim_instance *pim;

	RB_FOREACH (pim, pim_name_head, &pim_instances)
		pim_instance_terminate(pim);

	vrf_terminate();
}
