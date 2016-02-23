/*
 * $Id: openserObjects.c 5901 2009-07-21 07:45:05Z bogdan_iancu $
 *
 * SNMPStats Module 
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 2007-02-16 Moved all OID registrations from the experimental branch to 
 *            OpenSER's IANA assigned enterprise branch. (jmagder)
 * 
 * Note: this file originally auto-generated by mib2c using
 *		: mib2c.scalar.conf,v 1.9 2005/01/07 09:37:18 dts12 Exp $
 *
 * This file defines all registration and handling of all scalars defined in the
 * OPENSER-MIB.  Please see OPENSER-MIB for the complete descriptions of the
 * individual scalars.
 */

#include <string.h>

#include "openserObjects.h"

#include "../../dprint.h"
#include "../../statistics.h"
#include "../../config.h"
#include "../../socket_info.h"

#include "snmpstats_globals.h"
#include "utilities.h"
#include "alarm_checks.h"

/* 
 * Initializes the openserObjects module.  This involves:
 *
 *  - Registering all OID's
 *  - Setting up handlers for all OID's
 *
 * This function is mostly auto-generated.
 */
void init_openserObjects(void)
{
	static oid openserMsgQueueDepth_oid[]            =  
		{ OPENSER_OID,3,1,3,1,2,3,1 };

	static oid openserMsgQueueMinorThreshold_oid[]   = 
		{ OPENSER_OID,3,1,3,1,2,3,2 };

	static oid openserMsgQueueMajorThreshold_oid[]   = 
		{ OPENSER_OID,3,1,3,1,2,3,3 };

	static oid openserMsgQueueDepthAlarmStatus_oid[] = 
		{ OPENSER_OID,3,1,3,1,2,4,1 };
  
  	static oid openserMsgQueueDepthMinorAlarm_oid[]  = 
		{ OPENSER_OID,3,1,3,1,2,4,2 };

	static oid openserMsgQueueDepthMajorAlarm_oid[]  = 
		{ OPENSER_OID,3,1,3,1,2,4,3 };

	static oid openserCurNumDialogs_oid[]            = 
		{ OPENSER_OID,3,1,3,1,3,2,1 };

	static oid openserCurNumDialogsInProgress_oid[]  = 
		{ OPENSER_OID,3,1,3,1,3,2,2 };

	static oid openserCurNumDialogsInSetup_oid[]     = 
		{ OPENSER_OID,3,1,3,1,3,2,3 };

	static oid openserTotalNumFailedDialogSetups_oid[] = 
		{ OPENSER_OID,3,1,3,1,3,2,4 };

	static oid openserDialogLimitMinorThreshold_oid[]  = 
		{ OPENSER_OID,3,1,3,1,3,2,5 };

	static oid openserDialogLimitMajorThreshold_oid[]  = 
		{ OPENSER_OID,3,1,3,1,3,2,6 };

	static oid openserDialogUsageState_oid[]       = 
		{ OPENSER_OID,3,1,3,1,3,3,1 };

	static oid openserDialogLimitAlarmStatus_oid[] = 
		{ OPENSER_OID,3,1,3,1,3,4,1 };

	static oid openserDialogLimitMinorAlarm_oid[]  = 
		{ OPENSER_OID,3,1,3,1,3,4,2 };

	static oid openserDialogLimitMajorAlarm_oid[]  = 
		{ OPENSER_OID,3,1,3,1,3,4,3 };


  DEBUGMSGTL(("openserObjects", "Initializing\n"));

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueDepth", 
			handle_openserMsgQueueDepth,
			openserMsgQueueDepth_oid, 
			OID_LENGTH(openserMsgQueueDepth_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueMinorThreshold", 
			handle_openserMsgQueueMinorThreshold,
			openserMsgQueueMinorThreshold_oid, 
			OID_LENGTH(openserMsgQueueMinorThreshold_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueMajorThreshold", 
			handle_openserMsgQueueMajorThreshold,
			openserMsgQueueMajorThreshold_oid, 
			OID_LENGTH(openserMsgQueueMajorThreshold_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueDepthAlarmStatus", 
			handle_openserMsgQueueDepthAlarmStatus,
			openserMsgQueueDepthAlarmStatus_oid, 
			OID_LENGTH(openserMsgQueueDepthAlarmStatus_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueDepthMinorAlarm", 
			handle_openserMsgQueueDepthMinorAlarm,
			openserMsgQueueDepthMinorAlarm_oid, 
			OID_LENGTH(openserMsgQueueDepthMinorAlarm_oid),
			HANDLER_CAN_RONLY)
		);
  
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserMsgQueueDepthMajorAlarm", 
			handle_openserMsgQueueDepthMajorAlarm,
			openserMsgQueueDepthMajorAlarm_oid, 
			OID_LENGTH(openserMsgQueueDepthMajorAlarm_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserCurNumDialogs", 
			handle_openserCurNumDialogs,
			openserCurNumDialogs_oid, 
			OID_LENGTH(openserCurNumDialogs_oid),
			HANDLER_CAN_RONLY)
		);
	
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserCurNumDialogsInProgress", 
			handle_openserCurNumDialogsInProgress,
			openserCurNumDialogsInProgress_oid, 
			OID_LENGTH(openserCurNumDialogsInProgress_oid),
			HANDLER_CAN_RONLY)
		);
	
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserCurNumDialogsInSetup", 
			handle_openserCurNumDialogsInSetup,
			openserCurNumDialogsInSetup_oid, 
			OID_LENGTH(openserCurNumDialogsInSetup_oid),
			HANDLER_CAN_RONLY)
		);
  
  	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserTotalNumFailedDialogSetups", 
		handle_openserTotalNumFailedDialogSetups,
		openserTotalNumFailedDialogSetups_oid, 
		OID_LENGTH(openserTotalNumFailedDialogSetups_oid),
		HANDLER_CAN_RONLY)
		);
	
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogLimitMinorThreshold", 
			handle_openserDialogLimitMinorThreshold,
			openserDialogLimitMinorThreshold_oid, 
			OID_LENGTH(openserDialogLimitMinorThreshold_oid),
			 HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogLimitMajorThreshold", 
			handle_openserDialogLimitMajorThreshold,
			openserDialogLimitMajorThreshold_oid, 
			OID_LENGTH(openserDialogLimitMajorThreshold_oid),
			HANDLER_CAN_RONLY)
		);

	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogUsageState", 
			handle_openserDialogUsageState,
			openserDialogUsageState_oid, 
			OID_LENGTH(openserDialogUsageState_oid),
			HANDLER_CAN_RONLY)
		);
  
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogLimitAlarmStatus", 
			handle_openserDialogLimitAlarmStatus,
			openserDialogLimitAlarmStatus_oid, 
			OID_LENGTH(openserDialogLimitAlarmStatus_oid),
			HANDLER_CAN_RONLY)
		);
   
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogLimitMinorAlarm", 
			handle_openserDialogLimitMinorAlarm,
			openserDialogLimitMinorAlarm_oid, 
			OID_LENGTH(openserDialogLimitMinorAlarm_oid),
			HANDLER_CAN_RONLY)
		);
 
	netsnmp_register_scalar(
		netsnmp_create_handler_registration(
			"openserDialogLimitMajorAlarm", 
			handle_openserDialogLimitMajorAlarm,
			openserDialogLimitMajorAlarm_oid, 
			OID_LENGTH(openserDialogLimitMajorAlarm_oid),
			HANDLER_CAN_RONLY)
		);
}


