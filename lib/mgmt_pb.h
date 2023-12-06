// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MGMTD protobuf main header file
 * Copyright (C) 2021  Vmware, Inc.
 *		       Pushpasis Sarkar <spushpasis@vmware.com>
 */

#ifndef _FRR_MGMTD_PB_H_
#define _FRR_MGMTD_PB_H_

#include <libyang/libyang.h>

#include "lib/mgmt.pb-c.h"

#define mgmt_yang_data_xpath_init(ptr) mgmtd__yang_data_xpath__init(ptr)

#define mgmt_yang_data_value_init(ptr) mgmtd__yang_data_value__init(ptr)

#define mgmt_yang_data_init(ptr) mgmtd__yang_data__init(ptr)

#define mgmt_yang_data_reply_init(ptr) mgmtd__yang_data_reply__init(ptr)

#define mgmt_yang_cfg_data_req_init(ptr) mgmtd__yang_cfg_data_req__init(ptr)

#define mgmt_yang_get_data_req_init(ptr) mgmtd__yang_get_data_req__init(ptr)

static inline LYD_FORMAT mgmt_to_lyd_format(Mgmtd__YangDataFormat format)
{
	switch (format) {
	case MGMTD__YANG_DATA_FORMAT__JSON:
		return LYD_JSON;
	case MGMTD__YANG_DATA_FORMAT__XML:
		return LYD_XML;
	case MGMTD__YANG_DATA_FORMAT__BINARY:
		return LYD_LYB;
	case _MGMTD__YANG_DATA_FORMAT_IS_INT_SIZE:
		return LYD_UNKNOWN;
	}
	return LYD_UNKNOWN;
}

static inline Mgmtd__YangDataFormat lyd_to_mgmt_format(LYD_FORMAT format)
{
	switch (format) {
	case LYD_JSON:
		return MGMTD__YANG_DATA_FORMAT__JSON;
	case LYD_XML:
		return MGMTD__YANG_DATA_FORMAT__XML;
	case LYD_LYB:
		return MGMTD__YANG_DATA_FORMAT__BINARY;
	case LYD_UNKNOWN:
		return _MGMTD__YANG_DATA_FORMAT_IS_INT_SIZE;
	}
	return _MGMTD__YANG_DATA_FORMAT_IS_INT_SIZE;
}

#endif /* _FRR_MGMTD_PB_H_ */
