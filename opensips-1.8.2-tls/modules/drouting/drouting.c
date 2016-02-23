/*
 * $Id: drouting.c 9339 2012-10-18 08:00:49Z bogdan_iancu $
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Open SIP Server (OpenSIPS).
 *
 * DROUTING OpenSIPS-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * DROUTING OpenSIPS-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-system.ro
 *
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../db/db.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../rw_locking.h"
#include "../../action.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../mod_fix.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../mi/mi.h"
#include "../tm/tm_load.h"

#include "dr_load.h"
#include "prefix_tree.h"
#include "routing.h"
#include "dr_bl.h"


#define DR_PARAM_USE_WEIGTH         (1<<0)
#define DR_PARAM_RULE_FALLBACK      (1<<1)
#define DR_PARAM_STRICT_LEN         (1<<2)
#define DR_PARAM_ONLY_CHECK         (1<<3)
#define DR_PARAM_INTERNAL_TRIGGERED (1<<30)


/* probing related stuff */
static unsigned int dr_prob_interval = 30;
static str dr_probe_replies = {NULL,0};
struct tm_binds dr_tmb;
str dr_probe_method = str_init("OPTIONS");
str dr_probe_from = str_init("sip:prober@localhost");
static int* probing_reply_codes = NULL;
static int probing_codes_no = 0;

static int dr_disable(struct sip_msg *req);


/*** DB relatede stuff ***/
/* parameters  */
static str db_url = {NULL,0};
static str drg_table = str_init("dr_groups");
static str drd_table = str_init("dr_gateways");
static str drr_table = str_init("dr_rules");
static str drc_table = str_init("dr_carriers");
/* DRG use domain */
static int use_domain = 1;
int dr_default_grp = -1;
int dr_force_dns = 1;

/* DRG table columns */
static str drg_user_col = str_init("username");
static str drg_domain_col = str_init("domain");
static str drg_grpid_col = str_init("groupid");
/* variables */
static db_con_t  *db_hdl=0;     /* DB handler */
static db_func_t dr_dbf;        /* DB functions */

/* current dr data - pointer to a pointer in shm */
static rt_data_t **rdata = 0;

struct _dr_avp{
	unsigned short type; /* AVP ID */
	int name; /* AVP name*/
};

/* AVP used to store serial RURIs */
static struct _dr_avp ruri_avp = { 0, -1 };
static str ruri_avp_spec = str_init("$avp(0xad346b2f)");

/* AVP used to store GW IDs */
static struct _dr_avp gw_id_avp = { 0, -1 };
static str gw_id_avp_spec = str_init("$avp(0xad346b30)");

/* AVP used to store GW ATTRs */
static struct _dr_avp gw_attrs_avp = { 0, -1 };
static str gw_attrs_avp_spec = { NULL, 0};

/* AVP used to store RULE IDs */
static struct _dr_avp rule_id_avp = { 0, -1 };
static str rule_id_avp_spec = {NULL, 0};

/* AVP used to store RULE ATTRs */
static struct _dr_avp rule_attrs_avp = { 0, -1 };
static str rule_attrs_avp_spec = {NULL, 0};

/* AVP used to store RULE prefix */
static struct _dr_avp rule_prefix_avp = { 0, -1 };
static str rule_prefix_avp_spec = {NULL, 0};

/* AVP used to store CARRIER ID */
static struct _dr_avp carrier_id_avp = { 0, -1 };
static str carrier_id_avp_spec = {NULL, 0};

/* AVP used to store CARRIER ATTRs */
static struct _dr_avp carrier_attrs_avp = { 0, -1 };
static str carrier_attrs_avp_spec = {NULL, 0};

/* internal AVPs used for fallback */
static int avpID_store_ruri;
static int avpID_store_prefix;
static int avpID_store_index;
static int avpID_store_whitelist;
static int avpID_store_group;
static int avpID_store_flags;

/* statistic data */
int tree_size = 0;
int inode = 0;
int unode = 0;
static str attrs_empty = str_init("");

/* reader-writers lock for reloading the data */
static rw_lock_t *ref_lock = NULL; 

static int dr_init(void);
static int dr_child_init(int rank);
static int dr_exit(void);

static int fixup_do_routing(void** param, int param_no);
static int fixup_from_gw(void** param, int param_no);

static int do_routing(struct sip_msg* msg, dr_group_t *drg, int sort, gparam_t* wl);
static int do_routing_0(struct sip_msg* msg);
static int do_routing_123(struct sip_msg* msg, char* str1, char* str2, char *str3);
static int use_next_gw(struct sip_msg* msg);
static int is_from_gw_0(struct sip_msg* msg, char* str1, char* str2);
static int is_from_gw_1(struct sip_msg* msg, char* str1, char* str2);
static int is_from_gw_2(struct sip_msg* msg, char* str1, char* str2);
static int goes_to_gw_0(struct sip_msg* msg, char* f1, char* f2);
static int goes_to_gw_1(struct sip_msg* msg, char* f1, char* f2);
static int route2_carrier(struct sip_msg* msg, char* cr);
static int route2_gw(struct sip_msg* msg, char* gw);