/* 
 * The following are thresholds used by:
 * 
 * - The alarm monitoring process, to decide when to send out traps.  
 * - All scalars involving alarm status's and thresholds.  
 *
 * By default they are initialized to -1, which disables alarm checks.
 * These are set through the opensips.cfg file with the following modparams to
 * the snmpstats module:
 *
 *  - dlg_minor_threshold  
 *  - dlg_major_threshold  
 *
 *  - MsgQueueMinorThreshold
 *  - MsgQueueMajorThreshold
 *
 */
static int dialog_minor_threshold = -1;
static int dialog_major_threshold = -1;

static int msgQueueMinorThreshold = -1;
static int msgQueueMajorThreshold = -1;


int handle_openserMsgQueueDepth(netsnmp_mib_handler *handler,
  		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int bytesWaiting;
	
	bytesWaiting = get_total_bytes_waiting(PROTO_NONE);

	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &bytesWaiting, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserMsgQueueMinorThreshold(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &msgQueueMinorThreshold, sizeof(int));
		return SNMP_ERR_NOERROR;
	} 

	return SNMP_ERR_GENERR;
}

int handle_openserMsgQueueMajorThreshold(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &msgQueueMajorThreshold, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserMsgQueueDepthAlarmStatus(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{

	/* The MIB specifications say the scalar should be set to 'clear' if
	 * everything is ok.  According the X731AlarmStatus specification,
	 * this means that no bits are toggled.  So we set the state to zero by
	 * default */
	unsigned int state = 0;
	
	if (check_msg_queue_alarm(msgQueueMinorThreshold)) {
		state |=  TC_ALARM_STATUS_MINOR;
	}

	if (check_msg_queue_alarm(msgQueueMajorThreshold)) {
		state |=  TC_ALARM_STATUS_MAJOR;
	}

	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
			(u_char *)&state, 1);
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserMsgQueueDepthMinorAlarm(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int x731AlarmState = TC_ALARM_STATE_CLEAR;

	if (check_msg_queue_alarm(msgQueueMinorThreshold)) { 	
		x731AlarmState = TC_ALARM_STATE_MINOR;
	}

	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &x731AlarmState, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserMsgQueueDepthMajorAlarm(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int x731AlarmState = TC_ALARM_STATE_CLEAR;

	if (check_msg_queue_alarm(msgQueueMajorThreshold)) { 	
		x731AlarmState = TC_ALARM_STATE_MAJOR;
	}
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &x731AlarmState, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserCurNumDialogs(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* This scalar is defined as the number of dialogs in both the EARLY and
	 * the CONFIRMED state.  */
	int result = get_statistic("active_dialogs");

	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserCurNumDialogsInProgress(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* This scalar is defined as the number of dialogs in the CONFIRMED
	 * state only.  active_dialogs includes both confirmed and early, so
	 * we subtract out early_dialogs from active_dialogs. */
	int result = 
		get_statistic("active_dialogs") -
		get_statistic("early_dialogs");
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserCurNumDialogsInSetup(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* This scalar is defined as the number of dialogs in the EARLY state.
	 * */
	int result = get_statistic("early_dialogs");
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserTotalNumFailedDialogSetups(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int result = get_statistic("failed_dialogs");
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
			(u_char *) &result, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserDialogLimitMinorThreshold(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &dialog_minor_threshold, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserDialogLimitMajorThreshold(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &dialog_major_threshold, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserDialogUsageState(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* Return value follows the X731UsageState Textual Convention 
	 *
	 * We default to 'unknown' */
	int usage_state = TC_USAGE_STATE_UNKNOWN;

	int num_dialogs = get_statistic("active_dialogs");

	if (num_dialogs==0) {
		usage_state = TC_USAGE_STATE_IDLE;
	} else {
		usage_state = TC_USAGE_STATE_ACTIVE;
	}

	if ((dialog_major_threshold > -1) && (num_dialogs > 
		    dialog_major_threshold)) {
		usage_state = TC_USAGE_STATE_BUSY;
	}
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &usage_state, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}


int handle_openserDialogLimitAlarmStatus(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	/* The MIB specifications say the scalar should be set to 'clear' if
	 * everything is ok.  According the X731AlarmStatus specification,
	 * this means that no bits are toggled.  So we set the state to zero by
	 * default */
	unsigned int state = 0;
	
	if (check_dialog_alarm(dialog_minor_threshold)) {
		state |=  TC_ALARM_STATUS_MINOR;
	}

	if (check_dialog_alarm(dialog_major_threshold)) {
		state |=  TC_ALARM_STATUS_MAJOR;
	}

	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
			(u_char *) &state, 1);
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}



int handle_openserDialogLimitMinorAlarm(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int x731AlarmState = TC_ALARM_STATE_CLEAR;

	if (check_dialog_alarm(dialog_minor_threshold))
	{
		x731AlarmState = TC_ALARM_STATE_MINOR;
	}
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &x731AlarmState, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

int handle_openserDialogLimitMajorAlarm(netsnmp_mib_handler *handler,
		netsnmp_handler_registration *reginfo,
		netsnmp_agent_request_info   *reqinfo,
		netsnmp_request_info         *requests)
{
	int x731AlarmState = TC_ALARM_STATE_CLEAR;

	if (check_dialog_alarm(dialog_major_threshold))
	{
		x731AlarmState = TC_ALARM_STATE_MAJOR;
	}
	
	if (reqinfo->mode == MODE_GET) {
		snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
			(u_char *) &x731AlarmState, sizeof(int));
		return SNMP_ERR_NOERROR;
	}

	return SNMP_ERR_GENERR;
}

/* If a proper integer is passed that is >= -1, then newValue will be set to
 * val, and 0 returned.  Otherwise -1 is returned. */
static int set_if_valid_threshold(modparam_t type, void *val, char *varStr, 
	int *newVal) 
{
	if (val==0) {
		LM_ERR("%s called with a null value!\n", varStr);
		return -1;
	}

	if (type != INT_PARAM) {
		LM_ERR("%s called with type %d instead of %d!\n",
				varStr, type, INT_PARAM);
		return -1;
	}
	
	int new_threshold = (int)(long)(int *)val;

	if (new_threshold < -1) {
		LM_ERR("%s called with an invalid threshold=%d!\n",
				varStr, new_threshold); 
		return -1;
	}
	
	*newVal = new_threshold;

	return 0;
}

/* 
 * Paramater Configuration Functions 
 */

/* Handles setting of the message queue minor alarm threshold */
int set_queue_minor_threshold(modparam_t type, void *val) {
	return set_if_valid_threshold(type, val, "MsgQueueMinorThreshold",
			&msgQueueMinorThreshold);
}


/* Handles setting of the message queue major alarm threshold */
int set_queue_major_threshold(modparam_t type, void *val) {
	return set_if_valid_threshold(type, val, "MsgQueueMajorThreshold",
			&msgQueueMajorThreshold);
}


/* Handles setting of the dialog minor threshold */
int set_dlg_minor_threshold(modparam_t type, void *val) {
	return set_if_valid_threshold(type, val, "set_dlg_minor_threshold", 
		&dialog_minor_threshold);
}


/* Handles setting of the dialog major threshold */
int set_dlg_major_threshold(modparam_t type, void *val) {
	return set_if_valid_threshold(type, val, "set_dlg_major_threshold", 
		&dialog_major_threshold);
}

int get_msg_queue_minor_threshold(void) 
{
	return msgQueueMinorThreshold;
}

int get_msg_queue_major_threshold(void) 
{
	return msgQueueMajorThreshold;
}

int get_dialog_minor_threshold(void)
{
	return dialog_minor_threshold;
}

int get_dialog_major_threshold(void)
{
	return dialog_major_threshold;
}