static struct mi_root* dr_reload_cmd(struct mi_root *cmd_tree, void *param);
static struct mi_root* mi_dr_gw_status(struct mi_root *cmd, void *param);
static struct mi_root* mi_dr_cr_status(struct mi_root *cmd, void *param);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"do_routing",  (cmd_function)do_routing_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"do_routing",  (cmd_function)do_routing_123, 1,  fixup_do_routing, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"do_routing",  (cmd_function)do_routing_123, 2,  fixup_do_routing, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"do_routing",  (cmd_function)do_routing_123, 3,  fixup_do_routing, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"use_next_gw",  (cmd_function)use_next_gw,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"next_routing",  (cmd_function)use_next_gw,  0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_1,   1,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_2,   2,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"goes_to_gw",  (cmd_function)goes_to_gw_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|LOCAL_ROUTE},
	{"goes_to_gw",  (cmd_function)goes_to_gw_1,   1,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"goes_to_gw",  (cmd_function)goes_to_gw_1,   2,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|LOCAL_ROUTE},
	{"dr_disable", (cmd_function)dr_disable,      0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"route_to_carrier",(cmd_function)route2_carrier,1,fixup_pvar_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{"route_to_gw",     (cmd_function)route2_gw,     1,fixup_pvar_null, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|LOCAL_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",           STR_PARAM, &db_url.s        },
	{"drd_table",        STR_PARAM, &drd_table.s     },
	{"drr_table",        STR_PARAM, &drr_table.s     },
	{"drg_table",        STR_PARAM, &drg_table.s     },
	{"drc_table",        STR_PARAM, &drc_table.s     },
	{"use_domain",       INT_PARAM, &use_domain      },
	{"drg_user_col",     STR_PARAM, &drg_user_col.s  },
	{"drg_domain_col",   STR_PARAM, &drg_domain_col.s},
	{"drg_grpid_col",    STR_PARAM, &drg_grpid_col.s },
	{"ruri_avp",         STR_PARAM, &ruri_avp_spec.s },
	{"gw_id_avp",        STR_PARAM, &gw_id_avp_spec.s        },
	{"gw_attrs_avp",     STR_PARAM, &gw_attrs_avp_spec.s     },
	{"rule_id_avp",      STR_PARAM, &rule_id_avp_spec.s      },
	{"rule_attrs_avp",   STR_PARAM, &rule_attrs_avp_spec.s   },
	{"rule_prefix_avp",  STR_PARAM, &rule_prefix_avp_spec.s  },
	{"carrier_id_avp",   STR_PARAM, &carrier_id_avp_spec.s   },
	{"carrier_attrs_avp",STR_PARAM, &carrier_attrs_avp_spec.s},
	{"force_dns",        INT_PARAM, &dr_force_dns            },
	{"default_group",    INT_PARAM, &dr_default_grp          },
	{"define_blacklist", STR_PARAM|USE_FUNC_PARAM, (void*)set_dr_bl },
	{ "probing_interval",      INT_PARAM, &dr_prob_interval         },
	{ "probing_method",        STR_PARAM, &dr_probe_method.s        },
	{ "probing_from",          STR_PARAM, &dr_probe_from.s          },
	{ "probing_reply_codes",   STR_PARAM, &dr_probe_replies.s       },
	{0, 0, 0}
};


/*
 * Exported MI functions
 */
#define HLP1 "Params: none ; Forces drouting module to reload data from DB "\
	"into memory; A return string is returned only in case of error."
#define HLP2 "Params: [ gw_id [ status ]] ; Sets/gets the status of a GW; "\
	"If no gw_id is given, all gws will be listed; if a new status is give, "\
	"it will be pushed to the given GW."
#define HLP3 "Params: [ carrier_id [ status ]] ; Sets/gets the status of a " \
	"carrier; If no carrier_id is given, all carrier will be listed; if a " \
	"new status is give, it will be pushed to the given carrier."
static mi_export_t mi_cmds[] = {
	{ "dr_reload",         HLP1, dr_reload_cmd,    MI_NO_INPUT_FLAG, 0,  0},
	{ "dr_gw_status",      HLP2, mi_dr_gw_status,  0,                0,  0},
	{ "dr_carrier_status", HLP3, mi_dr_cr_status,  0,                0,  0},
	{ 0, 0, 0, 0, 0, 0}
};



struct module_exports exports = {
	"drouting",
	MODULE_VERSION,
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Exported parameters */
	0,               /* exported statistics */
	mi_cmds,         /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* additional processes */
	dr_init,         /* Module initialization function */
	(response_function) 0,
	(destroy_function) dr_exit,
	(child_init_function) dr_child_init /* per-child init function */
};


/** Probing Section **/

static int check_options_rplcode(int code)
{
	int i;

	for (i =0; i< probing_codes_no; i++) {
		if(probing_reply_codes[i] == code)
			return 1;
	}

	return 0;
}


static int dr_disable(struct sip_msg *req)
{
	struct usr_avp *avp;
	int_str id_val;
	pgw_t *gw;

	lock_start_read( ref_lock );

	avp = search_first_avp( AVP_VAL_STR|gw_id_avp.type, gw_id_avp.name,
		&id_val,0);
	if (avp==NULL) {
		LM_DBG(" no AVP ID ->nothing to disable\n");
		lock_stop_read( ref_lock );
		return -1;
	}

	gw = get_gw_by_id( (*rdata)->pgw_l, &id_val.s );
	if (gw!=NULL)
		gw->flags |= DR_DST_STAT_DSBL_FLAG;

	lock_stop_read( ref_lock );

	return 1;
}


static void dr_probing_callback( struct cell *t, int type,
		struct tmcb_params *ps )
{
	int code = ps->code;
	pgw_t *gw;

	if (!*ps->param) {
		LM_CRIT("BUG - reply to a DR probe with no ID (code=%d)\n", ps->code);
		return;
	}

	lock_start_read( ref_lock );

	gw = get_gw_by_internal_id( (*rdata)->pgw_l, (int)(long)(*ps->param) );
	if (gw==NULL)
		goto end;

	if ((code == 200) || check_options_rplcode(code)) {
		/* re-enable to DST  (if allowed) */
		if ( gw->flags&DR_DST_STAT_NOEN_FLAG )
			goto end;
		gw->flags &= ~DR_DST_STAT_DSBL_FLAG;
		goto end;
	}

	if (code>=400) {
		gw->flags |= DR_DST_STAT_DSBL_FLAG;
	}


end:
	lock_stop_read( ref_lock );

	return;
}


static void dr_prob_handler(unsigned int ticks, void* param)
{
	static char buff[1000] = {"sip:"};
	str uri;

	if (rdata==NULL || *rdata==NULL)
		return;

	lock_start_read( ref_lock );

	/* do probing */
	pgw_t *dst;

	/* go through all destinations */
	for( dst = (*rdata)->pgw_l ; dst ; dst=dst->next ) {
		/* dst requires probing ? */
		if ( dst->flags&DR_DST_STAT_NOEN_FLAG
			|| !( (dst->flags&DR_DST_PING_PERM_FLAG)  ||  /*permanent probing*/
					( dst->flags&DR_DST_PING_DSBL_FLAG 
					&& dst->flags&DR_DST_STAT_DSBL_FLAG  /*probing on disable*/
					)
				)
			)
			continue;

		memcpy(buff + 4, dst->ip_str.s, dst->ip_str.len);
		uri.s = buff;
		uri.len = dst->ip_str.len + 4;
		
		if (dr_tmb.t_request( &dr_probe_method, &uri, &uri,
		&dr_probe_from, NULL, NULL, NULL, dr_probing_callback,
		(void*)(long)dst->_id, NULL) < 0) {
			LM_ERR("probing failed\n");
		}

	}


	lock_stop_read( ref_lock );
}

static inline int dr_reload_data( void )
{
	rt_data_t *new_data;
	rt_data_t *old_data;

	new_data = dr_load_routing_info( &dr_dbf, db_hdl,
		&drd_table, &drc_table, &drr_table);
	if ( new_data==0 ) {
		LM_CRIT("failed to load routing info\n");
		return -1;
	}

	lock_start_write( ref_lock );

	/* no more activ readers -> do the swapping */
	old_data = *rdata;
	*rdata = new_data;

	lock_stop_write( ref_lock );

	/* destroy old data */
	if (old_data)
		free_rt_data( old_data, 1 );

	/* generate new blacklist from the routing info */
	populate_dr_bls((*rdata)->pgw_l);

	return 0;
}



static int dr_init(void)
{
	pv_spec_t avp_spec;
	str name;

	LM_INFO("Dynamic-Routing - initializing\n");

	/* check the module params */
	init_db_url( db_url , 0 /*cannot be null*/);

	drd_table.len = strlen(drd_table.s);
	if (drd_table.s[0]==0) {
		LM_CRIT("mandatory parameter \"DRD_TABLE\" found empty\n");
		goto error;
	}

	drr_table.len = strlen(drr_table.s);
	if (drr_table.s[0]==0) {
		LM_CRIT("mandatory parameter \"DRR_TABLE\" found empty\n");
		goto error;
	}

	drg_table.len = strlen(drg_table.s);
	if (drg_table.s[0]==0) {
		LM_CRIT("mandatory parameter \"DRG_TABLE\"  found empty\n");
		goto error;
	}

	drc_table.len = strlen(drc_table.s);
	if (drc_table.s[0]==0) {
		LM_CRIT("mandatory parameter \"DRC_TABLE\"  found empty\n");
		goto error;
	}

	drg_user_col.len = strlen(drg_user_col.s);
	drg_domain_col.len = strlen(drg_domain_col.s);
	drg_grpid_col.len = strlen(drg_grpid_col.s);

	/* fix specs for internal AVP (used for fallback) */
	name.s = "_dr_fb_ruri"; name.len=11;
	if ( parse_avp_spec( &name, &avpID_store_ruri)!=0 ) {
		LM_ERR("failed to init internal AVP for ruri\n");
		return E_UNSPEC;
	}
	name.s = "_dr_fb_prefix"; name.len=13;
	if ( parse_avp_spec( &name, &avpID_store_prefix)!=0 ) {
		LM_ERR("failed to init internal AVP for prefix\n");
		return E_UNSPEC;
	}
	name.s = "_dr_fb_index"; name.len=12;
	if ( parse_avp_spec( &name, &avpID_store_index)!=0 ) {
		LM_ERR("failed to init internal AVP for index\n");
		return E_UNSPEC;
	}
	name.s = "_dr_fb_whitelist"; name.len=16;
	if ( parse_avp_spec( &name, &avpID_store_whitelist)!=0 ) {
		LM_ERR("failed to init internal AVP for whitelist\n");
		return E_UNSPEC;
	}
	name.s = "_dr_fb_group"; name.len=12;
	if ( parse_avp_spec( &name, &avpID_store_group)!=0 ) {
		LM_ERR("failed to init internal AVP for group\n");
		return E_UNSPEC;
	}
	name.s = "_dr_fb_flags"; name.len=12;
	if ( parse_avp_spec( &name, &avpID_store_flags)!=0 ) {
		LM_ERR("failed to init internal AVP for flags\n");
		return E_UNSPEC;
	}

	/* fix AVP specs for parameters */

	ruri_avp_spec.len = strlen(ruri_avp_spec.s);
	if (pv_parse_spec( &ruri_avp_spec, &avp_spec)==0
	|| avp_spec.type!=PVT_AVP) {
		LM_ERR("malformed or non AVP [%.*s] for RURI AVP definition\n",
			ruri_avp_spec.len, ruri_avp_spec.s);
		return E_CFG;
	}
	if( pv_get_avp_name(0, &(avp_spec.pvp), &(ruri_avp.name),
	&(ruri_avp.type) )!=0) {
		LM_ERR("[%.*s]- invalid AVP definition for RURI AVP\n",
			ruri_avp_spec.len, ruri_avp_spec.s);
		return E_CFG;
	}

	gw_id_avp_spec.len = strlen(gw_id_avp_spec.s);
	if (pv_parse_spec( &gw_id_avp_spec, &avp_spec)==0
	|| avp_spec.type!=PVT_AVP) {
		LM_ERR("malformed or non AVP [%.*s] for ID AVP definition\n",
			gw_id_avp_spec.len, gw_id_avp_spec.s);
		return E_CFG;
	}
	if( pv_get_avp_name(0, &(avp_spec.pvp), &(gw_id_avp.name),
	&(gw_id_avp.type) )!=0) {
		LM_ERR("[%.*s]- invalid AVP definition for ID AVP\n",
			gw_id_avp_spec.len, gw_id_avp_spec.s);
		return E_CFG;
	}

	if (gw_attrs_avp_spec.s) {
		gw_attrs_avp_spec.len = strlen(gw_attrs_avp_spec.s);
		if (pv_parse_spec( &gw_attrs_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for ATTRS AVP definition\n",
				gw_attrs_avp_spec.len, gw_attrs_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(gw_attrs_avp.name),
		&(gw_attrs_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for ATTRS AVP\n",
				gw_attrs_avp_spec.len, gw_attrs_avp_spec.s);
			return E_CFG;
		}
	}

	if (rule_id_avp_spec.s) {
		rule_id_avp_spec.len = strlen(rule_id_avp_spec.s);
		if (pv_parse_spec( &rule_id_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for ID AVP definition\n",
				rule_id_avp_spec.len, rule_id_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(rule_id_avp.name),
		&(rule_id_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for ID AVP\n",
				rule_id_avp_spec.len, rule_id_avp_spec.s);
			return E_CFG;
		}
	}

	if (rule_attrs_avp_spec.s) {
		rule_attrs_avp_spec.len = strlen(rule_attrs_avp_spec.s);
		if (pv_parse_spec( &rule_attrs_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for ATTRS AVP definition\n",
				rule_attrs_avp_spec.len, rule_attrs_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(rule_attrs_avp.name),
		&(rule_attrs_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for ATTRS AVP\n",
				rule_attrs_avp_spec.len, rule_attrs_avp_spec.s);
			return E_CFG;
		}
	}

	if (rule_prefix_avp_spec.s) {
		rule_prefix_avp_spec.len = strlen(rule_prefix_avp_spec.s);
		if (pv_parse_spec( &rule_prefix_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for PREFIX AVP definition\n",
				rule_prefix_avp_spec.len, rule_prefix_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(rule_prefix_avp.name),
		&(rule_prefix_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for PREFIX AVP\n",
				rule_prefix_avp_spec.len, rule_prefix_avp_spec.s);
			return E_CFG;
		}
	}

	if (carrier_id_avp_spec.s) {
		carrier_id_avp_spec.len = strlen(carrier_id_avp_spec.s);
		if (pv_parse_spec( &carrier_id_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("bad or non AVP [%.*s] for carrier id AVP definition\n",
				carrier_id_avp_spec.len, carrier_id_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(carrier_id_avp.name),
		&(carrier_id_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for carrier id AVP\n",
				carrier_id_avp_spec.len, carrier_id_avp_spec.s);
			return E_CFG;
		}
	}

	if (carrier_attrs_avp_spec.s) {
		carrier_attrs_avp_spec.len = strlen(carrier_attrs_avp_spec.s);
		if (pv_parse_spec( &carrier_attrs_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("bad or non AVP [%.*s] for carrier attrs AVP definition\n",
				carrier_attrs_avp_spec.len, carrier_attrs_avp_spec.s);
			return E_CFG;
		}
		if( pv_get_avp_name(0, &(avp_spec.pvp), &(carrier_attrs_avp.name),
		&(carrier_attrs_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for carrier attrs AVP\n",
				carrier_attrs_avp_spec.len, carrier_attrs_avp_spec.s);
			return E_CFG;
		}
	}

	if (init_dr_bls()!=0) {
		LM_ERR("failed to init DR blacklists\n");
		return E_CFG;
	}

	/* data pointer in shm */
	rdata = (rt_data_t**)shm_malloc( sizeof(rt_data_t*) );
	if (rdata==0) {
		LM_CRIT("failed to get shm mem for data ptr\n");
		goto error;
	}
	*rdata = 0;

	/* create & init lock */
	if ((ref_lock = lock_init_rw()) == NULL) {
		LM_CRIT("failed to init lock\n");
		goto error;
	}

	/* bind to the mysql module */
	if (db_bind_mod( &db_url, &dr_dbf  )) {
		LM_CRIT("cannot bind to database module! "
			"Did you forget to load a database module ?\n");
		goto error;
	}

	if (!DB_CAPABILITY( dr_dbf, DB_CAP_QUERY)) {
		LM_CRIT( "database modules does not "
			"provide QUERY functions needed by DRounting module\n");
		return -1;
	}


	/* arm a function for probing */
	if (dr_prob_interval) {
		/* load TM API */
		if (load_tm_api(&dr_tmb)!=0) {
			LM_ERR("can't load TM API\n");
			return -1;
		}

		/* probing method */
		dr_probe_method.len = strlen(dr_probe_method.s);
		dr_probe_from.len = strlen(dr_probe_from.s);
		if (dr_probe_replies.s)
			dr_probe_replies.len = strlen(dr_probe_replies.s);

		/* register pinger function */
		if (register_timer( dr_prob_handler , NULL, dr_prob_interval)<0) {
			LM_ERR("failed to register probing handler\n");
			return -1;
		}

		if (dr_probe_replies.s) {
			dr_probe_replies.len = strlen(dr_probe_replies.s);
			if(parse_reply_codes( &dr_probe_replies, &probing_reply_codes,
			&probing_codes_no )< 0) {
				LM_ERR("Bad format for options_reply_code parameter"
					" - Need a code list separated by commas\n");
				return -1;
			}
		}

	}

	return 0;
error:
	if (ref_lock) {
		lock_destroy_rw( ref_lock );
		ref_lock = 0;
	}
	if (db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}
	if (rdata) {
		shm_free(rdata);
		rdata = 0;
	}
	return -1;
}



static int dr_child_init(int rank)
{
	/* only workers needs DB connection */
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	/* init DB connection */
	if ( (db_hdl=dr_dbf.init(&db_url))==0 ) {
		LM_CRIT("cannot initialize database connection\n");
		return -1;
	}

	/* child 1 load the routing info */
	if ( (rank==1) && dr_reload_data()!=0 ) {
		LM_CRIT("failed to load routing data\n");
		return -1;
	}

	/* set GROUP table for workers */
	if (dr_dbf.use_table( db_hdl, &drg_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drg_table.len, drg_table.s);
		return -1;
	}
	srand(getpid()+time(0)+rank);
	return 0;
}


static int dr_exit(void)
{
	/* close DB connection */
	if (db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}

	/* destroy data */
	if ( rdata) {
		if (*rdata)
			free_rt_data( *rdata, 1 );
		shm_free( rdata );
		rdata = 0;
	}

	/* destroy lock */
	if (ref_lock) {
		lock_destroy_rw( ref_lock );
		ref_lock = 0;
	}
	
	/* destroy blacklists */
	destroy_dr_bls();

	return 0;
}



static struct mi_root* dr_reload_cmd(struct mi_root *cmd_tree, void *param)
{
	int n;

	LM_INFO("dr_reload MI command received!\n");

	if ( (n=dr_reload_data())!=0 ) {
		LM_CRIT("failed to load routing data\n");
		goto error;
	}

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
error:
	return init_mi_tree( 500, "Failed to reload",16);
}



static inline int get_group_id(struct sip_uri *uri)
{
	db_key_t keys_ret[1];
	db_key_t keys_cmp[2];
	db_val_t vals_cmp[2];
	db_res_t* res;
	int n;

	/* user */
	keys_cmp[0] = &drg_user_col;
	vals_cmp[0].type = DB_STR;
	vals_cmp[0].nul  = 0;
	vals_cmp[0].val.str_val = uri->user;
	n = 1;

	if (use_domain) {
		keys_cmp[1] = &drg_domain_col;
		vals_cmp[1].type = DB_STR;
		vals_cmp[1].nul  = 0;
		vals_cmp[1].val.str_val = uri->host;
		n++;
	}

	keys_ret[0] = &drg_grpid_col;
	res = 0;

	if ( dr_dbf.query(db_hdl,keys_cmp,0,vals_cmp,keys_ret,n,1,0,&res)<0 ) {
		LM_ERR("DB query failed\n");
		goto error;
	}

	if (RES_ROW_N(res) == 0) {
		if (dr_default_grp!=-1)
			return dr_default_grp;
		LM_ERR("no group for user "
			"\"%.*s\"@\"%.*s\"\n", uri->user.len, uri->user.s,
			uri->host.len, uri->host.s);
		goto error;
	}
	if (res->rows[0].values[0].nul || res->rows[0].values[0].type!=DB_INT) {
		LM_ERR("null or non-integer group_id\n");
		goto error;
	}
	n = res->rows[0].values[0].val.int_val;

	dr_dbf.free_result(db_hdl, res);
	return n;
error:
	if (res)
		dr_dbf.free_result(db_hdl, res);
	return -1;
}



static inline str* build_ruri(struct sip_uri *uri, int strip, str *pri,
																str *hostport)
{
	static str uri_str;
	char *p;

	if (uri->user.len<=strip) {
		LM_ERR("stripping %d makes "
			"username <%.*s> null\n",strip,uri->user.len,uri->user.s);
		return 0;
	}

	uri_str.len = 4 /*sip:*/ + uri->user.len - strip +pri->len +
		(uri->passwd.s?(uri->passwd.len+1):0) + 1/*@*/ + hostport->len +
		(uri->params.s?(uri->params.len+1):0) +
		(uri->headers.s?(uri->headers.len+1):0);

	if ( (uri_str.s=(char*)pkg_malloc( uri_str.len + 1))==0) {
		LM_ERR("no more pkg mem\n");
		return 0;
	}

	p = uri_str.s;
	*(p++)='s';
	*(p++)='i';
	*(p++)='p';
	*(p++)=':';
	if (pri->len) {
		memcpy(p, pri->s, pri->len);
		p += pri->len;
	}
	memcpy(p, uri->user.s+strip, uri->user.len-strip);
	p += uri->user.len-strip;
	if (uri->passwd.len) {
		*(p++)=':';
		memcpy(p, uri->passwd.s, uri->passwd.len);
		p += uri->passwd.len;
	}
	*(p++)='@';
	memcpy(p, hostport->s, hostport->len);
	p += hostport->len;
	if (uri->params.len) {
		*(p++)=';';
		memcpy(p, uri->params.s, uri->params.len);
		p += uri->params.len;
	}
	if (uri->headers.len) {
		*(p++)='?';
		memcpy(p, uri->headers.s, uri->headers.len);
		p += uri->headers.len;
	}
	*p = 0;

	if (p-uri_str.s!=uri_str.len) {
		LM_CRIT("difference between allocated(%d)"
			" and written(%d)\n",uri_str.len,(int)(long)(p-uri_str.s));
		return 0;
	}
	return &uri_str;
}


static int do_routing_0(struct sip_msg* msg)
{
	return do_routing(msg, NULL, 0, NULL);
}

static int do_routing_123(struct sip_msg* msg, char* grp, char* param,
															char* white_list)
{
	str res = {0,0};
	int flags=0;
	char *p;

	if (param) {
		if (fixup_get_svalue(msg, (gparam_p)param, &res) !=0){
			LM_ERR("falied to extract flags\n");
			return -1;
		}

		for (p=res.s;p<res.s+res.len;p++)
		{
			switch (*p)
			{
				case 'W':
					flags |= DR_PARAM_USE_WEIGTH;
					LM_DBG("using weights in GW selection\n");
					break;
				case 'F':
					flags |= DR_PARAM_RULE_FALLBACK;
					LM_DBG("enabling rule fallback\n");
					break;
				case 'L':
					flags |= DR_PARAM_STRICT_LEN;
					LM_DBG("matching prefix with strict len\n");
					break;
				case 'C':
					flags |= DR_PARAM_ONLY_CHECK;
					LM_DBG("only check the prefix\n");
					break;
				default:
					LM_DBG("unknown flag : [%c] . Skipping\n",*p);
			}
		}
	}

	return do_routing(msg, (dr_group_t*)grp, flags, (gparam_t*)white_list);
}


static int use_next_gw(struct sip_msg* msg)
{
	struct usr_avp *avp, *avp_ru;
	unsigned int flags;
	gparam_t wl_list;
	dr_group_t grp;
	int_str val;
	str ruri;
	int ok;
	pgw_t * dst;

	while(1)
	{
		/* remove the old attrs */
		if (gw_attrs_avp.name!=-1) {
			avp = NULL;
			do {
				if (avp) destroy_avp(avp);
				avp = search_first_avp( gw_attrs_avp.type,
					gw_attrs_avp.name, NULL, 0);
			}while (avp && (avp->flags&AVP_VAL_STR)==0 );
			if (avp) destroy_avp(avp);
		}

		/* remove the old carrier ID */
		if (carrier_id_avp.name!=-1) {
			avp = NULL;
			do {
				if (avp) destroy_avp(avp);
				avp = search_first_avp( carrier_id_avp.type,
					carrier_id_avp.name, NULL, 0);
			}while (avp && (avp->flags&AVP_VAL_STR)==0 );
			if (avp) destroy_avp(avp);
		}

		/* remove the old carrier attrs */
		if (carrier_attrs_avp.name!=-1) {
			avp = NULL;
			do {
				if (avp) destroy_avp(avp);
				avp = search_first_avp( carrier_attrs_avp.type,
					carrier_attrs_avp.name, NULL, 0);
			}while (avp && (avp->flags&AVP_VAL_STR)==0 );
			if (avp) destroy_avp(avp);
		}

		/* remove old gw ID */
		avp = NULL;
		do {
			if (avp) destroy_avp(avp);
			avp = search_first_avp(gw_id_avp.type, gw_id_avp.name, NULL, 0);
		}while (avp && (avp->flags&AVP_VAL_STR)==0 );
		if (avp) destroy_avp(avp);

		/* search for the first RURI AVP containing a string */
		avp_ru = NULL;
		do {
			if (avp_ru) destroy_avp(avp_ru);
			avp_ru = search_first_avp( ruri_avp.type, ruri_avp.name, &val, 0);
		}while (avp_ru && (avp_ru->flags&AVP_VAL_STR)==0 );

		if (!avp_ru)
			goto rule_fallback;

		ruri = val.s;
		LM_DBG("new RURI set to <%.*s>\n", val.s.len,val.s.s);

		/* get value for next gw ID from avp */
		if (avp) {
			get_avp_val(avp, &val);
		} else {
			/* if no other ID found, simply use the GW as good */
			break;
		}

		/* we have an ID, so we can check the GW state */
		lock_start_read( ref_lock );
		dst = get_gw_by_id( (*rdata)->pgw_l, &val.s);
		if (dst && (dst->flags & DR_DST_STAT_DSBL_FLAG) == 0)
			ok = 1;

		lock_stop_read( ref_lock );

		if ( ok )
			break;

		/* search for the next available GW*/
		destroy_avp(avp_ru);
	}

	if (set_ruri( msg, &ruri)==-1) {
		LM_ERR("failed to rewite RURI\n");
		return -1;
	}

	destroy_avp(avp_ru);

	return 1;


rule_fallback:

	/* check if a "flags" AVP is there and if fallback allowed */
	avp = search_first_avp( 0, avpID_store_flags, &val, 0);
	if (avp==NULL || !(val.n & DR_PARAM_RULE_FALLBACK) )
		return -1;

	/* fallback allowed, fetch the rest of data from AVPs */
	flags = val.n | DR_PARAM_INTERNAL_TRIGGERED;

	if (search_first_avp( 0, avpID_store_group, &val, 0)==NULL) {
		LM_ERR("Cannot find group AVP during a fallback\n");
		goto fallback_failed;
	}
	grp.type = 0;
	grp.u.grp_id = val.n;

	if (search_first_avp( AVP_VAL_STR, avpID_store_whitelist, &val, 0)==NULL) {
		wl_list.type = 0;
	} else {
		wl_list.type = GPARAM_TYPE_STR;
		wl_list.v.sval = val.s;
		wl_list.v.sval.s[--wl_list.v.sval.len] = 0;
	}

	if (do_routing( msg, &grp, flags, wl_list.type?&wl_list:NULL)==1) {
		return 1;
	}

fallback_failed:
	/* prevent any more fallback by removing the flags AVP */
	destroy_avp(avp);
	return -1;
}


#define DR_MAX_GWLIST	64

static int sort_rt_dst(pgw_list_t *pgwl, unsigned short size,
											int weight, unsigned short *idx)
{
	unsigned short running_sum[DR_MAX_GWLIST];
	unsigned int i, first, weight_sum, rand_no;

	/* populate the index array */
	for( i=0 ; i<size ; i++ ) idx[i] = i;
	first = 0;

	if (weight==0)
		return 0;

	while (size-first>1) {
		/* calculate the running sum */
		for( i=first,weight_sum=0 ; i<size ; i++ ) {
			weight_sum += pgwl[ idx[i] ].weight ;
			running_sum[i] = weight_sum;
			LM_DBG("elen %d, weight=%d, sum=%d\n",i,
				pgwl[ idx[i] ].weight, running_sum[i]);
		}
		if (weight_sum) {
			/* randomly select number */
			rand_no = (unsigned int)(weight_sum*((float)rand()/RAND_MAX));
			LM_DBG("random number is %d\n",rand_no);
			/* select the element */
			for( i=first ; i<size ; i++ )
				if (running_sum[i]>=rand_no) break;
			if (i==size) {
				LM_CRIT("bug in weight sort\n");
				return -1;
			}
		} else {
			/* randomly select index */
			i = (unsigned int)((size-first)*((float)rand()/RAND_MAX));
		}
		LM_DBG("selecting element %d with weight %d\n",
			idx[i], pgwl[ idx[i] ].weight);
		/* "i" is the selected element : swap it with first position and
		   retake alg without first elem */
		rand_no = idx[i];
		idx[i] = idx[first];
		idx[first] = rand_no;
		first ++;
	}

	return 0;
}


inline static int push_gw_for_usage(struct sip_msg *msg, struct sip_uri *uri,
								pgw_t *gw , str *c_id, str *c_attrs, int idx)
{
	str *ruri;
	int_str val;

	/* build uri*/
	ruri = build_ruri( uri, gw->strip, &gw->pri, &gw->ip_str);
	if (ruri==0) {
		LM_ERR("failed to build new ruri\n");
		return -1;
	}

	LM_DBG("adding gw [%.*s] as \"%.*s\" in order %d\n",
			gw->id.len, gw->id.s, ruri->len, ruri->s, idx);

	/* first GW to be added ? */
	if (idx==0) {
		/* add to RURI */
		if (set_ruri( msg, ruri)!= 0 ) {
			LM_ERR("failed to set new RURI\n");
			goto error;
		}

	} else {

		/* add ruri as AVP */
		val.s = *ruri;
		if (add_avp_last( AVP_VAL_STR|(ruri_avp.type),ruri_avp.name, val)!=0 ) {
			LM_ERR("failed to insert ruri avp\n");
			goto error;
		}

	}

	/* add GW id avp */
	val.s = gw->id;
	LM_DBG("setting GW id [%.*s] as avp\n",val.s.len, val.s.s);
	if (add_avp_last( AVP_VAL_STR|(gw_id_avp.type),gw_id_avp.name, val)!=0 ) {
		LM_ERR("failed to insert ids avp\n");
		goto error;
	}

	/* add GW attrs avp */
	if (gw_attrs_avp.name!=-1) {
		val.s = gw->attrs.s? gw->attrs : attrs_empty;
		LM_DBG("setting GW attr [%.*s] as avp\n",val.s.len,val.s.s);
		if (add_avp_last(AVP_VAL_STR|(gw_attrs_avp.type),gw_attrs_avp.name,val)!=0){
			LM_ERR("failed to insert attrs avp\n");
			goto error;
		}
	}

	if (carrier_id_avp.name!=-1) {
		val.s = (c_id && c_id->s)? *c_id : attrs_empty ;
		LM_DBG("setting CR Id [%.*s] as avp\n",val.s.len,val.s.s);
		if (add_avp_last(AVP_VAL_STR|(carrier_id_avp.type),
		carrier_id_avp.name,val)!=0){
			LM_ERR("failed to insert attrs avp\n");
			goto error;
		}
	}

	if (carrier_attrs_avp.name!=-1) {
		val.s = (c_attrs && c_attrs->s)? *c_attrs : attrs_empty ;
		LM_DBG("setting CR attr [%.*s] as avp\n",val.s.len,val.s.s);
		if (add_avp_last(AVP_VAL_STR|(carrier_attrs_avp.type),
		carrier_attrs_avp.name,val)!=0){
			LM_ERR("failed to insert attrs avp\n");
			goto error;
		}
	}

	pkg_free(ruri->s);
	return 0;
error:
	pkg_free(ruri->s);
	return -1;
}


static inline int is_dst_in_list(void* dst, pgw_list_t *list, unsigned short len)
{
	unsigned short i;

	if (list==NULL)
		return 1;
	for( i=0 ; i<len ; i++ ) {
		if (dst==(void*)list[i].dst.gw)
			return 1;
	}
	return 0;
}


static int do_routing(struct sip_msg* msg, dr_group_t *drg, int flags,
														gparam_t* whitelist)
{
	unsigned short dsts_idx[DR_MAX_GWLIST];
	unsigned short carrier_idx[DR_MAX_GWLIST];
	struct to_body  *from;
	struct sip_uri  uri;
	rt_info_t  *rt_info;
	struct usr_avp *avp, *avp_prefix=NULL, *avp_index=NULL;
	str parsed_whitelist;
	pgw_list_t *dst, *cdst;
	pgw_list_t *wl_list;
	unsigned int prefix_len;
	unsigned int rule_idx;
	unsigned short wl_len;
	str username;
	int grp_id;
	int i, j, n;
	int_str val;
	str ruri;
	int ret;
	char tmp;
	char *ruri_buf;

	ret = -1;
	ruri_buf = NULL;

	if ( (*rdata)==0 || (*rdata)->pgw_l==0 ) {
		LM_DBG("empty routing table\n");
		goto error1;
	}

	/* do some cleanup first */
	destroy_avps( ruri_avp.type, ruri_avp.name, 1);
	destroy_avps( gw_id_avp.type, gw_id_avp.name, 1);
	if (gw_attrs_avp.name!=-1)
		destroy_avps( gw_attrs_avp.type, gw_attrs_avp.name, 1);
	if (rule_id_avp.name!=-1)
		destroy_avps( rule_id_avp.type, rule_id_avp.name, 1);
	if (rule_attrs_avp.name!=-1)
		destroy_avps( rule_attrs_avp.type, rule_attrs_avp.name, 1);
	if (rule_prefix_avp.name!=-1)
		destroy_avps( rule_prefix_avp.type, rule_prefix_avp.name, 1);

	if ( !(flags & DR_PARAM_INTERNAL_TRIGGERED) ) {
		/* not internally triggered, so get data from SIP msg */

		/* get the username from FROM_HDR */
		if (parse_from_header(msg)!=0) {
			LM_ERR("unable to parse from hdr\n");
			goto error1;
		}
		from = (struct to_body*)msg->from->parsed;
		/* parse uri */
		if (parse_uri( from->uri.s, from->uri.len, &uri)!=0) {
			LM_ERR("unable to parse from uri\n");
			goto error1;
		}

		/* get user's routing group */
		if(drg==NULL)
		{
			grp_id = get_group_id( &uri );
			if (grp_id<0) {
				LM_ERR("failed to get group id\n");
				goto error1;
			}
		} else {
			if(drg->type==0)
				grp_id = (int)drg->u.grp_id;
			else if(drg->type==1) {
				grp_id = 0; /* call get avp here */
				if((avp=search_first_avp( drg->u.avp_id.type,
				drg->u.avp_id.name, &val, 0))==NULL||(avp->flags&AVP_VAL_STR)) {
					LM_ERR( "failed to get group id\n");
					goto error1;
				}
				grp_id = val.n;
			} else
				grp_id = 0; 
		}

		/* get the number/RURI and make a copy of it */
		ruri = *GET_RURI(msg);
		ruri_buf = (char*)pkg_malloc(ruri.len);
		if (ruri_buf==NULL) {
			LM_ERR("no more pkg mem (needed %d)\n",ruri.len);
			goto error1;
		}
		memcpy(ruri_buf, ruri.s, ruri.len);
		ruri.s = ruri_buf;
		/* parse ruri */
		if (parse_uri( ruri.s, ruri.len, &uri)!=0) {
			LM_ERR("unable to parse RURI\n");
			goto error1;
		}
		username = uri.user;

		/* search all rules on dr tree (start from beginning) */
		rule_idx = 0;

	} else {

		/* resume index on the rule under same prefix */
		avp_index = search_first_avp( 0, avpID_store_index, &val, 0);
		if (avp_index==NULL) {
			LM_ERR("Cannot find index AVP during a fallback\n");
			goto error1;
		}
		rule_idx = val.n;

		/* prefix to resume with */
		avp_prefix = search_first_avp( AVP_VAL_STR, avpID_store_prefix, &val, 0);
		if (avp_prefix==NULL) {
			LM_ERR("Cannot find prefix AVP during a fallback\n");
			goto error1;
		}
		username = val.s;
		/* still something to look for ? */
		if (username.len==0) return -1;

		/* original RURI to be used when building RURIs for new attempts */
		if (search_first_avp( AVP_VAL_STR, avpID_store_ruri, &val, 0)==NULL) {
			LM_ERR("Cannot find ruri AVP during a fallback\n");
			goto error1;
		}
		if (parse_uri( val.s.s, val.s.len, &uri)!=0) {
			LM_ERR("unable to parse RURI from AVP\n");
			goto error1;
		}

		grp_id = (int)drg->u.grp_id;
		ruri.s = NULL; ruri.len = 0;
	}


	LM_DBG("using dr group %d, rule_idx %d, username %.*s\n",
		grp_id,rule_idx,username.len,username.s);

	/* ref the data for reading */
	lock_start_read( ref_lock );

	/* search a prefix */
	rt_info = get_prefix( (*rdata)->pt, &username , (unsigned int)grp_id,
			&prefix_len, &rule_idx);

	if (flags & DR_PARAM_STRICT_LEN) {
		if (rt_info==NULL || prefix_len!=username.len)
			goto error2;
	}

	if (rt_info==0) {
		LM_DBG("no matching for prefix \"%.*s\"\n",
			username.len, username.s);
		/* try prefixless rules */
		rt_info = check_rt( &(*rdata)->noprefix, (unsigned int)grp_id);
		if (rt_info==0) {
			LM_DBG("no prefixless matching for "
				"grp %d\n", grp_id);
			goto error2;
		}
		prefix_len = 0;
	}

	if (rt_info->route_idx>0 && rt_info->route_idx<RT_NO) {
		ret = run_top_route( rlist[rt_info->route_idx].a, msg );
		if (ret&ACT_FL_DROP) {
			/* drop the action */
			LM_DBG("script route %s drops routing "
				"by %d\n", rlist[rt_info->route_idx].name, ret);
			ret = -1;
			goto error2;
		}
		ret = -1;
	}

	/* add RULE prefix avp - we do it now, as URI may change */
	if (rule_prefix_avp.name!=-1) {
		val.s.s = username.s ;
		val.s.len = prefix_len;
		LM_DBG("setting RULE prefix [%.*s] \n",val.s.len,val.s.s);
		if (add_avp( AVP_VAL_STR|(rule_prefix_avp.type),
		rule_prefix_avp.name, val)!=0 ) {
			LM_ERR("failed to insert rule prefix avp\n");
			goto error2;
		}
	}

	/* do we have anything left to failover to ? */
	if (prefix_len==0 && rule_idx==0)
		/* disable failover as nothing left */
		flags = flags & ~DR_PARAM_RULE_FALLBACK;

	if ( flags & DR_PARAM_RULE_FALLBACK ) {
		if ( !(flags & DR_PARAM_INTERNAL_TRIGGERED) ) {
			/* first time -? we need to save some date, to be able to 
			   do the rule fallback later in "next_gw" */
			LM_DBG("saving rule_idx %d, prefix %.*s\n",rule_idx,
				prefix_len - (rule_idx?0:1), username.s);
			val.n = rule_idx;
			if (add_avp( 0 , avpID_store_index, val) ) {
				LM_ERR("failed to insert index avp for fallback\n");
				flags = flags & ~DR_PARAM_RULE_FALLBACK;
			}
			/* if no rules available on current prefix (index is 0), simply
			   reduce the len of the prefix from start, to lookup another
			   prefix in the DR tree */
			val.s.s = username.s ;
			val.s.len = prefix_len - (rule_idx?0:1);
			if (add_avp( AVP_VAL_STR, avpID_store_prefix, val) ) {
				LM_ERR("failed to insert prefix avp for fallback\n");
				flags = flags & ~DR_PARAM_RULE_FALLBACK;
			}
			/* also store current ruri as we will need it */
			val.s = ruri;
			if (add_avp( AVP_VAL_STR, avpID_store_ruri, val) ) {
				LM_ERR("failed to insert ruri avp for fallback\n");
				flags = flags & ~DR_PARAM_RULE_FALLBACK;
			}
		} else {
			/* update the fallback coordonats for next resume */
			/* using ugly hack by directly accessing the AVP data in order
			   to perform changes - we want to avoid re-creating the AVP -bogdan */
			avp_index->data = (void *)(long)rule_idx;
			if (rule_idx==0) {
				void *data;
				/* all rules under current prefix used -> reduce the prefix */
				data = (void*)&avp_prefix->data;
				((str*)data)->len = prefix_len-1;
			}
			LM_DBG("updating to %d, prefix %.*s \n",rule_idx,
				prefix_len-(rule_idx?1:0),username.s);
		}
	}


	n = 0;
	wl_list = NULL;

	/* if only checking the prefix, we are done here */
	if (flags & DR_PARAM_ONLY_CHECK)
		goto no_gws;

	if (rt_info->pgwl==NULL) {
		LM_WARN("no destination for dr group %d, rule_idx %d, username %.*s\n",
			grp_id,rule_idx,username.len,username.s);
		goto error2;
	}

	/* sort the destination elements in the rule */
	i = sort_rt_dst(rt_info->pgwl, rt_info->pgwa_len,
		flags&DR_PARAM_USE_WEIGTH, dsts_idx);
	if (i!=0) {
		LM_ERR("failed to sort destinations in rule\n");
		goto error2;
	}

	/* evaluate and parse the whitelist of GWs/CARRIERs, if provided */
	if (whitelist) {
		if (fixup_get_svalue(msg, whitelist, &parsed_whitelist)!=0) {
			LM_ERR("failed to evaluate whitelist-> ignoring...\n");
		} else {
			tmp = parsed_whitelist.s[parsed_whitelist.len];
			parsed_whitelist.s[parsed_whitelist.len] = 0;
			if (parse_destination_list( *rdata, parsed_whitelist.s,
			&wl_list, &wl_len, 1)!=0) {
				LM_ERR("invalid format in whitelist-> ignoring...\n");
				wl_list = NULL;
			}
			parsed_whitelist.s[parsed_whitelist.len] = tmp;
		}
	}

	/* iterate through the list, skip the disabled destination */
	for ( i=0 ; i<rt_info->pgwa_len ; i++ ) {

		dst = &rt_info->pgwl[dsts_idx[i]];

		/* is the destination carrier or gateway ? */
		if (dst->is_carrier) {

			/* is carrier turned off ? */
			if( dst->dst.carrier->flags & DR_CR_FLAG_IS_OFF
			|| !is_dst_in_list( (void*)dst, wl_list, wl_len) )
				continue;

			/* any gws for this carrier ? */
			if( dst->dst.carrier->pgwl==NULL )
				continue;

			/* sort the gws of the carrier */
			j = sort_rt_dst(dst->dst.carrier->pgwl, dst->dst.carrier->pgwa_len,
				dst->dst.carrier->flags&DR_CR_FLAG_WEIGHT, carrier_idx);
			if (j!=0) {
				LM_ERR("failed to sort gws for carrier <%.*s>, skipping\n",
					dst->dst.carrier->id.len, dst->dst.carrier->id.s);
				continue;
			}

			/* iterate through the list of GWs provided by carrier */
			for ( j=0 ; j<dst->dst.carrier->pgwa_len ; j++ ) {

				cdst = &dst->dst.carrier->pgwl[carrier_idx[j]];

				/* is gateway disabled ? */
				if (cdst->dst.gw->flags & DR_DST_STAT_DSBL_FLAG ) {
					/*ignore it*/
				} else {
					/* add gateway to usage list */
					if ( push_gw_for_usage(msg, &uri, cdst->dst.gw ,
					&dst->dst.carrier->id, &dst->dst.carrier->attrs, n ) ) {
						LM_ERR("failed to use gw <%.*s>, skipping\n",
							cdst->dst.gw->id.len, cdst->dst.gw->id.s);
					} else {
						n++;
						if (dst->dst.carrier->flags&DR_CR_FLAG_FIRST)
							/* use only first valid GW */
							break;
					}
				}

			}

		} else {

			/* is gateway disabled ? */
			if (dst->dst.gw->flags & DR_DST_STAT_DSBL_FLAG
			|| !is_dst_in_list( (void*)dst, wl_list, wl_len) )
				continue;

			/* add gateway to usage list */
			if ( push_gw_for_usage(msg, &uri, dst->dst.gw, NULL, NULL, n) ) {
				LM_ERR("failed to use gw <%.*s>, skipping\n",
					dst->dst.gw->id.len, dst->dst.gw->id.s);
			} else {
				n++;
			}

		}

	}

	if( n < 1) {
		LM_ERR("All the gateways are disabled\n");
		goto error2;
	}

no_gws:
	/* add RULE attrs avp */
	if (rule_attrs_avp.name!=-1) {
		val.s = rt_info->attrs.s ? rt_info->attrs : attrs_empty;
		LM_DBG("setting RULE attr [%.*s] \n",val.s.len,val.s.s);
		if (add_avp( AVP_VAL_STR|(rule_attrs_avp.type),
		rule_attrs_avp.name, val)!=0 ) {
			LM_ERR("failed to insert rule attrs avp\n");
			goto error2;
		}
	}

	/* add RULE id avp */
	if (rule_id_avp.name!=-1) {
		val.n = (int) rt_info->id;
		LM_DBG("setting RULE id [%d] as avp\n",val.n);
		if (add_avp( rule_id_avp.type,rule_id_avp.name, val)!=0 ) {
			LM_ERR("failed to insert rule ids avp\n");
			goto error2;
		}
	}

	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );

	if ( (flags & DR_PARAM_RULE_FALLBACK) && !(flags & DR_PARAM_INTERNAL_TRIGGERED)) {
		/* we need to save a some date, to be able to do the rule 
		   fallback later in "next_gw" (prefix/index already added) */
		if (wl_list) {
			val.s = parsed_whitelist ;
			val.s.len++; /* we need extra space to place \0 when using */
			if (add_avp( AVP_VAL_STR, avpID_store_whitelist, val) ) {
				LM_ERR("failed to insert whitelist avp for fallback\n");
				flags = flags & ~DR_PARAM_RULE_FALLBACK;
			}
		}
		val.n = grp_id ;
		if (add_avp( 0, avpID_store_group, val) ) {
			LM_ERR("failed to insert group avp for fallback\n");
			flags = flags & ~DR_PARAM_RULE_FALLBACK;
		}
		val.n = flags ;
		if (add_avp( 0, avpID_store_flags, val) ) {
			LM_ERR("failed to insert flags avp for fallback\n");
		}
	}

	if (ruri_buf) pkg_free(ruri_buf);
	return 1;
error2:
	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );
error1:
	if (ruri_buf) pkg_free(ruri_buf);
	return ret;
}


static int route2_carrier(struct sip_msg* msg, char *cr_str)
{
	unsigned short carrier_idx[DR_MAX_GWLIST];
	struct sip_uri  uri;
	pgw_list_t *cdst;
	pv_value_t val;
	pcr_t *cr;
	str *ruri;
	int j,n;

	/* get the carrier ID */
	if ( pv_get_spec_value(msg, (pv_spec_p)cr_str, &val)!=0 ||
	(val.flags&PV_VAL_STR)==0 ) {
		LM_ERR("failed to get string value for carrier ID\n");
		return -1;
	}

	/* do some cleanup first */
	destroy_avps( ruri_avp.type, ruri_avp.name, 1);
	destroy_avps( gw_id_avp.type, gw_id_avp.name, 1);
	if (gw_attrs_avp.name!=-1)
		destroy_avps( gw_attrs_avp.type, gw_attrs_avp.name, 1);
	if (rule_id_avp.name!=-1)
		destroy_avps( rule_id_avp.type, rule_id_avp.name, 1);
	if (rule_attrs_avp.name!=-1)
		destroy_avps( rule_attrs_avp.type, rule_attrs_avp.name, 1);
	if (rule_prefix_avp.name!=-1)
		destroy_avps( rule_prefix_avp.type, rule_prefix_avp.name, 1);

	/* get the RURI */
	ruri = GET_RURI(msg);
	/* parse ruri */
	if (parse_uri( ruri->s, ruri->len, &uri)!=0) {
		LM_ERR("unable to parse RURI\n");
		return -1;
	}

	/* ref the data for reading */
	lock_start_read( ref_lock );

	cr = get_carrier_by_id( (*rdata)->carriers, &val.rs );
	if (cr==NULL) {
		LM_ERR("carrier <%.*s> was not found\n",val.rs.len, val.rs.s );
		goto error;
	}

	/* is carrier turned off ? */
	if( cr->flags & DR_CR_FLAG_IS_OFF ) {
		LM_NOTICE("routing to disabled carrier <%.*s> failed\n",
			cr->id.len, cr->id.s);
		goto error;
	}

	/* any GWs for the carrier? */
	if (cr->pgwl==NULL)
		goto no_gws;

	/* sort the gws of the carrier */
	j = sort_rt_dst( cr->pgwl, cr->pgwa_len, cr->flags&DR_CR_FLAG_WEIGHT,
		carrier_idx);
	if (j!=0) {
		LM_ERR("failed to sort gws for carrier <%.*s>, skipping\n",
			cr->id.len, cr->id.s);
		goto error;
	}

	/* iterate through the list of GWs provided by carrier */
	for ( j=0,n=0 ; j<cr->pgwa_len ; j++ ) {

		cdst = &cr->pgwl[carrier_idx[j]];

		/* is gateway disabled ? */
		if (cdst->dst.gw->flags & DR_DST_STAT_DSBL_FLAG ) {
			/*ignore it*/
		} else {
			/* add gateway to usage list */
			if ( push_gw_for_usage(msg, &uri, cdst->dst.gw,
			&cr->id, &cr->attrs, n ) ) {
				LM_ERR("failed to use gw <%.*s>, skipping\n",
					cdst->dst.gw->id.len, cdst->dst.gw->id.s);
			} else {
				n++;
				if (cr->flags&DR_CR_FLAG_FIRST)
					/* use only first valid GW */
					break;
			}
		}

	}

	if( n < 1) {
		LM_ERR("All the gateways are disabled\n");
		goto error;
	}
no_gws:

	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );

	return 1;
error:
	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );
	return -1;
}


static int route2_gw(struct sip_msg* msg, char *gw_str)
{
	struct sip_uri  uri;
	pv_value_t val;
	pgw_t *gw;
	str *ruri;

	/* get the gw ID */
	if ( pv_get_spec_value(msg, (pv_spec_p)gw_str, &val)!=0 ||
	(val.flags&PV_VAL_STR)==0 ) {
		LM_ERR("failed to get string value for gw ID\n");
		return -1;
	}

	/* get the RURI */
	ruri = GET_RURI(msg);
	/* parse ruri */
	if (parse_uri( ruri->s, ruri->len, &uri)!=0) {
		LM_ERR("unable to parse RURI\n");
		return -1;
	}

	/* ref the data for reading */
	lock_start_read( ref_lock );

	gw = get_gw_by_id( (*rdata)->pgw_l, &val.rs );
	if (gw==NULL) {
		LM_ERR("no GW found with ID <%.*s> \n",val.rs.len,val.rs.s);
		goto error;
	}

	if ( push_gw_for_usage(msg, &uri, gw, NULL, NULL, 0 ) ) {
		LM_ERR("failed to use gw <%.*s>, skipping\n",
			gw->id.len, gw->id.s);
		goto error;
	}

	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );

	return 1;
error:
	/* we are done reading -> unref the data */
	lock_stop_read( ref_lock );
	return -1;
}


static int fixup_do_routing(void** param, int param_no)
{
	char *s;
	dr_group_t *drg;
	pv_spec_t avp_spec;
	str r;

	s = (char*)*param;

	if (param_no==1) {
		/* group */
		drg = (dr_group_t*)pkg_malloc(sizeof(dr_group_t));
		if(drg==NULL) {
			LM_ERR( "no more memory\n");
			return E_OUT_OF_MEM;
		}
		memset(drg, 0, sizeof(dr_group_t));

		if ( s==NULL || s[0]==0 ) {
			pkg_free(*param);
			*param = NULL;
			return 0;
		}

		if (s[0]=='$') {
			/* param is a PV (AVP only supported) */
			r.s = s;
			r.len = strlen(s);
			if (pv_parse_spec( &r, &avp_spec)==0
			|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %s AVP definition\n", s);
				return E_CFG;
			}

			if( pv_get_avp_name(0, &(avp_spec.pvp), &(drg->u.avp_id.name),
			&(drg->u.avp_id.type) )!=0) {
				LM_ERR("[%s]- invalid AVP definition\n", s);
				return E_CFG;
			}
			drg->type = 1;
			/* do not free the param as the AVP spec may point inside 
			   this string*/
		} else {
			while(s && *s) {
				if(*s<'0' || *s>'9') {
					LM_ERR( "bad number\n");
					return E_UNSPEC;
				}
				drg->u.grp_id = (drg->u.grp_id)*10+(*s-'0');
				s++;
			}
			pkg_free(*param);
		}
		*param = (void*)drg;
	} else
	if (param_no==2) {
		/* string of flags */
		return fixup_sgp(param);
	} else
	if (param_no==3) {
		/* white_list of GWs/Carriers */
		return fixup_spve(param);
	}

	return 0;
}


static int fixup_from_gw( void** param, int param_no)
{
	if (param_no == 1) {
		/* GW type*/
		return fixup_uint(param);
	} else if (param_no == 2) {
		/* GW ops */
		return fixup_spve(param);
	}
	return 0;
}


static int strip_username(struct sip_msg* msg, int strip)
{
	struct action act;
 
	act.type = STRIP_T;
	act.elem[0].type = NUMBER_ST;
	act.elem[0].u.number = strip;
	act.next = 0;

	if (do_action(&act, msg) < 0) {
		LM_ERR( "Error in do_action\n");
		return -1;
	}
	return 0;
}


static int prefix_username(struct sip_msg* msg, str *pri)
{
	struct action act;
 
	act.type = PREFIX_T;
	act.elem[0].type = STR_ST;
	act.elem[0].u.s = *pri;
	act.next = 0;

	if (do_action(&act, msg) < 0) {
		LM_ERR( "Error in do_action\n");
		return -1;
	}
	return 0;
}


static int gw_matches_ip(pgw_t *pgwa, struct ip_addr *ip, unsigned short port)
{
	unsigned short j;
	for ( j=0 ; j<pgwa->ips_no ; j++)
		if ( (pgwa->ports[j]==0 || pgwa->ports[j]==port) &&
		ip_addr_cmp( &pgwa->ips[j], ip) ) return 1;
	return 0;
}


static int is_from_gw_0(struct sip_msg* msg, char* str, char* str2)
{
	pgw_t *pgwa = NULL;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;
	
	pgwa = (*rdata)->pgw_l;
	while(pgwa) {
		if( gw_matches_ip( pgwa, &msg->rcv.src_ip, msg->rcv.src_port) )
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}


static int is_from_gw_1(struct sip_msg* msg, char* str, char* str2)
{
	pgw_t *pgwa = NULL;
	int type = (int)(long)str;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;
	
	pgwa = (*rdata)->pgw_l;
	while(pgwa) {
		if( type==pgwa->type && 
		gw_matches_ip( pgwa, &msg->rcv.src_ip, msg->rcv.src_port) )
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}


#define DR_IFG_STRIP_FLAG      (1<<0)
#define DR_IFG_PREFIX_FLAG     (1<<1)
#define DR_IFG_ATTRS_FLAG      (1<<2)
static int is_from_gw_2(struct sip_msg* msg, char* type_s, char* flags_pv)
{
	pgw_t *pgwa = NULL;
	int type = (int)(long)type_s;
	int flags = 0;
	str flags_s;
	int_str val;
	int i;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;

	if (flags_pv && flags_pv[0]) {
		if (fixup_get_svalue( msg, (gparam_p)flags_pv, &flags_s)!=0) {
			LM_ERR("invalid flags parameter");
			return -1;
		}
		for( i=0 ; i < flags_s.len ; i++ ) {
			switch (flags_s.s[i]) {
				case 's': flags |= DR_IFG_STRIP_FLAG; break;
				case 'p': flags |= DR_IFG_PREFIX_FLAG; break;
				case 'a': flags |= DR_IFG_ATTRS_FLAG; break;
				default: LM_WARN("unsuported flag %c \n",flags_s.s[i]);
			}
		}
	}

	pgwa = (*rdata)->pgw_l;
	while(pgwa) {
		if( type==pgwa->type &&
		gw_matches_ip( pgwa, &msg->rcv.src_ip, msg->rcv.src_port) ) {
			/* strip ? */
			if ( (flags&DR_IFG_STRIP_FLAG) && pgwa->strip>0)
				strip_username(msg, pgwa->strip);
			/* prefix ? */
			if ( (flags&DR_IFG_PREFIX_FLAG) && pgwa->pri.len>0)
				prefix_username(msg, &pgwa->pri);
			/* attrs ? */
			if (gw_attrs_avp.name!=-1) {
				val.s = pgwa->attrs.s ? pgwa->attrs : attrs_empty ;
				if (add_avp(AVP_VAL_STR|(gw_attrs_avp.type),
				gw_attrs_avp.name,val)!=0)
					LM_ERR("failed to insert GW attrs avp\n");
			}
			return 1;
		}
		pgwa = pgwa->next;
	}
	return -1;
}


static int goes_to_gw_1(struct sip_msg* msg, char* _type, char* flags_pv)
{
	pgw_t *pgwa = NULL;
	struct sip_uri puri;
	struct ip_addr *ip;
	str *uri;
	int type;
	int flags = 0;
	str flags_s;
	int_str val;
	int i;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;

	uri = GET_NEXT_HOP(msg);
	type = (int)(long)_type;

	if (parse_uri(uri->s, uri->len, &puri)<0){
		LM_ERR("bad uri <%.*s>\n", uri->len, uri->s);
		return -1;
	}

	if ( ((ip=str2ip(&puri.host))!=0)
#ifdef USE_IPV6
	|| ((ip=str2ip6(&puri.host))!=0)
#endif
	){
		/* prepare flags */
		if (flags_pv && flags_pv[0]) {
			if (fixup_get_svalue( msg, (gparam_p)flags_pv, &flags_s)!=0) {
				LM_ERR("invalid flags parameter");
				return -1;
			}
			for( i=0 ; i < flags_s.len ; i++ ) {
				switch (flags_s.s[i]) {
					case 's': flags |= DR_IFG_STRIP_FLAG; break;
					case 'p': flags |= DR_IFG_PREFIX_FLAG; break;
					case 'a': flags |= DR_IFG_ATTRS_FLAG; break;
					default: LM_WARN("unsuported flag %c \n",flags_s.s[i]);
				}
			}
		}

		pgwa = (*rdata)->pgw_l;
		while(pgwa) {
			if( (type<0 || type==pgwa->type) && gw_matches_ip( pgwa, ip, 0) ) {

				/* strip ? */
				if ( (flags&DR_IFG_STRIP_FLAG) && pgwa->strip>0)
					strip_username(msg, pgwa->strip);
				/* prefix ? */
				if ( (flags&DR_IFG_PREFIX_FLAG) && pgwa->pri.len>0)
					prefix_username(msg, &pgwa->pri);
				/* attrs ? */
				if (gw_attrs_avp.name!=-1) {
					val.s = pgwa->attrs.s ? pgwa->attrs : attrs_empty ;
					if (add_avp( AVP_VAL_STR|(gw_attrs_avp.type),
					gw_attrs_avp.name, val))
						LM_ERR("failed to insert attrs avp\n");
				}
				return 1;
			}
			pgwa = pgwa->next;
		}
	}

	return -1;
}


static int goes_to_gw_0(struct sip_msg* msg, char* _type, char* _f2)
{
	return goes_to_gw_1(msg, (char*)(long)-1, _f2);
}

static struct mi_root* mi_dr_gw_status(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node;
	struct mi_attr *attr;
	unsigned int stat;
	pgw_t *gw;
	str *id;

	node = cmd->node.kids;

	lock_start_read( ref_lock );

	if (node==NULL) {
		/* no GW specified, list all of them */
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			goto error;

		for( gw=(*rdata)->pgw_l ; gw ; gw=gw->next ) {
			node = add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE,
				"ID", 2, gw->id.s, gw->id.len);
			if (node==NULL) goto error;
			attr = add_mi_attr( node, MI_DUP_VALUE, "IP" , 2,
				gw->ip_str.s, gw->ip_str.len);
			if (attr==NULL) goto error;
			attr = add_mi_attr( node, 0, "Enabled", 7,
				(gw->flags&DR_DST_STAT_DSBL_FLAG)?"no ":"yes", 3);
			if (attr==NULL) goto error;
		}

		goto done;
	}

	/* GW ID (param 1) */
	id =  &node->value;

	/* search for the Gw */
	gw = get_gw_by_id( (*rdata)->pgw_l, id);
	if (gw==NULL) {
		rpl_tree = init_mi_tree( 404, MI_SSTR("GW ID not found"));
		goto done;
	}

	/* status (param 2) */
	node = node->next;
	if (node == NULL) {

		/* no status provided -> return the internal one */
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			goto error;
		node = add_mi_node_child( &rpl_tree->node, 0, "Enabled", 7,
			(gw->flags&DR_DST_STAT_DSBL_FLAG)?"no ":"yes", 3);
		if (node==NULL)
			goto error;

		goto done;

	}

	/* set the status */
	if (node->next) {
		rpl_tree = init_mi_tree( 400,
			MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
		goto done;
	}
	if (str2int( &node->value, &stat) < 0) {
		rpl_tree = init_mi_tree( 400, MI_SSTR(MI_BAD_PARM_S));
		goto done;
	}
	/* set the disable/enable */
	if (stat) {
		gw->flags &= ~ (DR_DST_STAT_DSBL_FLAG|DR_DST_STAT_NOEN_FLAG);
	} else {
		gw->flags |= DR_DST_STAT_DSBL_FLAG|DR_DST_STAT_NOEN_FLAG;
	}
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

done:
	lock_stop_read( ref_lock );
	return rpl_tree;
error:
	lock_stop_read( ref_lock );
	if(rpl_tree) free_mi_tree(rpl_tree);
	return NULL;
}


static struct mi_root* mi_dr_cr_status(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node;
	struct mi_attr *attr;
	unsigned int stat;
	pcr_t *cr;
	str *id;

	node = cmd->node.kids;

	lock_start_read( ref_lock );

	if (node==NULL) {
		/* no carrier specified, list all of them */
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			goto error;

		for( cr=(*rdata)->carriers ; cr ; cr=cr->next ) {
			node = add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE,
				"ID", 2, cr->id.s, cr->id.len);
			if (node==NULL) goto error;
			attr = add_mi_attr( node, 0, "Enabled", 7,
				(cr->flags&DR_CR_FLAG_IS_OFF)?"no ":"yes", 3);
			if (attr==NULL) goto error;
		}

		goto done;
	}

	/* GW ID (param 1) */
	id =  &node->value;

	/* search for the Carrier */
	cr = get_carrier_by_id( (*rdata)->carriers, id);
	if (cr==NULL) {
		rpl_tree = init_mi_tree( 404, MI_SSTR("Carrier ID not found"));
		goto done;
	}

	/* status (param 2) */
	node = node->next;
	if (node == NULL) {

		/* no status provided -> return the internal one */
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			goto error;
		node = add_mi_node_child( &rpl_tree->node, 0, "Enabled", 7,
			(cr->flags&DR_CR_FLAG_IS_OFF)?"no ":"yes", 3);
		if (node==NULL)
			goto error;

		goto done;

	}

	/* set the status */
	if (node->next) {
		rpl_tree = init_mi_tree( 400,
			MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
		goto done;
	}
	if (str2int( &node->value, &stat) < 0) {
		rpl_tree = init_mi_tree( 400, MI_SSTR(MI_BAD_PARM_S));
		goto done;
	}
	/* set the disable/enable */
	if (stat) {
		cr->flags &= ~ (DR_CR_FLAG_IS_OFF);
	} else {
		cr->flags |= DR_CR_FLAG_IS_OFF;
	}
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

done:
	lock_stop_read( ref_lock );
	return rpl_tree;
error:
	lock_stop_read( ref_lock );
	if(rpl_tree) free_mi_tree(rpl_tree);
	return NULL;
}

