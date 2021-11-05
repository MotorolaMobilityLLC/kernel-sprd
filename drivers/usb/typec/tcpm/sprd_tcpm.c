/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Rong.wu <Rong.wu@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/proc_fs.h>
#include <linux/property.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/usb/sprd_pd.h>
#include <linux/usb/pd_ado.h>
#include <linux/usb/pd_bdo.h>
#include <linux/usb/pd_ext_sdb.h>
#include <linux/usb/pd_vdo.h>
#include <linux/usb/role.h>
#include <linux/usb/sprd_tcpm.h>
#include <linux/usb/typec_altmode.h>
#include <linux/workqueue.h>

enum sprd_tcpm_state {
	INVALID_STATE = 0,
	TOGGLING,
	SRC_UNATTACHED,
	SRC_ATTACH_WAIT,
	SRC_ATTACHED,
	SRC_STARTUP = 5,
	SRC_SEND_CAPABILITIES,
	SRC_SEND_CAPABILITIES_TIMEOUT,
	SRC_NEGOTIATE_CAPABILITIES,
	SRC_TRANSITION_SUPPLY,
	SRC_READY = 10,
	SRC_WAIT_NEW_CAPABILITIES,

	SNK_UNATTACHED,
	SNK_ATTACH_WAIT,
	SNK_DEBOUNCED,
	SNK_ATTACHED = 15,
	SNK_STARTUP,
	SNK_DISCOVERY,
	SNK_DISCOVERY_DEBOUNCE,
	SNK_DISCOVERY_DEBOUNCE_DONE,
	SNK_WAIT_CAPABILITIES = 20,
	SNK_NEGOTIATE_CAPABILITIES,
	SNK_NEGOTIATE_PPS_CAPABILITIES,
	SNK_TRANSITION_SINK,
	SNK_TRANSITION_SINK_VBUS,
	SNK_READY = 25,

	ACC_UNATTACHED,
	DEBUG_ACC_ATTACHED,
	AUDIO_ACC_ATTACHED,
	AUDIO_ACC_DEBOUNCE,

	HARD_RESET_SEND = 30,
	HARD_RESET_START,
	SRC_HARD_RESET_VBUS_OFF,
	SRC_HARD_RESET_VBUS_ON,
	SNK_HARD_RESET_SINK_OFF,
	SNK_HARD_RESET_WAIT_VBUS = 35,
	SNK_HARD_RESET_SINK_ON,

	SOFT_RESET,
	SOFT_RESET_SEND,

	DR_SWAP_ACCEPT,
	DR_SWAP_SEND = 40,
	DR_SWAP_SEND_TIMEOUT,
	DR_SWAP_CANCEL,
	DR_SWAP_CHANGE_DR,

	PR_SWAP_ACCEPT,
	PR_SWAP_SEND = 45,
	PR_SWAP_SEND_TIMEOUT,
	PR_SWAP_CANCEL,
	PR_SWAP_START,
	PR_SWAP_SRC_SNK_TRANSITION_OFF,
	PR_SWAP_SRC_SNK_SOURCE_OFF = 50,
	PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED,
	PR_SWAP_SRC_SNK_SINK_ON,
	PR_SWAP_SNK_SRC_SINK_OFF,
	PR_SWAP_SNK_SRC_SOURCE_ON,
	PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP = 55,

	VCONN_SWAP_ACCEPT,
	VCONN_SWAP_SEND,
	VCONN_SWAP_SEND_TIMEOUT,
	VCONN_SWAP_CANCEL,
	VCONN_SWAP_START = 60,
	VCONN_SWAP_WAIT_FOR_VCONN,
	VCONN_SWAP_TURN_ON_VCONN,
	VCONN_SWAP_TURN_OFF_VCONN,

	SNK_TRY,
	SNK_TRY_WAIT = 65,
	SNK_TRY_WAIT_DEBOUNCE,
	SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS,
	SRC_TRYWAIT,
	SRC_TRYWAIT_DEBOUNCE,
	SRC_TRYWAIT_UNATTACHED = 70,

	SRC_TRY,
	SRC_TRY_WAIT,
	SRC_TRY_DEBOUNCE,
	SNK_TRYWAIT,
	SNK_TRYWAIT_DEBOUNCE = 75,
	SNK_TRYWAIT_VBUS,
	BIST_RX,

	GET_STATUS_SEND,
	GET_STATUS_SEND_TIMEOUT,
	GET_PPS_STATUS_SEND = 80,
	GET_PPS_STATUS_SEND_TIMEOUT,

	ERROR_RECOVERY,
	PORT_RESET,
	PORT_RESET_WAIT_OFF,
};

static const char * const sprd_tcpm_states[] = {
	"INVALID_STATE",				/* = 0 */
	"TOGGLING",
	"SRC_UNATTACHED",
	"SRC_ATTACH_WAIT",
	"SRC_ATTACHED",
	"SRC_STARTUP",					/* = 5 */
	"SRC_SEND_CAPABILITIES",
	"SRC_SEND_CAPABILITIES_TIMEOUT",
	"SRC_NEGOTIATE_CAPABILITIES",
	"SRC_TRANSITION_SUPPLY",
	"SRC_READY",					/* = 10 */
	"SRC_WAIT_NEW_CAPABILITIES",

	"SNK_UNATTACHED",
	"SNK_ATTACH_WAIT",
	"SNK_DEBOUNCED",
	"SNK_ATTACHED",					/* = 15 */
	"SNK_STARTUP",
	"SNK_DISCOVERY",
	"SNK_DISCOVERY_DEBOUNCE",
	"SNK_DISCOVERY_DEBOUNCE_DONE",
	"SNK_WAIT_CAPABILITIES",			/* = 20 */
	"SNK_NEGOTIATE_CAPABILITIES",
	"SNK_NEGOTIATE_PPS_CAPABILITIES",
	"SNK_TRANSITION_SINK",
	"SNK_TRANSITION_SINK_VBUS",
	"SNK_READY",					/* = 25 */

	"ACC_UNATTACHED",
	"DEBUG_ACC_ATTACHED",
	"AUDIO_ACC_ATTACHED",
	"AUDIO_ACC_DEBOUNCE",

	"HARD_RESET_SEND",				/* = 30 */
	"HARD_RESET_START",
	"SRC_HARD_RESET_VBUS_OFF",
	"SRC_HARD_RESET_VBUS_ON",
	"SNK_HARD_RESET_SINK_OFF",
	"SNK_HARD_RESET_WAIT_VBUS",			/* = 35 */
	"SNK_HARD_RESET_SINK_ON",

	"SOFT_RESET",
	"SOFT_RESET_SEND",

	"DR_SWAP_ACCEPT",
	"DR_SWAP_SEND",					/* = 40 */
	"DR_SWAP_SEND_TIMEOUT",
	"DR_SWAP_CANCEL",
	"DR_SWAP_CHANGE_DR",

	"PR_SWAP_ACCEPT",
	"PR_SWAP_SEND",					/* = 45 */
	"PR_SWAP_SEND_TIMEOUT",
	"PR_SWAP_CANCEL",
	"PR_SWAP_START",
	"PR_SWAP_SRC_SNK_TRANSITION_OFF",
	"PR_SWAP_SRC_SNK_SOURCE_OFF",			/* = 50 */
	"PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED",
	"PR_SWAP_SRC_SNK_SINK_ON",
	"PR_SWAP_SNK_SRC_SINK_OFF",
	"PR_SWAP_SNK_SRC_SOURCE_ON",
	"PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP",	/* = 55 */

	"VCONN_SWAP_ACCEPT",
	"VCONN_SWAP_SEND",
	"VCONN_SWAP_SEND_TIMEOUT",
	"VCONN_SWAP_CANCEL",
	"VCONN_SWAP_START",				/* = 60 */
	"VCONN_SWAP_WAIT_FOR_VCONN",
	"VCONN_SWAP_TURN_ON_VCONN",
	"VCONN_SWAP_TURN_OFF_VCONN",

	"SNK_TRY",
	"SNK_TRY_WAIT",					/* = 65 */
	"SNK_TRY_WAIT_DEBOUNCE",
	"SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS",
	"SRC_TRYWAIT",
	"SRC_TRYWAIT_DEBOUNCE",
	"SRC_TRYWAIT_UNATTACHED",			/* = 70 */

	"SRC_TRY",
	"SRC_TRY_WAIT",
	"SRC_TRY_DEBOUNCE",
	"SNK_TRYWAIT",
	"SNK_TRYWAIT_DEBOUNCE",				/* = 75 */
	"SNK_TRYWAIT_VBUS",
	"BIST_RX",

	"GET_STATUS_SEND",
	"GET_STATUS_SEND_TIMEOUT",
	"GET_PPS_STATUS_SEND",				/* = 80 */
	"GET_PPS_STATUS_SEND_TIMEOUT",

	"ERROR_RECOVERY",
	"PORT_RESET",
	"PORT_RESET_WAIT_OFF",
};

enum sprd_vdm_states {
	VDM_STATE_ERR_BUSY = -3,
	VDM_STATE_ERR_SEND = -2,
	VDM_STATE_ERR_TMOUT = -1,
	VDM_STATE_DONE = 0,
	/* Anything >0 represents an active state */
	VDM_STATE_READY = 1,
	VDM_STATE_BUSY = 2,
	VDM_STATE_WAIT_RSP_BUSY = 3,
};

enum sprd_pd_msg_request {
	PD_MSG_NONE = 0,
	PD_MSG_CTRL_REJECT,
	PD_MSG_CTRL_WAIT,
	PD_MSG_CTRL_NOT_SUPP,
	PD_MSG_DATA_SINK_CAP,
	PD_MSG_DATA_SOURCE_CAP,
};

/* Events from low level driver */

#define SPRD_TCPM_CC_EVENT		BIT(0)
#define SPRD_TCPM_VBUS_EVENT		BIT(1)
#define SPRD_TCPM_RESET_EVENT		BIT(2)

#define SPRD_LOG_BUFFER_ENTRIES		1024
#define SPRD_LOG_BUFFER_ENTRY_SIZE	128

/* Alternate mode support */

#define SPRD_SVID_DISCOVERY_MAX		16
#define SPRD_ALTMODE_DISCOVERY_MAX	(SPRD_SVID_DISCOVERY_MAX * MODE_DISCOVERY_MAX)

struct sprd_pd_mode_data {
	int svid_index;		/* current SVID index		*/
	int nsvids;
	u16 svids[SPRD_SVID_DISCOVERY_MAX];
	int altmodes;		/* number of alternate modes	*/
	struct typec_altmode_desc altmode_desc[SPRD_ALTMODE_DISCOVERY_MAX];
};

/*
 * @min_volt: Actual min voltage at the local port
 * @req_min_volt: Requested min voltage to the port partner
 * @max_volt: Actual max voltage at the local port
 * @req_max_volt: Requested max voltage to the port partner
 * @max_curr: Actual max current at the local port
 * @req_max_curr: Requested max current of the port partner
 * @req_out_volt: Requested output voltage to the port partner
 * @req_op_curr: Requested operating current to the port partner
 * @supported: Parter has atleast one APDO hence supports PPS
 * @active: PPS mode is active
 */
struct sprd_pd_pps_data {
	u32 min_volt;
	u32 req_min_volt;
	u32 max_volt;
	u32 req_max_volt;
	u32 max_curr;
	u32 req_max_curr;
	u32 req_out_volt;
	u32 req_op_curr;
	bool supported;
	bool active;
};

struct sprd_tcpm_port {
	struct device *dev;

	struct mutex lock;		/* tcpm state machine lock */
	struct workqueue_struct *wq;

	struct typec_capability typec_caps;
	struct typec_port *typec_port;

	struct tcpc_dev	*tcpc;
	struct usb_role_switch *role_sw;

	enum typec_role vconn_role;
	enum typec_role pwr_role;
	enum typec_data_role data_role;
	enum typec_pwr_opmode pwr_opmode;

	struct usb_pd_identity partner_ident;
	struct typec_partner_desc partner_desc;
	struct typec_partner *partner;

	enum sprd_typec_cc_status cc_req;

	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;
	enum sprd_typec_cc_polarity polarity;

	bool attached;
	bool connected;
	enum typec_port_type port_type;
	bool vbus_present;
	bool vbus_never_low;
	bool vbus_source;
	bool vbus_charge;

	bool send_discover;
	bool op_vsafe5v;

	int try_role;
	int try_snk_count;
	int try_src_count;

	enum sprd_pd_msg_request queued_message;

	enum sprd_tcpm_state enter_state;
	enum sprd_tcpm_state prev_state;
	enum sprd_tcpm_state state;
	enum sprd_tcpm_state delayed_state;
	unsigned long delayed_runtime;
	unsigned long delay_ms;

	spinlock_t pd_event_lock;
	u32 pd_events;

	struct work_struct event_work;
	struct delayed_work state_machine;
	struct delayed_work vdm_state_machine;
	bool state_machine_running;

	struct completion tx_complete;
	enum sprd_tcpm_transmit_status tx_status;

	struct mutex swap_lock;		/* swap command lock */
	bool swap_pending;
	bool non_pd_role_swap;
	struct completion swap_complete;
	int swap_status;

	unsigned int negotiated_rev;
	unsigned int message_id;
	unsigned int caps_count;
	unsigned int hard_reset_count;
	bool pd_capable;
	bool explicit_contract;
	unsigned int rx_msgid;

	/* Partner capabilities/requests */
	u32 sink_request;
	u32 source_caps[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_source_caps;
	u32 sink_caps[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_sink_caps;

	/* Local capabilities */
	u32 src_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_src_pdo;
	u32 snk_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_snk_pdo;
	u32 snk_default_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_snk_default_pdo;
	u32 snk_vdo[VDO_MAX_OBJECTS];
	unsigned int nr_snk_vdo;

	unsigned int operating_snk_mw;
	unsigned int operating_snk_default_mw;
	bool update_sink_caps;

	/* Requested current / voltage to the port partner */
	u32 req_current_limit;
	u32 req_supply_voltage;
	/* Actual current / voltage limit of the local port */
	u32 current_limit;
	u32 supply_voltage;

	/* Used to export TA voltage and current */
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	enum power_supply_usb_type usb_type;
	enum power_supply_usb_type last_usb_type;

	u32 bist_request;

	/* PD state for Vendor Defined Messages */
	enum sprd_vdm_states vdm_state;
	u32 vdm_retries;
	/* next Vendor Defined Message to send */
	u32 vdo_data[VDO_MAX_SIZE];
	u8 vdo_count;
	/* VDO to retry if UFP responder replied busy */
	u32 vdo_retry;

	/* PPS */
	struct sprd_pd_pps_data pps_data;
	struct completion pps_complete;
	bool pps_pending;
	int pps_status;

	/* Alternate mode data */
	struct sprd_pd_mode_data mode_data;
	struct typec_altmode *partner_altmode[SPRD_ALTMODE_DISCOVERY_MAX];
	struct typec_altmode *port_altmode[SPRD_ALTMODE_DISCOVERY_MAX];

	/* Deadline in jiffies to exit src_try_wait state */
	unsigned long max_wait;

	/* port belongs to a self powered device */
	bool self_powered;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	struct mutex logbuffer_lock;	/* log buffer access lock */
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[SPRD_LOG_BUFFER_ENTRIES];
#endif
};

struct sprd_pd_rx_event {
	struct work_struct work;
	struct sprd_tcpm_port *port;
	struct sprd_pd_message msg;
};

#define sprd_tcpm_cc_is_sink(cc) \
	((cc) == SPRD_TYPEC_CC_RP_DEF || (cc) == SPRD_TYPEC_CC_RP_1_5 || \
	 (cc) == SPRD_TYPEC_CC_RP_3_0)

#define sprd_tcpm_port_is_sink(port) \
	((sprd_tcpm_cc_is_sink((port)->cc1) && !sprd_tcpm_cc_is_sink((port)->cc2)) || \
	 (sprd_tcpm_cc_is_sink((port)->cc2) && !sprd_tcpm_cc_is_sink((port)->cc1)))

#define sprd_tcpm_cc_is_source(cc) ((cc) == SPRD_TYPEC_CC_RD)
#define sprd_tcpm_cc_is_audio(cc) ((cc) == SPRD_TYPEC_CC_RA)
#define sprd_tcpm_cc_is_open(cc) ((cc) == SPRD_TYPEC_CC_OPEN)

#define sprd_tcpm_port_is_source(port) \
	((sprd_tcpm_cc_is_source((port)->cc1) && \
	 !sprd_tcpm_cc_is_source((port)->cc2)) || \
	 (sprd_tcpm_cc_is_source((port)->cc2) && \
	  !sprd_tcpm_cc_is_source((port)->cc1)))

#define sprd_tcpm_port_is_debug(port) \
	(sprd_tcpm_cc_is_source((port)->cc1) && sprd_tcpm_cc_is_source((port)->cc2))

#define sprd_tcpm_port_is_audio(port) \
	(sprd_tcpm_cc_is_audio((port)->cc1) && sprd_tcpm_cc_is_audio((port)->cc2))

#define sprd_tcpm_port_is_audio_detached(port) \
	((sprd_tcpm_cc_is_audio((port)->cc1) && sprd_tcpm_cc_is_open((port)->cc2)) || \
	 (sprd_tcpm_cc_is_audio((port)->cc2) && sprd_tcpm_cc_is_open((port)->cc1)))

#define sprd_tcpm_try_snk(port) \
	((port)->try_snk_count == 0 && (port)->try_role == TYPEC_SINK && \
	(port)->port_type == TYPEC_PORT_DRP)

#define sprd_tcpm_try_src(port) \
	((port)->try_src_count == 0 && (port)->try_role == TYPEC_SOURCE && \
	(port)->port_type == TYPEC_PORT_DRP)

static enum sprd_tcpm_state sprd_tcpm_default_state(struct sprd_tcpm_port *port)
{
	if (port->port_type == TYPEC_PORT_DRP) {
		if (port->try_role == TYPEC_SINK)
			return SNK_UNATTACHED;
		else if (port->try_role == TYPEC_SOURCE)
			return SRC_UNATTACHED;
		else if (port->tcpc->config &&
			 port->tcpc->config->default_role == TYPEC_SINK)
			return SNK_UNATTACHED;
		/* Fall through to return SRC_UNATTACHED */
	} else if (port->port_type == TYPEC_PORT_SNK) {
		return SNK_UNATTACHED;
	}
	return SRC_UNATTACHED;
}

static bool sprd_tcpm_port_is_disconnected(struct sprd_tcpm_port *port)
{
	return (!port->attached && port->cc1 == SPRD_TYPEC_CC_OPEN &&
		port->cc2 == SPRD_TYPEC_CC_OPEN) ||
	       (port->attached && ((port->polarity == SPRD_TYPEC_POLARITY_CC1 &&
				    port->cc1 == SPRD_TYPEC_CC_OPEN) ||
				   (port->polarity == SPRD_TYPEC_POLARITY_CC2 &&
				    port->cc2 == SPRD_TYPEC_CC_OPEN)));
}

/*
 * Logging
 */

#ifdef CONFIG_DEBUG_FS

static bool sprd_tcpm_log_full(struct sprd_tcpm_port *port)
{
	return port->logbuffer_tail ==
		(port->logbuffer_head + 1) % SPRD_LOG_BUFFER_ENTRIES;
}

__printf(2, 0)
static void _sprd_tcpm_log(struct sprd_tcpm_port *port, const char *fmt, va_list args)
{
	char tmpbuffer[SPRD_LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	mutex_lock(&port->logbuffer_lock);
	if (port->logbuffer_head < 0 ||
	    port->logbuffer_head >= SPRD_LOG_BUFFER_ENTRIES) {
		dev_warn(port->dev,
			 "Bad log buffer index %d\n", port->logbuffer_head);
		goto abort;
	}

	if (!port->logbuffer[port->logbuffer_head]) {
		port->logbuffer[port->logbuffer_head] =
				kzalloc(SPRD_LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!port->logbuffer[port->logbuffer_head]) {
			mutex_unlock(&port->logbuffer_lock);
			return;
		}
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	if (sprd_tcpm_log_full(port)) {
		port->logbuffer_head = max(port->logbuffer_head - 1, 0);
		strcpy(tmpbuffer, "overflow");
	}

	if (!port->logbuffer[port->logbuffer_head]) {
		dev_warn(port->dev,
			 "Log buffer index %d is NULL\n", port->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(port->logbuffer[port->logbuffer_head],
		  SPRD_LOG_BUFFER_ENTRY_SIZE, "[%5lu.%06lu] %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	port->logbuffer_head = (port->logbuffer_head + 1) % SPRD_LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&port->logbuffer_lock);
}

__printf(2, 3)
static void sprd_tcpm_log(struct sprd_tcpm_port *port, const char *fmt, ...)
{
	va_list args;

	/* Do not log while disconnected and unattached */
	if (sprd_tcpm_port_is_disconnected(port) &&
	    (port->state == SRC_UNATTACHED || port->state == SNK_UNATTACHED ||
	     port->state == TOGGLING))
		return;

	va_start(args, fmt);
	_sprd_tcpm_log(port, fmt, args);
	va_end(args);
}

__printf(2, 3)
static void sprd_tcpm_log_force(struct sprd_tcpm_port *port, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_sprd_tcpm_log(port, fmt, args);
	va_end(args);
}

static void sprd_tcpm_log_source_caps(struct sprd_tcpm_port *port)
{
	int i;

	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);
		char msg[64];

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			scnprintf(msg, sizeof(msg),
				  "%u mV, %u mA [%s%s%s%s%s%s]",
				  sprd_pdo_fixed_voltage(pdo),
				  sprd_pdo_max_current(pdo),
				  (pdo & SPRD_PDO_FIXED_DUAL_ROLE) ?
							"R" : "",
				  (pdo & SPRD_PDO_FIXED_SUSPEND) ?
							"S" : "",
				  (pdo & SPRD_PDO_FIXED_HIGHER_CAP) ?
							"H" : "",
				  (pdo & SPRD_PDO_FIXED_USB_COMM) ?
							"U" : "",
				  (pdo & SPRD_PDO_FIXED_DATA_SWAP) ?
							"D" : "",
				  (pdo & SPRD_PDO_FIXED_EXTPOWER) ?
							"E" : "");
			break;
		case SPRD_PDO_TYPE_VAR:
			scnprintf(msg, sizeof(msg),
				  "%u-%u mV, %u mA",
				  sprd_pdo_min_voltage(pdo),
				  sprd_pdo_max_voltage(pdo),
				  sprd_pdo_max_current(pdo));
			break;
		case SPRD_PDO_TYPE_BATT:
			scnprintf(msg, sizeof(msg),
				  "%u-%u mV, %u mW",
				  sprd_pdo_min_voltage(pdo),
				  sprd_pdo_max_voltage(pdo),
				  sprd_pdo_max_power(pdo));
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS)
				scnprintf(msg, sizeof(msg),
					  "%u-%u mV, %u mA",
					  sprd_pdo_pps_apdo_min_voltage(pdo),
					  sprd_pdo_pps_apdo_max_voltage(pdo),
					  sprd_pdo_pps_apdo_max_current(pdo));
			else
				strcpy(msg, "undefined APDO");
			break;
		default:
			strcpy(msg, "undefined");
			break;
		}
		sprd_tcpm_log(port, " PDO %d: type %d, %s",
			 i, type, msg);
	}
}

static int sprd_tcpm_debug_show(struct seq_file *s, void *v)
{
	struct sprd_tcpm_port *port = (struct sprd_tcpm_port *)s->private;
	int tail;

	mutex_lock(&port->logbuffer_lock);
	tail = port->logbuffer_tail;
	while (tail != port->logbuffer_head) {
		seq_printf(s, "%s\n", port->logbuffer[tail]);
		tail = (tail + 1) % SPRD_LOG_BUFFER_ENTRIES;
	}
	if (!seq_has_overflowed(s))
		port->logbuffer_tail = tail;
	mutex_unlock(&port->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sprd_tcpm_debug);

static struct dentry *rootdir;

static void sprd_tcpm_debugfs_init(struct sprd_tcpm_port *port)
{
	mutex_init(&port->logbuffer_lock);
	/* /sys/kernel/debug/tcpm/usbcX */
	if (!rootdir)
		rootdir = debugfs_create_dir("sprd_tcpm", NULL);

	port->dentry = debugfs_create_file(dev_name(port->dev),
					   S_IFREG | 0444, rootdir,
					   port, &sprd_tcpm_debug_fops);
}

static void sprd_tcpm_debugfs_exit(struct sprd_tcpm_port *port)
{
	int i;

	mutex_lock(&port->logbuffer_lock);
	for (i = 0; i < SPRD_LOG_BUFFER_ENTRIES; i++) {
		kfree(port->logbuffer[i]);
		port->logbuffer[i] = NULL;
	}
	mutex_unlock(&port->logbuffer_lock);

	debugfs_remove(port->dentry);
}

#else

__printf(2, 3)
static void sprd_tcpm_log(const struct sprd_tcpm_port *port, const char *fmt, ...) { }
__printf(2, 3)
static void sprd_tcpm_log_force(struct sprd_tcpm_port *port, const char *fmt, ...) { }
static void sprd_tcpm_log_source_caps(struct sprd_tcpm_port *port) { }
static void sprd_tcpm_debugfs_init(const struct sprd_tcpm_port *port) { }
static void sprd_tcpm_debugfs_exit(const struct sprd_tcpm_port *port) { }

#endif

static int sprd_tcpm_pd_transmit(struct sprd_tcpm_port *port,
				 enum sprd_tcpm_transmit_type type,
				 const struct sprd_pd_message *msg)
{
	unsigned long timeout;
	int ret;

	if (msg)
		sprd_tcpm_log(port, "PD TX, header: %#x", le16_to_cpu(msg->header));
	else
		sprd_tcpm_log(port, "PD TX, type: %#x", type);

	reinit_completion(&port->tx_complete);
	ret = port->tcpc->pd_transmit(port->tcpc, type, msg);
	if (ret < 0)
		return ret;

	mutex_unlock(&port->lock);
	timeout = wait_for_completion_timeout(&port->tx_complete,
					      msecs_to_jiffies(SPRD_PD_T_TCPC_TX_TIMEOUT));
	mutex_lock(&port->lock);
	if (!timeout)
		return -ETIMEDOUT;

	switch (port->tx_status) {
	case SPRD_TCPC_TX_SUCCESS:
		port->message_id = (port->message_id + 1) & SPRD_PD_HEADER_ID_MASK;
		return 0;
	case SPRD_TCPC_TX_DISCARDED:
		return -EAGAIN;
	case SPRD_TCPC_TX_FAILED:
	default:
		return -EIO;
	}
}

void sprd_tcpm_pd_transmit_complete(struct sprd_tcpm_port *port,
				    enum sprd_tcpm_transmit_status status)
{
	sprd_tcpm_log(port, "PD TX complete, status: %u", status);
	port->tx_status = status;
	complete(&port->tx_complete);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_transmit_complete);

static int sprd_tcpm_mux_set(struct sprd_tcpm_port *port, int state,
			     enum usb_role usb_role,
			     enum typec_orientation orientation)
{
	int ret;

	sprd_tcpm_log(port, "Requesting mux state %d, usb-role %d, orientation %d",
		      state, usb_role, orientation);

	ret = typec_set_orientation(port->typec_port, orientation);
	if (ret)
		return ret;

	if (port->role_sw) {
		ret = usb_role_switch_set_role(port->role_sw, usb_role);
		if (ret)
			return ret;
	}

	return typec_set_mode(port->typec_port, state);
}

static int sprd_tcpm_set_polarity(struct sprd_tcpm_port *port,
				  enum sprd_typec_cc_polarity polarity)
{
	int ret;

	sprd_tcpm_log(port, "polarity %d", polarity);

	ret = port->tcpc->set_polarity(port->tcpc, polarity);
	if (ret < 0)
		return ret;

	port->polarity = polarity;

	return 0;
}

static int sprd_tcpm_set_vconn(struct sprd_tcpm_port *port, bool enable)
{
	int ret;

	sprd_tcpm_log(port, "vconn:=%d", enable);

	ret = port->tcpc->set_vconn(port->tcpc, enable);
	if (!ret) {
		port->vconn_role = enable ? TYPEC_SOURCE : TYPEC_SINK;
		typec_set_vconn_role(port->typec_port, port->vconn_role);
	}

	return ret;
}

static u32 sprd_tcpm_get_current_limit(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_status cc;
	u32 limit;

	cc = port->polarity ? port->cc2 : port->cc1;
	switch (cc) {
	case SPRD_TYPEC_CC_RP_1_5:
		limit = 1500;
		break;
	case SPRD_TYPEC_CC_RP_3_0:
		limit = 3000;
		break;
	case SPRD_TYPEC_CC_RP_DEF:
	default:
		if (port->tcpc->get_current_limit)
			limit = port->tcpc->get_current_limit(port->tcpc);
		else
			limit = 0;
		break;
	}

	return limit;
}

static int sprd_tcpm_set_current_limit(struct sprd_tcpm_port *port, u32 max_ma, u32 mv)
{
	int ret = -EOPNOTSUPP;

	sprd_tcpm_log(port, "Setting voltage/current limit %u mV %u mA", mv, max_ma);

	port->supply_voltage = mv;
	port->current_limit = max_ma;

	if (port->tcpc->set_current_limit)
		ret = port->tcpc->set_current_limit(port->tcpc, max_ma, mv);

	return ret;
}

/*
 * Determine RP value to set based on maximum current supported
 * by a port if configured as source.
 * Returns CC value to report to link partner.
 */
static enum sprd_typec_cc_status sprd_tcpm_rp_cc(struct sprd_tcpm_port *port)
{
	const u32 *src_pdo = port->src_pdo;
	int nr_pdo = port->nr_src_pdo;
	int i;

	/*
	 * Search for first entry with matching voltage.
	 * It should report the maximum supported current.
	 */
	for (i = 0; i < nr_pdo; i++) {
		const u32 pdo = src_pdo[i];

		if (sprd_pdo_type(pdo) == SPRD_PDO_TYPE_FIXED &&
		    sprd_pdo_fixed_voltage(pdo) == 5000) {
			unsigned int curr = sprd_pdo_max_current(pdo);

			if (curr >= 3000)
				return SPRD_TYPEC_CC_RP_3_0;
			else if (curr >= 1500)
				return SPRD_TYPEC_CC_RP_1_5;
			return SPRD_TYPEC_CC_RP_DEF;
		}
	}

	return SPRD_TYPEC_CC_RP_DEF;
}

static int sprd_tcpm_set_attached_state(struct sprd_tcpm_port *port, bool attached)
{
	return port->tcpc->set_roles(port->tcpc, attached, port->pwr_role,
				     port->data_role);
}

static int sprd_tcpm_set_roles(struct sprd_tcpm_port *port, bool attached,
			       enum typec_role role, enum typec_data_role data)
{
	enum typec_orientation orientation;
	enum usb_role usb_role;
	int ret;

	if (port->polarity == SPRD_TYPEC_POLARITY_CC1)
		orientation = TYPEC_ORIENTATION_NORMAL;
	else
		orientation = TYPEC_ORIENTATION_REVERSE;

	if (data == TYPEC_HOST)
		usb_role = USB_ROLE_HOST;
	else
		usb_role = USB_ROLE_DEVICE;

	ret = sprd_tcpm_mux_set(port, TYPEC_STATE_USB, usb_role, orientation);
	if (ret < 0)
		return ret;

	ret = port->tcpc->set_roles(port->tcpc, attached, role, data);
	if (ret < 0)
		return ret;

	port->pwr_role = role;
	port->data_role = data;
	typec_set_data_role(port->typec_port, data);
	typec_set_pwr_role(port->typec_port, role);

	return 0;
}

static int sprd_tcpm_set_pwr_role(struct sprd_tcpm_port *port, enum typec_role role)
{
	int ret;

	ret = port->tcpc->set_roles(port->tcpc, true, role,
				    port->data_role);
	if (ret < 0)
		return ret;

	port->pwr_role = role;
	typec_set_pwr_role(port->typec_port, role);

	return 0;
}

static int sprd_tcpm_pd_send_source_caps(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int i;

	memset(&msg, 0, sizeof(msg));
	if (!port->nr_src_pdo) {
		/* No source capabilities defined, sink only */
		msg.header = SPRD_PD_HEADER_LE(SPRD_PD_CTRL_REJECT,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, 0);
	} else {
		msg.header = SPRD_PD_HEADER_LE(SPRD_PD_DATA_SOURCE_CAP,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id,
					       port->nr_src_pdo);
	}
	for (i = 0; i < port->nr_src_pdo; i++)
		msg.payload[i] = cpu_to_le32(port->src_pdo[i]);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_send_sink_caps(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int i;

	memset(&msg, 0, sizeof(msg));
	if (!port->nr_snk_pdo) {
		/* No sink capabilities defined, source only */
		msg.header = SPRD_PD_HEADER_LE(SPRD_PD_CTRL_REJECT,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, 0);
	} else {
		msg.header = SPRD_PD_HEADER_LE(SPRD_PD_DATA_SINK_CAP,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id,
					       port->nr_snk_pdo);
	}
	for (i = 0; i < port->nr_snk_pdo; i++)
		msg.payload[i] = cpu_to_le32(port->snk_pdo[i]);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static void sprd_tcpm_set_state(struct sprd_tcpm_port *port, enum sprd_tcpm_state state,
				unsigned int delay_ms)
{
	if (delay_ms) {
		sprd_tcpm_log(port, "pending state change %s -> %s @ %u ms",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state],
			      delay_ms);
		port->delayed_state = state;
		mod_delayed_work(port->wq, &port->state_machine,
				 msecs_to_jiffies(delay_ms));
		port->delayed_runtime = jiffies + msecs_to_jiffies(delay_ms);
		port->delay_ms = delay_ms;
	} else {
		sprd_tcpm_log(port, "state change %s -> %s",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state]);
		port->delayed_state = INVALID_STATE;
		port->prev_state = port->state;
		port->state = state;
		/*
		 * Don't re-queue the state machine work item if we're currently
		 * in the state machine and we're immediately changing states.
		 * sprd_tcpm_state_machine_work() will continue running the state
		 * machine.
		 */
		if (!port->state_machine_running)
			mod_delayed_work(port->wq, &port->state_machine, 0);
	}
}

static void sprd_tcpm_set_state_cond(struct sprd_tcpm_port *port, enum sprd_tcpm_state state,
				     unsigned int delay_ms)
{
	if (port->enter_state == port->state)
		sprd_tcpm_set_state(port, state, delay_ms);
	else
		sprd_tcpm_log(port,
			      "skipped %sstate change %s -> %s [%u ms], context state %s",
			      delay_ms ? "delayed " : "",
			      sprd_tcpm_states[port->state], sprd_tcpm_states[state],
			      delay_ms, sprd_tcpm_states[port->enter_state]);
}

static void sprd_tcpm_queue_message(struct sprd_tcpm_port *port,
				    enum sprd_pd_msg_request message)
{
	port->queued_message = message;
	mod_delayed_work(port->wq, &port->state_machine, 0);
}

/*
 * VDM/VDO handling functions
 */
static void sprd_tcpm_queue_vdm(struct sprd_tcpm_port *port, const u32 header,
				const u32 *data, int cnt)
{
	port->vdo_count = cnt + 1;
	port->vdo_data[0] = header;
	memcpy(&port->vdo_data[1], data, sizeof(u32) * cnt);
	/* Set ready, vdm state machine will actually send */
	port->vdm_retries = 0;
	port->vdm_state = VDM_STATE_READY;
}

static void sprd_svdm_consume_identity(struct sprd_tcpm_port *port,
				       const __le32 *payload, int cnt)
{
	u32 vdo = le32_to_cpu(payload[VDO_INDEX_IDH]);
	u32 product = le32_to_cpu(payload[VDO_INDEX_PRODUCT]);

	memset(&port->mode_data, 0, sizeof(port->mode_data));

	port->partner_ident.id_header = vdo;
	port->partner_ident.cert_stat = le32_to_cpu(payload[VDO_INDEX_CSTAT]);
	port->partner_ident.product = product;

	typec_partner_set_identity(port->partner);

	sprd_tcpm_log(port, "Identity: %04x:%04x.%04x",
		      PD_IDH_VID(vdo),
		      PD_PRODUCT_PID(product), product & 0xffff);
}

static bool sprd_svdm_consume_svids(struct sprd_tcpm_port *port,
				    const __le32 *payload, int cnt)
{
	struct sprd_pd_mode_data *pmdata = &port->mode_data;
	int i;

	for (i = 1; i < cnt; i++) {
		u32 p = le32_to_cpu(payload[i]);
		u16 svid;

		svid = (p >> 16) & 0xffff;
		if (!svid)
			return false;

		if (pmdata->nsvids >= SPRD_SVID_DISCOVERY_MAX)
			goto abort;

		pmdata->svids[pmdata->nsvids++] = svid;
		sprd_tcpm_log(port, "SVID %d: 0x%x", pmdata->nsvids, svid);

		svid = p & 0xffff;
		if (!svid)
			return false;

		if (pmdata->nsvids >= SPRD_SVID_DISCOVERY_MAX)
			goto abort;

		pmdata->svids[pmdata->nsvids++] = svid;
		sprd_tcpm_log(port, "SVID %d: 0x%x", pmdata->nsvids, svid);
	}
	return true;
abort:
	sprd_tcpm_log(port, "SPRD_SVID_DISCOVERY_MAX(%d) too low!", SPRD_SVID_DISCOVERY_MAX);
	return false;
}

static void sprd_svdm_consume_modes(struct sprd_tcpm_port *port,
				    const __le32 *payload, int cnt)
{
	struct sprd_pd_mode_data *pmdata = &port->mode_data;
	struct typec_altmode_desc *paltmode;
	int i;

	if (pmdata->altmodes >= ARRAY_SIZE(port->partner_altmode)) {
		/* Already logged in sprd_svdm_consume_svids() */
		return;
	}

	for (i = 1; i < cnt; i++) {
		paltmode = &pmdata->altmode_desc[pmdata->altmodes];
		memset(paltmode, 0, sizeof(*paltmode));

		paltmode->svid = pmdata->svids[pmdata->svid_index];
		paltmode->mode = i;
		paltmode->vdo = le32_to_cpu(payload[i]);

		sprd_tcpm_log(port, " Alternate mode %d: SVID 0x%04x, VDO %d: 0x%08x",
			      pmdata->altmodes, paltmode->svid,
			      paltmode->mode, paltmode->vdo);

		pmdata->altmodes++;
	}
}

static void sprd_tcpm_register_partner_altmodes(struct sprd_tcpm_port *port)
{
	struct sprd_pd_mode_data *modep = &port->mode_data;
	struct typec_altmode *altmode;
	int i;

	for (i = 0; i < modep->altmodes; i++) {
		altmode = typec_partner_register_altmode(port->partner, &modep->altmode_desc[i]);
		if (!altmode)
			sprd_tcpm_log(port, "Failed to register partner SVID 0x%04x",
				      modep->altmode_desc[i].svid);
		port->partner_altmode[i] = altmode;
	}
}

#define supports_modal(port)	PD_IDH_MODAL_SUPP((port)->partner_ident.id_header)

static int sprd_tcpm_pd_svdm(struct sprd_tcpm_port *port,
			     const __le32 *payload, int cnt, u32 *response)
{
	struct typec_altmode *adev;
	struct typec_altmode *pdev;
	struct sprd_pd_mode_data *modep;
	u32 p[SPRD_PD_MAX_PAYLOAD];
	int rlen = 0;
	int cmd_type;
	int cmd;
	int i;

	for (i = 0; i < cnt; i++)
		p[i] = le32_to_cpu(payload[i]);

	cmd_type = PD_VDO_CMDT(p[0]);
	cmd = PD_VDO_CMD(p[0]);

	sprd_tcpm_log(port, "Rx VDM cmd 0x%x type %d cmd %d len %d", p[0], cmd_type, cmd, cnt);

	modep = &port->mode_data;

	adev = typec_match_altmode(port->port_altmode, SPRD_ALTMODE_DISCOVERY_MAX,
				   PD_VDO_VID(p[0]), PD_VDO_OPOS(p[0]));

	pdev = typec_match_altmode(port->partner_altmode, SPRD_ALTMODE_DISCOVERY_MAX,
				   PD_VDO_VID(p[0]), PD_VDO_OPOS(p[0]));

	switch (cmd_type) {
	case CMDT_INIT:
		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			/* 6.4.4.3.1: Only respond as UFP (device) */
			if (port->data_role == TYPEC_DEVICE &&
			    port->nr_snk_vdo) {
				for (i = 0; i <  port->nr_snk_vdo; i++)
					response[i + 1] = port->snk_vdo[i];
				rlen = port->nr_snk_vdo + 1;
			}
			break;
		case CMD_DISCOVER_SVID:
			break;
		case CMD_DISCOVER_MODES:
			break;
		case CMD_ENTER_MODE:
			break;
		case CMD_EXIT_MODE:
			break;
		case CMD_ATTENTION:
			/* Attention command does not have response */
			if (adev)
				typec_altmode_attention(adev, p[1]);
			return 0;
		default:
			break;
		}
		if (rlen >= 1) {
			response[0] = p[0] | VDO_CMDT(CMDT_RSP_ACK);
		} else if (rlen == 0) {
			response[0] = p[0] | VDO_CMDT(CMDT_RSP_NAK);
			rlen = 1;
		} else {
			response[0] = p[0] | VDO_CMDT(CMDT_RSP_BUSY);
			rlen = 1;
		}
		break;
	case CMDT_RSP_ACK:
		/* silently drop message if we are not connected */
		if (IS_ERR_OR_NULL(port->partner))
			break;

		switch (cmd) {
		case CMD_DISCOVER_IDENT:
			/* 6.4.4.3.1 */
			sprd_svdm_consume_identity(port, payload, cnt);
			response[0] = VDO(USB_SID_PD, 1, CMD_DISCOVER_SVID);
			rlen = 1;
			break;
		case CMD_DISCOVER_SVID:
			/* 6.4.4.3.2 */
			if (sprd_svdm_consume_svids(port, payload, cnt)) {
				response[0] = VDO(USB_SID_PD, 1,
						  CMD_DISCOVER_SVID);
				rlen = 1;
			} else if (modep->nsvids && supports_modal(port)) {
				response[0] = VDO(modep->svids[0], 1,
						  CMD_DISCOVER_MODES);
				rlen = 1;
			}
			break;
		case CMD_DISCOVER_MODES:
			/* 6.4.4.3.3 */
			sprd_svdm_consume_modes(port, payload, cnt);
			modep->svid_index++;
			if (modep->svid_index < modep->nsvids) {
				u16 svid = modep->svids[modep->svid_index];
				response[0] = VDO(svid, 1, CMD_DISCOVER_MODES);
				rlen = 1;
			} else {
				sprd_tcpm_register_partner_altmodes(port);
			}
			break;
		case CMD_ENTER_MODE:
			if (adev && pdev) {
				typec_altmode_update_active(pdev, true);

				if (typec_altmode_vdm(adev, p[0], &p[1], cnt)) {
					response[0] = VDO(adev->svid, 1,
							  CMD_EXIT_MODE);
					response[0] |= VDO_OPOS(adev->mode);
					return 1;
				}
			}
			return 0;
		case CMD_EXIT_MODE:
			if (adev && pdev) {
				typec_altmode_update_active(pdev, false);

				/* Back to USB Operation */
				WARN_ON(typec_altmode_notify(adev,
							     TYPEC_STATE_USB,
							     NULL));
			}
			break;
		default:
			break;
		}
		break;
	case CMDT_RSP_NAK:
		switch (cmd) {
		case CMD_ENTER_MODE:
			/* Back to USB Operation */
			if (adev)
				WARN_ON(typec_altmode_notify(adev,
							     TYPEC_STATE_USB,
							     NULL));
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	/* Informing the alternate mode drivers about everything */
	if (adev)
		typec_altmode_vdm(adev, p[0], &p[1], cnt);

	return rlen;
}

static void sprd_tcpm_handle_vdm_request(struct sprd_tcpm_port *port,
					 const __le32 *payload, int cnt)
{
	int rlen = 0;
	u32 response[8] = { };
	u32 p0 = le32_to_cpu(payload[0]);

	if (port->vdm_state == VDM_STATE_BUSY) {
		/* If UFP responded busy retry after timeout */
		if (PD_VDO_CMDT(p0) == CMDT_RSP_BUSY) {
			port->vdm_state = VDM_STATE_WAIT_RSP_BUSY;
			port->vdo_retry = (p0 & ~VDO_CMDT_MASK) |
				CMDT_INIT;
			mod_delayed_work(port->wq, &port->vdm_state_machine,
					 msecs_to_jiffies(PD_T_VDM_BUSY));
			return;
		}
		port->vdm_state = VDM_STATE_DONE;
	}

	if (PD_VDO_SVDM(p0))
		rlen = sprd_tcpm_pd_svdm(port, payload, cnt, response);

	if (rlen > 0) {
		sprd_tcpm_queue_vdm(port, response[0], &response[1], rlen - 1);
		mod_delayed_work(port->wq, &port->vdm_state_machine, 0);
	}
}

static void sprd_tcpm_send_vdm(struct sprd_tcpm_port *port,
			       u32 vid, int cmd, const u32 *data, int count)
{
	u32 header;

	if (WARN_ON(count > VDO_MAX_SIZE - 1))
		count = VDO_MAX_SIZE - 1;

	/* set VDM header with VID & CMD */
	header = VDO(vid, ((vid & USB_SID_PD) == USB_SID_PD) ?
		     1 : (PD_VDO_CMD(cmd) <= CMD_ATTENTION), cmd);
	sprd_tcpm_queue_vdm(port, header, data, count);

	mod_delayed_work(port->wq, &port->vdm_state_machine, 0);
}

static unsigned int sprd_vdm_ready_timeout(u32 vdm_hdr)
{
	unsigned int timeout;
	int cmd = PD_VDO_CMD(vdm_hdr);

	/* its not a structured VDM command */
	if (!PD_VDO_SVDM(vdm_hdr))
		return PD_T_VDM_UNSTRUCTURED;

	switch (PD_VDO_CMDT(vdm_hdr)) {
	case CMDT_INIT:
		if (cmd == CMD_ENTER_MODE || cmd == CMD_EXIT_MODE)
			timeout = PD_T_VDM_WAIT_MODE_E;
		else
			timeout = PD_T_VDM_SNDR_RSP;
		break;
	default:
		if (cmd == CMD_ENTER_MODE || cmd == CMD_EXIT_MODE)
			timeout = PD_T_VDM_E_MODE;
		else
			timeout = PD_T_VDM_RCVR_RSP;
		break;
	}
	return timeout;
}

static void sprd_vdm_run_state_machine(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int i, res;

	switch (port->vdm_state) {
	case VDM_STATE_READY:
		/* Only transmit VDM if attached */
		if (!port->attached) {
			port->vdm_state = VDM_STATE_ERR_BUSY;
			break;
		}

		/*
		 * if there's traffic or we're not in PDO ready state don't send
		 * a VDM.
		 */
		if (port->state != SRC_READY && port->state != SNK_READY)
			break;

		/* Prepare and send VDM */
		memset(&msg, 0, sizeof(msg));
		msg.header = SPRD_PD_HEADER_LE(SPRD_PD_DATA_VENDOR_DEF,
					       port->pwr_role,
					       port->data_role,
					       port->negotiated_rev,
					       port->message_id, port->vdo_count);
		for (i = 0; i < port->vdo_count; i++)
			msg.payload[i] = cpu_to_le32(port->vdo_data[i]);
		res = sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
		if (res < 0) {
			port->vdm_state = VDM_STATE_ERR_SEND;
		} else {
			unsigned long timeout;

			port->vdm_retries = 0;
			port->vdm_state = VDM_STATE_BUSY;
			timeout = sprd_vdm_ready_timeout(port->vdo_data[0]);
			mod_delayed_work(port->wq, &port->vdm_state_machine,
					 timeout);
		}
		break;
	case VDM_STATE_WAIT_RSP_BUSY:
		port->vdo_data[0] = port->vdo_retry;
		port->vdo_count = 1;
		port->vdm_state = VDM_STATE_READY;
		break;
	case VDM_STATE_BUSY:
		port->vdm_state = VDM_STATE_ERR_TMOUT;
		break;
	case VDM_STATE_ERR_SEND:
		/*
		 * A partner which does not support USB PD will not reply,
		 * so this is not a fatal error. At the same time, some
		 * devices may not return GoodCRC under some circumstances,
		 * so we need to retry.
		 */
		if (port->vdm_retries < 3) {
			sprd_tcpm_log(port, "VDM Tx error, retry");
			port->vdm_retries++;
			port->vdm_state = VDM_STATE_READY;
		}
		break;
	default:
		break;
	}
}

static void sprd_vdm_state_machine_work(struct work_struct *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   vdm_state_machine.work);
	enum sprd_vdm_states prev_state;

	mutex_lock(&port->lock);

	/*
	 * Continue running as long as the port is not busy and there was
	 * a state change.
	 */
	do {
		prev_state = port->vdm_state;
		sprd_vdm_run_state_machine(port);
	} while (port->vdm_state != prev_state &&
		 port->vdm_state != VDM_STATE_BUSY);

	mutex_unlock(&port->lock);
}

enum sprd_pdo_err {
	PDO_NO_ERR,
	PDO_ERR_NO_VSAFE5V,
	PDO_ERR_VSAFE5V_NOT_FIRST,
	PDO_ERR_PDO_TYPE_NOT_IN_ORDER,
	PDO_ERR_FIXED_NOT_SORTED,
	PDO_ERR_VARIABLE_BATT_NOT_SORTED,
	PDO_ERR_DUPE_PDO,
	PDO_ERR_PPS_APDO_NOT_SORTED,
	PDO_ERR_DUPE_PPS_APDO,
};

static const char * const sprd_pdo_err_msg[] = {
	[PDO_ERR_NO_VSAFE5V] =
	" err: source/sink caps should atleast have vSafe5V",
	[PDO_ERR_VSAFE5V_NOT_FIRST] =
	" err: vSafe5V Fixed Supply Object Shall always be the first object",
	[PDO_ERR_PDO_TYPE_NOT_IN_ORDER] =
	" err: PDOs should be in the following order: Fixed; Battery; Variable",
	[PDO_ERR_FIXED_NOT_SORTED] =
	" err: Fixed supply pdos should be in increasing order of their fixed voltage",
	[PDO_ERR_VARIABLE_BATT_NOT_SORTED] =
	" err: Variable/Battery supply pdos should be in increasing order of their minimum voltage",
	[PDO_ERR_DUPE_PDO] =
	" err: Variable/Batt supply pdos cannot have same min/max voltage",
	[PDO_ERR_PPS_APDO_NOT_SORTED] =
	" err: Programmable power supply apdos should be in increasing order of their maximum voltage",
	[PDO_ERR_DUPE_PPS_APDO] =
	" err: Programmable power supply apdos cannot have same min/max voltage and max current",
};

static enum sprd_pdo_err sprd_tcpm_caps_err(struct sprd_tcpm_port *port, const u32 *pdo,
					    unsigned int nr_pdo)
{
	unsigned int i;

	/* Should at least contain vSafe5v */
	if (nr_pdo < 1)
		return PDO_ERR_NO_VSAFE5V;

	/* The vSafe5V Fixed Supply Object Shall always be the first object */
	if (sprd_pdo_type(pdo[0]) != SPRD_PDO_TYPE_FIXED ||
	    sprd_pdo_fixed_voltage(pdo[0]) != SPRD_VSAFE5V)
		return PDO_ERR_VSAFE5V_NOT_FIRST;

	for (i = 1; i < nr_pdo; i++) {
		if (sprd_pdo_type(pdo[i]) < sprd_pdo_type(pdo[i - 1])) {
			return PDO_ERR_PDO_TYPE_NOT_IN_ORDER;
		} else if (sprd_pdo_type(pdo[i]) == sprd_pdo_type(pdo[i - 1])) {
			enum sprd_pd_pdo_type type = sprd_pdo_type(pdo[i]);

			switch (type) {
			/*
			 * The remaining Fixed Supply Objects, if
			 * present, shall be sent in voltage order;
			 * lowest to highest.
			 */
			case SPRD_PDO_TYPE_FIXED:
				if (sprd_pdo_fixed_voltage(pdo[i]) <=
				    sprd_pdo_fixed_voltage(pdo[i - 1]))
					return PDO_ERR_FIXED_NOT_SORTED;
				break;
			/*
			 * The Battery Supply Objects and Variable
			 * supply, if present shall be sent in Minimum
			 * Voltage order; lowest to highest.
			 */
			case SPRD_PDO_TYPE_VAR:
			case SPRD_PDO_TYPE_BATT:
				if (sprd_pdo_min_voltage(pdo[i]) <
				    sprd_pdo_min_voltage(pdo[i - 1]))
					return PDO_ERR_VARIABLE_BATT_NOT_SORTED;
				else if ((sprd_pdo_min_voltage(pdo[i]) ==
					  sprd_pdo_min_voltage(pdo[i - 1])) &&
					 (sprd_pdo_max_voltage(pdo[i]) ==
					  sprd_pdo_max_voltage(pdo[i - 1])))
					return PDO_ERR_DUPE_PDO;
				break;
			/*
			 * The Programmable Power Supply APDOs, if present,
			 * shall be sent in Maximum Voltage order;
			 * lowest to highest.
			 */
			case SPRD_PDO_TYPE_APDO:
				if (sprd_pdo_apdo_type(pdo[i]) != SPRD_APDO_TYPE_PPS)
					break;

				if (sprd_pdo_pps_apdo_max_voltage(pdo[i]) <
				    sprd_pdo_pps_apdo_max_voltage(pdo[i - 1]))
					return PDO_ERR_PPS_APDO_NOT_SORTED;
				else if (sprd_pdo_pps_apdo_min_voltage(pdo[i]) ==
					  sprd_pdo_pps_apdo_min_voltage(pdo[i - 1]) &&
					 sprd_pdo_pps_apdo_max_voltage(pdo[i]) ==
					  sprd_pdo_pps_apdo_max_voltage(pdo[i - 1]) &&
					 sprd_pdo_pps_apdo_max_current(pdo[i]) ==
					  sprd_pdo_pps_apdo_max_current(pdo[i - 1]))
					return PDO_ERR_DUPE_PPS_APDO;
				break;
			default:
				sprd_tcpm_log_force(port, " Unknown pdo type");
			}
		}
	}

	return PDO_NO_ERR;
}

static int sprd_tcpm_validate_caps(struct sprd_tcpm_port *port, const u32 *pdo,
				   unsigned int nr_pdo)
{
	enum sprd_pdo_err err_index = sprd_tcpm_caps_err(port, pdo, nr_pdo);

	if (err_index != PDO_NO_ERR) {
		sprd_tcpm_log_force(port, " %s", sprd_pdo_err_msg[err_index]);
		return -EINVAL;
	}

	return 0;
}

static int sprd_tcpm_altmode_enter(struct typec_altmode *altmode)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);
	u32 header;

	mutex_lock(&port->lock);
	header = VDO(altmode->svid, 1, CMD_ENTER_MODE);
	header |= VDO_OPOS(altmode->mode);

	sprd_tcpm_queue_vdm(port, header, NULL, 0);
	mod_delayed_work(port->wq, &port->vdm_state_machine, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static int sprd_tcpm_altmode_exit(struct typec_altmode *altmode)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);
	u32 header;

	mutex_lock(&port->lock);
	header = VDO(altmode->svid, 1, CMD_EXIT_MODE);
	header |= VDO_OPOS(altmode->mode);

	sprd_tcpm_queue_vdm(port, header, NULL, 0);
	mod_delayed_work(port->wq, &port->vdm_state_machine, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static int sprd_tcpm_altmode_vdm(struct typec_altmode *altmode,
			    u32 header, const u32 *data, int count)
{
	struct sprd_tcpm_port *port = typec_altmode_get_drvdata(altmode);

	mutex_lock(&port->lock);
	sprd_tcpm_queue_vdm(port, header, data, count - 1);
	mod_delayed_work(port->wq, &port->vdm_state_machine, 0);
	mutex_unlock(&port->lock);

	return 0;
}

static const struct typec_altmode_ops sprd_tcpm_altmode_ops = {
	.enter = sprd_tcpm_altmode_enter,
	.exit = sprd_tcpm_altmode_exit,
	.vdm = sprd_tcpm_altmode_vdm,
};

/*
 * PD (data, control) command handling functions
 */
static inline enum sprd_tcpm_state sprd_ready_state(struct sprd_tcpm_port *port)
{
	if (port->pwr_role == TYPEC_SOURCE)
		return SRC_READY;
	else
		return SNK_READY;
}

static int sprd_tcpm_pd_send_control(struct sprd_tcpm_port *port,
				     enum sprd_pd_ctrl_msg_type type);

static void sprd_tcpm_handle_alert(struct sprd_tcpm_port *port,
				   const __le32 *payload, int cnt)
{
	u32 p0 = le32_to_cpu(payload[0]);
	unsigned int type = usb_pd_ado_type(p0);

	if (!type) {
		sprd_tcpm_log(port, "Alert message received with no type");
		return;
	}

	/* Just handling non-battery alerts for now */
	if (!(type & USB_PD_ADO_TYPE_BATT_STATUS_CHANGE)) {
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, GET_STATUS_SEND, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
	}
}

static void sprd_tcpm_pd_data_request(struct sprd_tcpm_port *port,
				      const struct sprd_pd_message *msg)
{
	enum sprd_pd_data_msg_type type = sprd_pd_header_type_le(msg->header);
	unsigned int cnt = sprd_pd_header_cnt_le(msg->header);
	unsigned int rev = sprd_pd_header_rev_le(msg->header);
	unsigned int i;

	switch (type) {
	case SPRD_PD_DATA_SOURCE_CAP:
		if (port->pwr_role != TYPEC_SINK)
			break;

		for (i = 0; i < cnt; i++)
			port->source_caps[i] = le32_to_cpu(msg->payload[i]);

		port->nr_source_caps = cnt;

		sprd_tcpm_log_source_caps(port);

		sprd_tcpm_validate_caps(port, port->source_caps, port->nr_source_caps);

		/*
		 * Adjust revision in subsequent message headers, as required,
		 * to comply with 6.2.1.1.5 of the USB PD 3.0 spec. We don't
		 * support Rev 1.0 so just do nothing in that scenario.
		 */
		if (rev == SPRD_PD_REV10)
			break;

		if (rev < SPRD_PD_MAX_REV)
			port->negotiated_rev = rev;

		/*
		 * This message may be received even if VBUS is not
		 * present. This is quite unexpected; see USB PD
		 * specification, sections 8.3.3.6.3.1 and 8.3.3.6.3.2.
		 * However, at the same time, we must be ready to
		 * receive this message and respond to it 15ms after
		 * receiving PS_RDY during power swap operations, no matter
		 * if VBUS is available or not (USB PD specification,
		 * section 6.5.9.2).
		 * So we need to accept the message either way,
		 * but be prepared to keep waiting for VBUS after it was
		 * handled.
		 */
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
		break;
	case SPRD_PD_DATA_REQUEST:
		if (port->pwr_role != TYPEC_SOURCE ||
		    cnt != 1) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}

		/*
		 * Adjust revision in subsequent message headers, as required,
		 * to comply with 6.2.1.1.5 of the USB PD 3.0 spec. We don't
		 * support Rev 1.0 so just reject in that scenario.
		 */
		if (rev == SPRD_PD_REV10) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}

		if (rev < SPRD_PD_MAX_REV)
			port->negotiated_rev = rev;

		port->sink_request = le32_to_cpu(msg->payload[0]);
		sprd_tcpm_set_state(port, SRC_NEGOTIATE_CAPABILITIES, 0);
		break;
	case SPRD_PD_DATA_SINK_CAP:
		/* We don't do anything with this at the moment... */
		for (i = 0; i < cnt; i++)
			port->sink_caps[i] = le32_to_cpu(msg->payload[i]);
		port->nr_sink_caps = cnt;
		break;
	case SPRD_PD_DATA_VENDOR_DEF:
		sprd_tcpm_handle_vdm_request(port, msg->payload, cnt);
		break;
	case SPRD_PD_DATA_BIST:
		if (port->state == SRC_READY || port->state == SNK_READY) {
			port->bist_request = le32_to_cpu(msg->payload[0]);
			sprd_tcpm_set_state(port, BIST_RX, 0);
		}
		break;
	case SPRD_PD_DATA_ALERT:
		sprd_tcpm_handle_alert(port, msg->payload, cnt);
		break;
	case SPRD_PD_DATA_BATT_STATUS:
	case SPRD_PD_DATA_GET_COUNTRY_INFO:
		/* Currently unsupported */
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		break;
	default:
		sprd_tcpm_log(port, "Unhandled data message type %#x", type);
		break;
	}
}

static void sprd_tcpm_pps_complete(struct sprd_tcpm_port *port, int result)
{
	if (port->pps_pending) {
		port->pps_status = result;
		port->pps_pending = false;
		complete(&port->pps_complete);
	}
}

static void sprd_tcpm_pd_ctrl_request(struct sprd_tcpm_port *port,
				      const struct sprd_pd_message *msg)
{
	enum sprd_pd_ctrl_msg_type type = sprd_pd_header_type_le(msg->header);
	enum sprd_tcpm_state next_state;

	switch (type) {
	case SPRD_PD_CTRL_GOOD_CRC:
	case SPRD_PD_CTRL_PING:
		break;
	case SPRD_PD_CTRL_GET_SOURCE_CAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_queue_message(port, PD_MSG_DATA_SOURCE_CAP);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GET_SINK_CAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_queue_message(port, PD_MSG_DATA_SINK_CAP);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GOTO_MIN:
		break;
	case SPRD_PD_CTRL_PS_RDY:
		switch (port->state) {
		case SNK_TRANSITION_SINK:
			if (port->vbus_present) {
				sprd_tcpm_set_current_limit(port,
							    port->req_current_limit,
							    port->req_supply_voltage);
				port->explicit_contract = true;
				sprd_tcpm_set_state(port, SNK_READY, 0);
			} else {
				/*
				 * Seen after power swap. Keep waiting for VBUS
				 * in a transitional state.
				 */
				sprd_tcpm_set_state(port, SNK_TRANSITION_SINK_VBUS, 0);
			}
			break;
		case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
			sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SINK_ON, 0);
			break;
		case PR_SWAP_SNK_SRC_SINK_OFF:
			sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SOURCE_ON, 0);
			break;
		case VCONN_SWAP_WAIT_FOR_VCONN:
			sprd_tcpm_set_state(port, VCONN_SWAP_TURN_OFF_VCONN, 0);
			break;
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_REJECT:
	case SPRD_PD_CTRL_WAIT:
	case SPRD_PD_CTRL_NOT_SUPP:
		switch (port->state) {
		case SNK_NEGOTIATE_CAPABILITIES:
			/* USB PD specification, Figure 8-43 */
			if (port->explicit_contract)
				next_state = SNK_READY;
			else
				next_state = SNK_WAIT_CAPABILITIES;
			sprd_tcpm_set_state(port, next_state, 0);
			break;
		case SNK_NEGOTIATE_PPS_CAPABILITIES:
			/* Revert data back from any requested PPS updates */
			port->pps_data.req_out_volt = port->supply_voltage;
			port->pps_data.req_op_curr = port->current_limit;
			port->pps_status = (type == SPRD_PD_CTRL_WAIT ?
					    -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, SNK_READY, 0);
			break;
		case DR_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, DR_SWAP_CANCEL, 0);
			break;
		case PR_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, PR_SWAP_CANCEL, 0);
			break;
		case VCONN_SWAP_SEND:
			port->swap_status = (type == SPRD_PD_CTRL_WAIT ?
					     -EAGAIN : -EOPNOTSUPP);
			sprd_tcpm_set_state(port, VCONN_SWAP_CANCEL, 0);
			break;
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_ACCEPT:
		switch (port->state) {
		case SNK_NEGOTIATE_CAPABILITIES:
			port->pps_data.active = false;
			sprd_tcpm_set_state(port, SNK_TRANSITION_SINK, 0);
			break;
		case SNK_NEGOTIATE_PPS_CAPABILITIES:
			port->pps_data.active = true;
			port->pps_data.min_volt = port->pps_data.req_min_volt;
			port->pps_data.max_volt = port->pps_data.req_max_volt;
			port->pps_data.max_curr = port->pps_data.req_max_curr;
			port->req_supply_voltage = port->pps_data.req_out_volt;
			port->req_current_limit = port->pps_data.req_op_curr;
			sprd_tcpm_set_state(port, SNK_TRANSITION_SINK, 0);
			break;
		case SOFT_RESET_SEND:
			port->message_id = 0;
			port->rx_msgid = -1;
			if (port->pwr_role == TYPEC_SOURCE)
				next_state = SRC_SEND_CAPABILITIES;
			else
				next_state = SNK_WAIT_CAPABILITIES;
			sprd_tcpm_set_state(port, next_state, 0);
			break;
		case DR_SWAP_SEND:
			sprd_tcpm_set_state(port, DR_SWAP_CHANGE_DR, 0);
			break;
		case PR_SWAP_SEND:
			sprd_tcpm_set_state(port, PR_SWAP_START, 0);
			break;
		case VCONN_SWAP_SEND:
			sprd_tcpm_set_state(port, VCONN_SWAP_START, 0);
			break;
		default:
			break;
		}
		break;
	case SPRD_PD_CTRL_SOFT_RESET:
		sprd_tcpm_set_state(port, SOFT_RESET, 0);
		break;
	case SPRD_PD_CTRL_DR_SWAP:
		if (port->port_type != TYPEC_PORT_DRP) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		/*
		 * XXX
		 * 6.3.9: If an alternate mode is active, a request to swap
		 * alternate modes shall trigger a port reset.
		 */
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, DR_SWAP_ACCEPT, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_PR_SWAP:
		if (port->port_type != TYPEC_PORT_DRP) {
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_REJECT);
			break;
		}
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, PR_SWAP_ACCEPT, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_VCONN_SWAP:
		switch (port->state) {
		case SRC_READY:
		case SNK_READY:
			sprd_tcpm_set_state(port, VCONN_SWAP_ACCEPT, 0);
			break;
		default:
			sprd_tcpm_queue_message(port, PD_MSG_CTRL_WAIT);
			break;
		}
		break;
	case SPRD_PD_CTRL_GET_SOURCE_CAP_EXT:
	case SPRD_PD_CTRL_GET_STATUS:
	case SPRD_PD_CTRL_FR_SWAP:
	case SPRD_PD_CTRL_GET_PPS_STATUS:
	case SPRD_PD_CTRL_GET_COUNTRY_CODES:
		/* Currently not supported */
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		break;
	default:
		sprd_tcpm_log(port, "Unhandled ctrl message type %#x", type);
		break;
	}
}

static void sprd_tcpm_pd_ext_msg_request(struct sprd_tcpm_port *port,
					 const struct sprd_pd_message *msg)
{
	enum sprd_pd_ext_msg_type type = sprd_pd_header_type_le(msg->header);
	unsigned int data_size = sprd_pd_ext_header_data_size_le(msg->ext_msg.header);

	if (!(msg->ext_msg.header & SPRD_PD_EXT_HDR_CHUNKED)) {
		sprd_tcpm_log(port, "Unchunked extended messages unsupported");
		return;
	}

	if (data_size > SPRD_PD_EXT_MAX_CHUNK_DATA) {
		sprd_tcpm_log(port, "Chunk handling not yet supported");
		return;
	}

	switch (type) {
	case SPRD_PD_EXT_STATUS:
		/*
		 * If PPS related events raised then get PPS status to clear
		 * (see USB PD 3.0 Spec, 6.5.2.4)
		 */
		if (msg->ext_msg.data[USB_PD_EXT_SDB_EVENT_FLAGS] &
		    USB_PD_EXT_SDB_PPS_EVENTS)
			sprd_tcpm_set_state(port, GET_PPS_STATUS_SEND, 0);
		else
			sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case SPRD_PD_EXT_PPS_STATUS:
		/*
		 * For now the PPS status message is used to clear events
		 * and nothing more.
		 */
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case SPRD_PD_EXT_SOURCE_CAP_EXT:
	case SPRD_PD_EXT_GET_BATT_CAP:
	case SPRD_PD_EXT_GET_BATT_STATUS:
	case SPRD_PD_EXT_BATT_CAP:
	case SPRD_PD_EXT_GET_MANUFACTURER_INFO:
	case SPRD_PD_EXT_MANUFACTURER_INFO:
	case SPRD_PD_EXT_SECURITY_REQUEST:
	case SPRD_PD_EXT_SECURITY_RESPONSE:
	case SPRD_PD_EXT_FW_UPDATE_REQUEST:
	case SPRD_PD_EXT_FW_UPDATE_RESPONSE:
	case SPRD_PD_EXT_COUNTRY_INFO:
	case SPRD_PD_EXT_COUNTRY_CODES:
		sprd_tcpm_queue_message(port, PD_MSG_CTRL_NOT_SUPP);
		break;
	default:
		sprd_tcpm_log(port, "Unhandled extended message type %#x", type);
		break;
	}
}

static void sprd_tcpm_pd_rx_handler(struct work_struct *work)
{
	struct sprd_pd_rx_event *event = container_of(work,
						      struct sprd_pd_rx_event, work);
	const struct sprd_pd_message *msg = &event->msg;
	unsigned int cnt = sprd_pd_header_cnt_le(msg->header);
	struct sprd_tcpm_port *port = event->port;

	mutex_lock(&port->lock);

	sprd_tcpm_log(port, "PD RX, header: %#x [%d]", le16_to_cpu(msg->header),
		 port->attached);

	if (port->attached) {
		enum sprd_pd_ctrl_msg_type type = sprd_pd_header_type_le(msg->header);
		unsigned int msgid = sprd_pd_header_msgid_le(msg->header);

		/*
		 * USB PD standard, 6.6.1.2:
		 * "... if MessageID value in a received Message is the
		 * same as the stored value, the receiver shall return a
		 * GoodCRC Message with that MessageID value and drop
		 * the Message (this is a retry of an already received
		 * Message). Note: this shall not apply to the Soft_Reset
		 * Message which always has a MessageID value of zero."
		 */
		if (msgid == port->rx_msgid && type != SPRD_PD_CTRL_SOFT_RESET)
			goto done;
		port->rx_msgid = msgid;

		/*
		 * If both ends believe to be DFP/host, we have a data role
		 * mismatch.
		 */
		if (!!(le16_to_cpu(msg->header) & SPRD_PD_HEADER_DATA_ROLE) ==
		    (port->data_role == TYPEC_HOST)) {
			sprd_tcpm_log(port, "Data role mismatch, initiating error recovery");
			sprd_tcpm_set_state(port, ERROR_RECOVERY, 0);
		} else {
			if (msg->header & SPRD_PD_HEADER_EXT_HDR)
				sprd_tcpm_pd_ext_msg_request(port, msg);
			else if (cnt)
				sprd_tcpm_pd_data_request(port, msg);
			else
				sprd_tcpm_pd_ctrl_request(port, msg);
		}
	}

done:
	mutex_unlock(&port->lock);
	kfree(event);
}

void sprd_tcpm_pd_receive(struct sprd_tcpm_port *port, const struct sprd_pd_message *msg)
{
	struct sprd_pd_rx_event *event;

	event = kzalloc(sizeof(*event), GFP_ATOMIC);
	if (!event)
		return;

	INIT_WORK(&event->work, sprd_tcpm_pd_rx_handler);
	event->port = port;
	memcpy(&event->msg, msg, sizeof(*msg));
	queue_work(port->wq, &event->work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_receive);

static int sprd_tcpm_pd_send_control(struct sprd_tcpm_port *port,
				     enum sprd_pd_ctrl_msg_type type)
{
	struct sprd_pd_message msg;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER_LE(type, port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 0);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

/*
 * Send queued message without affecting state.
 * Return true if state machine should go back to sleep,
 * false otherwise.
 */
static bool sprd_tcpm_send_queued_message(struct sprd_tcpm_port *port)
{
	enum sprd_pd_msg_request queued_message;

	do {
		queued_message = port->queued_message;
		port->queued_message = PD_MSG_NONE;

		switch (queued_message) {
		case PD_MSG_CTRL_WAIT:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_WAIT);
			break;
		case PD_MSG_CTRL_REJECT:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_REJECT);
			break;
		case PD_MSG_CTRL_NOT_SUPP:
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_NOT_SUPP);
			break;
		case PD_MSG_DATA_SINK_CAP:
			sprd_tcpm_pd_send_sink_caps(port);
			break;
		case PD_MSG_DATA_SOURCE_CAP:
			sprd_tcpm_pd_send_source_caps(port);
			break;
		default:
			break;
		}
	} while (port->queued_message != PD_MSG_NONE);

	if (port->delayed_state != INVALID_STATE) {
		if (time_is_after_jiffies(port->delayed_runtime)) {
			mod_delayed_work(port->wq, &port->state_machine,
					 port->delayed_runtime - jiffies);
			return true;
		}
		port->delayed_state = INVALID_STATE;
	}
	return false;
}

static int sprd_tcpm_pd_check_request(struct sprd_tcpm_port *port)
{
	u32 pdo, rdo = port->sink_request;
	unsigned int max, op, pdo_max, index;
	enum sprd_pd_pdo_type type;

	index = sprd_rdo_index(rdo);
	if (!index || index > port->nr_src_pdo)
		return -EINVAL;

	pdo = port->src_pdo[index - 1];
	type = sprd_pdo_type(pdo);
	switch (type) {
	case SPRD_PDO_TYPE_FIXED:
	case SPRD_PDO_TYPE_VAR:
		max = sprd_rdo_max_current(rdo);
		op = sprd_rdo_op_current(rdo);
		pdo_max = sprd_pdo_max_current(pdo);

		if (op > pdo_max)
			return -EINVAL;
		if (max > pdo_max && !(rdo & SPRD_RDO_CAP_MISMATCH))
			return -EINVAL;

		if (type == SPRD_PDO_TYPE_FIXED)
			sprd_tcpm_log(port,
				      "Requested %u mV, %u mA for %u / %u mA",
				      sprd_pdo_fixed_voltage(pdo), pdo_max, op, max);
		else
			sprd_tcpm_log(port,
				      "Requested %u -> %u mV, %u mA for %u / %u mA",
				      sprd_pdo_min_voltage(pdo), sprd_pdo_max_voltage(pdo),
				      pdo_max, op, max);
		break;
	case SPRD_PDO_TYPE_BATT:
		max = sprd_rdo_max_power(rdo);
		op = sprd_rdo_op_power(rdo);
		pdo_max = sprd_pdo_max_power(pdo);

		if (op > pdo_max)
			return -EINVAL;
		if (max > pdo_max && !(rdo & SPRD_RDO_CAP_MISMATCH))
			return -EINVAL;
		sprd_tcpm_log(port,
			      "Requested %u -> %u mV, %u mW for %u / %u mW",
			      sprd_pdo_min_voltage(pdo), sprd_pdo_max_voltage(pdo),
			      pdo_max, op, max);
		break;
	default:
		return -EINVAL;
	}

	port->op_vsafe5v = index == 1;

	return 0;
}

#define min_power(x, y) min(sprd_pdo_max_power(x), sprd_pdo_max_power(y))
#define min_current(x, y) min(sprd_pdo_max_current(x), sprd_pdo_max_current(y))

static int sprd_tcpm_pd_select_pdo(struct sprd_tcpm_port *port, int *sink_pdo, int *src_pdo)
{
	unsigned int i, j, max_src_mv = 0, min_src_mv = 0, max_mw = 0,
		     max_mv = 0, src_mw = 0, src_ma = 0, max_snk_mv = 0,
		     min_snk_mv = 0;
	int ret = -EINVAL;

	port->pps_data.supported = false;
	port->usb_type = POWER_SUPPLY_USB_TYPE_PD;

	/*
	 * Select the source PDO providing the most power which has a
	 * matchig sink cap.
	 */
	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			max_src_mv = sprd_pdo_fixed_voltage(pdo);
			min_src_mv = max_src_mv;
			break;
		case SPRD_PDO_TYPE_BATT:
		case SPRD_PDO_TYPE_VAR:
			max_src_mv = sprd_pdo_max_voltage(pdo);
			min_src_mv = sprd_pdo_min_voltage(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS) {
				port->pps_data.supported = true;
				port->usb_type =
					POWER_SUPPLY_USB_TYPE_PD_PPS;
			}
			continue;
		default:
			sprd_tcpm_log(port, "Invalid source PDO type, ignoring");
			continue;
		}

		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
		case SPRD_PDO_TYPE_VAR:
			src_ma = sprd_pdo_max_current(pdo);
			src_mw = src_ma * min_src_mv / 1000;
			break;
		case SPRD_PDO_TYPE_BATT:
			src_mw = sprd_pdo_max_power(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			continue;
		default:
			sprd_tcpm_log(port, "Invalid source PDO type, ignoring");
			continue;
		}

		for (j = 0; j < port->nr_snk_pdo; j++) {
			pdo = port->snk_pdo[j];

			switch (sprd_pdo_type(pdo)) {
			case SPRD_PDO_TYPE_FIXED:
				max_snk_mv = sprd_pdo_fixed_voltage(pdo);
				min_snk_mv = max_snk_mv;
				break;
			case SPRD_PDO_TYPE_BATT:
			case SPRD_PDO_TYPE_VAR:
				max_snk_mv = sprd_pdo_max_voltage(pdo);
				min_snk_mv = sprd_pdo_min_voltage(pdo);
				break;
			case SPRD_PDO_TYPE_APDO:
				continue;
			default:
				sprd_tcpm_log(port, "Invalid sink PDO type, ignoring");
				continue;
			}

			if (max_src_mv <= max_snk_mv &&
				min_src_mv >= min_snk_mv) {
				/* Prefer higher voltages if available */
				if ((src_mw == max_mw && min_src_mv > max_mv) ||
							src_mw > max_mw) {
					*src_pdo = i;
					*sink_pdo = j;
					max_mw = src_mw;
					max_mv = min_src_mv;
					ret = 0;
				}
			}
		}
	}

	return ret;
}

#define sprd_min_pps_apdo_current(x, y)	\
	min(sprd_pdo_pps_apdo_max_current(x), sprd_pdo_pps_apdo_max_current(y))

static unsigned int sprd_tcpm_pd_select_pps_apdo(struct sprd_tcpm_port *port)
{
	unsigned int i, j, max_mw = 0, max_mv = 0;
	unsigned int min_src_mv, max_src_mv, src_ma, src_mw;
	unsigned int min_snk_mv, max_snk_mv;
	unsigned int max_op_mv;
	u32 pdo, src, snk;
	unsigned int src_pdo = 0, snk_pdo = 0;

	/*
	 * Select the source PPS APDO providing the most power while staying
	 * within the board's limits. We skip the first PDO as this is always
	 * 5V 3A.
	 */
	for (i = 1; i < port->nr_source_caps; ++i) {
		pdo = port->source_caps[i];

		switch (sprd_pdo_type(pdo)) {
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
				sprd_tcpm_log(port, "Not PPS APDO (source), ignoring");
				continue;
			}

			min_src_mv = sprd_pdo_pps_apdo_min_voltage(pdo);
			max_src_mv = sprd_pdo_pps_apdo_max_voltage(pdo);
			src_ma = sprd_pdo_pps_apdo_max_current(pdo);
			src_mw = (src_ma * max_src_mv) / 1000;

			/*
			 * Now search through the sink PDOs to find a matching
			 * PPS APDO. Again skip the first sink PDO as this will
			 * always be 5V 3A.
			 */
			for (j = 1; j < port->nr_snk_pdo; j++) {
				pdo = port->snk_pdo[j];

				switch (sprd_pdo_type(pdo)) {
				case SPRD_PDO_TYPE_APDO:
					if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
						sprd_tcpm_log(port,
							      "Not PPS APDO (sink), ignoring");
						continue;
					}

					min_snk_mv =
						sprd_pdo_pps_apdo_min_voltage(pdo);
					max_snk_mv =
						sprd_pdo_pps_apdo_max_voltage(pdo);
					break;
				default:
					sprd_tcpm_log(port, "Not APDO type (sink), ignoring");
					continue;
				}

				if (min_src_mv <= max_snk_mv &&
				    max_src_mv >= min_snk_mv) {
					max_op_mv = min(max_src_mv, max_snk_mv);
					src_mw = (max_op_mv * src_ma) / 1000;
					/* Prefer higher voltages if available */
					if ((src_mw == max_mw &&
					     max_op_mv > max_mv) ||
					    src_mw > max_mw) {
						src_pdo = i;
						snk_pdo = j;
						max_mw = src_mw;
						max_mv = max_op_mv;
					}
				}
			}

			break;
		default:
			sprd_tcpm_log(port, "Not APDO type (source), ignoring");
			continue;
		}
	}

	if (src_pdo) {
		src = port->source_caps[src_pdo];
		snk = port->snk_pdo[snk_pdo];

		port->pps_data.req_min_volt = max(sprd_pdo_pps_apdo_min_voltage(src),
						  sprd_pdo_pps_apdo_min_voltage(snk));
		port->pps_data.req_max_volt = min(sprd_pdo_pps_apdo_max_voltage(src),
						  sprd_pdo_pps_apdo_max_voltage(snk));
		port->pps_data.req_max_curr = sprd_min_pps_apdo_current(src, snk);
		port->pps_data.req_out_volt = min(port->pps_data.req_max_volt,
						  max(port->pps_data.req_min_volt,
						      port->pps_data.req_out_volt));
		port->pps_data.req_op_curr = min(port->pps_data.req_max_curr,
						 port->pps_data.req_op_curr);
	}

	return src_pdo;
}

static int sprd_tcpm_pd_build_request(struct sprd_tcpm_port *port, u32 *rdo)
{
	unsigned int mv, ma, mw, flags;
	unsigned int max_ma, max_mw;
	enum sprd_pd_pdo_type type;
	u32 pdo, matching_snk_pdo;
	int src_pdo_index = 0;
	int snk_pdo_index = 0;
	int ret;

	ret = sprd_tcpm_pd_select_pdo(port, &snk_pdo_index, &src_pdo_index);
	if (ret < 0)
		return ret;

	pdo = port->source_caps[src_pdo_index];
	matching_snk_pdo = port->snk_pdo[snk_pdo_index];
	type = sprd_pdo_type(pdo);

	switch (type) {
	case SPRD_PDO_TYPE_FIXED:
		mv = sprd_pdo_fixed_voltage(pdo);
		break;
	case SPRD_PDO_TYPE_BATT:
	case SPRD_PDO_TYPE_VAR:
		mv = sprd_pdo_min_voltage(pdo);
		break;
	default:
		sprd_tcpm_log(port, "Invalid PDO selected!");
		return -EINVAL;
	}

	/* Select maximum available current within the sink pdo's limit */
	if (type == SPRD_PDO_TYPE_BATT) {
		mw = min_power(pdo, matching_snk_pdo);
		ma = 1000 * mw / mv;
	} else {
		ma = min_current(pdo, matching_snk_pdo);
		mw = ma * mv / 1000;
	}

	flags = SPRD_RDO_USB_COMM | SPRD_RDO_NO_SUSPEND;

	/* Set mismatch bit if offered power is less than operating power */
	max_ma = ma;
	max_mw = mw;
	if (mw < port->operating_snk_mw) {
		flags |= SPRD_RDO_CAP_MISMATCH;
		if (type == SPRD_PDO_TYPE_BATT &&
		    (sprd_pdo_max_power(matching_snk_pdo) > sprd_pdo_max_power(pdo)))
			max_mw = sprd_pdo_max_power(matching_snk_pdo);
		else if (sprd_pdo_max_current(matching_snk_pdo) >
			 sprd_pdo_max_current(pdo))
			max_ma = sprd_pdo_max_current(matching_snk_pdo);
	}

	sprd_tcpm_log(port, "cc=%d cc1=%d cc2=%d vbus=%d vconn=%s polarity=%d",
		      port->cc_req, port->cc1, port->cc2, port->vbus_source,
		      port->vconn_role == TYPEC_SOURCE ? "source" : "sink",
		      port->polarity);

	if (type == SPRD_PDO_TYPE_BATT) {
		*rdo = SPRD_RDO_BATT(src_pdo_index + 1, mw, max_mw, flags);

		sprd_tcpm_log(port, "Requesting PDO %d: %u mV, %u mW%s",
			      src_pdo_index, mv, mw,
			      flags & SPRD_RDO_CAP_MISMATCH ? " [mismatch]" : "");
	} else {
		*rdo = SPRD_RDO_FIXED(src_pdo_index + 1, ma, max_ma, flags);

		sprd_tcpm_log(port, "Requesting PDO %d: %u mV, %u mA%s",
			      src_pdo_index, mv, ma,
			      flags & SPRD_RDO_CAP_MISMATCH ? " [mismatch]" : "");
	}

	port->req_current_limit = ma;
	port->req_supply_voltage = mv;

	return 0;
}

static int sprd_tcpm_pd_send_request(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int ret;
	u32 rdo;

	ret = sprd_tcpm_pd_build_request(port, &rdo);
	if (ret < 0)
		return ret;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER_LE(SPRD_PD_DATA_REQUEST,
				       port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 1);
	msg.payload[0] = cpu_to_le32(rdo);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_pd_build_pps_request(struct sprd_tcpm_port *port, u32 *rdo)
{
	unsigned int out_mv, op_ma, op_mw, max_mv, max_ma, flags;
	enum sprd_pd_pdo_type type;
	unsigned int src_pdo_index;
	u32 pdo;

	src_pdo_index = sprd_tcpm_pd_select_pps_apdo(port);
	if (!src_pdo_index)
		return -EOPNOTSUPP;

	pdo = port->source_caps[src_pdo_index];
	type = sprd_pdo_type(pdo);

	switch (type) {
	case SPRD_PDO_TYPE_APDO:
		if (sprd_pdo_apdo_type(pdo) != SPRD_APDO_TYPE_PPS) {
			sprd_tcpm_log(port, "Invalid APDO selected!");
			return -EINVAL;
		}
		max_mv = port->pps_data.req_max_volt;
		max_ma = port->pps_data.req_max_curr;
		out_mv = port->pps_data.req_out_volt;
		op_ma = port->pps_data.req_op_curr;
		break;
	default:
		sprd_tcpm_log(port, "Invalid PDO selected!");
		return -EINVAL;
	}

	flags = SPRD_RDO_USB_COMM | SPRD_RDO_NO_SUSPEND;

	op_mw = (op_ma * out_mv) / 1000;
	if (op_mw < port->operating_snk_mw) {
		/*
		 * Try raising current to meet power needs. If that's not enough
		 * then try upping the voltage. If that's still not enough
		 * then we've obviously chosen a PPS APDO which really isn't
		 * suitable so abandon ship.
		 */
		op_ma = (port->operating_snk_mw * 1000) / out_mv;
		if ((port->operating_snk_mw * 1000) % out_mv)
			++op_ma;
		op_ma += SPRD_RDO_PROG_CURR_MA_STEP - (op_ma % SPRD_RDO_PROG_CURR_MA_STEP);

		if (op_ma > max_ma) {
			op_ma = max_ma;
			out_mv = (port->operating_snk_mw * 1000) / op_ma;
			if ((port->operating_snk_mw * 1000) % op_ma)
				++out_mv;
			out_mv += SPRD_RDO_PROG_VOLT_MV_STEP -
				  (out_mv % SPRD_RDO_PROG_VOLT_MV_STEP);

			if (out_mv > max_mv) {
				sprd_tcpm_log(port, "Invalid PPS APDO selected!");
				return -EINVAL;
			}
		}
	}

	sprd_tcpm_log(port, "cc=%d cc1=%d cc2=%d vbus=%d vconn=%s polarity=%d",
		      port->cc_req, port->cc1, port->cc2, port->vbus_source,
		      port->vconn_role == TYPEC_SOURCE ? "source" : "sink",
		      port->polarity);

	*rdo = SPRD_RDO_PROG(src_pdo_index + 1, out_mv, op_ma, flags);

	sprd_tcpm_log(port, "Requesting APDO %d: %u mV, %u mA",
		      src_pdo_index, out_mv, op_ma);

	port->pps_data.req_op_curr = op_ma;
	port->pps_data.req_out_volt = out_mv;

	return 0;
}

static int sprd_tcpm_pd_send_pps_request(struct sprd_tcpm_port *port)
{
	struct sprd_pd_message msg;
	int ret;
	u32 rdo;

	ret = sprd_tcpm_pd_build_pps_request(port, &rdo);
	if (ret < 0)
		return ret;

	memset(&msg, 0, sizeof(msg));
	msg.header = SPRD_PD_HEADER_LE(SPRD_PD_DATA_REQUEST,
				       port->pwr_role,
				       port->data_role,
				       port->negotiated_rev,
				       port->message_id, 1);
	msg.payload[0] = cpu_to_le32(rdo);

	return sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_SOP, &msg);
}

static int sprd_tcpm_set_vbus(struct sprd_tcpm_port *port, bool enable)
{
	int ret;

	if (enable && port->vbus_charge)
		return -EINVAL;

	sprd_tcpm_log(port, "vbus:=%d charge=%d", enable, port->vbus_charge);

	ret = port->tcpc->set_vbus(port->tcpc, enable, port->vbus_charge);
	if (ret < 0)
		return ret;

	port->vbus_source = enable;
	return 0;
}

static int sprd_tcpm_set_charge(struct sprd_tcpm_port *port, bool charge)
{
	int ret;

	if (charge && port->vbus_source)
		return -EINVAL;

	if (charge != port->vbus_charge) {
		sprd_tcpm_log(port, "vbus=%d charge:=%d", port->vbus_source, charge);
		ret = port->tcpc->set_vbus(port->tcpc, port->vbus_source,
					   charge);
		if (ret < 0)
			return ret;
	}
	port->vbus_charge = charge;
	return 0;
}

static bool sprd_tcpm_start_toggling(struct sprd_tcpm_port *port, enum sprd_typec_cc_status cc)
{
	int ret;

	if (!port->tcpc->start_toggling)
		return false;

	sprd_tcpm_log_force(port, "Start toggling");
	ret = port->tcpc->start_toggling(port->tcpc, port->port_type, cc);
	return ret == 0;
}

static void sprd_tcpm_set_cc(struct sprd_tcpm_port *port, enum sprd_typec_cc_status cc)
{
	sprd_tcpm_log(port, "cc:=%d", cc);
	port->cc_req = cc;
	port->tcpc->set_cc(port->tcpc, cc);
}

static int sprd_tcpm_init_vbus(struct sprd_tcpm_port *port)
{
	int ret;

	ret = port->tcpc->set_vbus(port->tcpc, false, false);
	port->vbus_source = false;
	port->vbus_charge = false;
	return ret;
}

static int sprd_tcpm_init_vconn(struct sprd_tcpm_port *port)
{
	int ret;

	ret = port->tcpc->set_vconn(port->tcpc, false);
	port->vconn_role = TYPEC_SINK;
	return ret;
}

static void sprd_tcpm_typec_connect(struct sprd_tcpm_port *port)
{
	if (!port->connected) {
		/* Make sure we don't report stale identity information */
		memset(&port->partner_ident, 0, sizeof(port->partner_ident));
		port->partner_desc.usb_pd = port->pd_capable;
		if (sprd_tcpm_port_is_debug(port))
			port->partner_desc.accessory = TYPEC_ACCESSORY_DEBUG;
		else if (sprd_tcpm_port_is_audio(port))
			port->partner_desc.accessory = TYPEC_ACCESSORY_AUDIO;
		else
			port->partner_desc.accessory = TYPEC_ACCESSORY_NONE;
		port->partner = typec_register_partner(port->typec_port,
						       &port->partner_desc);
		port->connected = true;
	}
}

static int sprd_tcpm_src_attach(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_polarity polarity =
					port->cc2 == SPRD_TYPEC_CC_RD ? SPRD_TYPEC_POLARITY_CC2
							 : SPRD_TYPEC_POLARITY_CC1;
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_polarity(port, polarity);
	if (ret < 0)
		return ret;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SOURCE, TYPEC_HOST);
	if (ret < 0)
		return ret;

	ret = port->tcpc->set_pd_rx(port->tcpc, true);
	if (ret < 0)
		goto out_disable_mux;

	/*
	 * USB Type-C specification, version 1.2,
	 * chapter 4.5.2.2.8.1 (Attached.SRC Requirements)
	 * Enable VCONN only if the non-RD port is set to RA.
	 */
	if ((polarity == SPRD_TYPEC_POLARITY_CC1 && port->cc2 == SPRD_TYPEC_CC_RA) ||
	    (polarity == SPRD_TYPEC_POLARITY_CC2 && port->cc1 == SPRD_TYPEC_CC_RA)) {
		ret = sprd_tcpm_set_vconn(port, true);
		if (ret < 0)
			goto out_disable_pd;
	}

	ret = sprd_tcpm_set_vbus(port, true);
	if (ret < 0)
		goto out_disable_vconn;

	port->pd_capable = false;

	port->partner = NULL;

	port->attached = true;
	port->send_discover = true;

	return 0;

out_disable_vconn:
	sprd_tcpm_set_vconn(port, false);
out_disable_pd:
	port->tcpc->set_pd_rx(port->tcpc, false);
out_disable_mux:
	sprd_tcpm_mux_set(port, TYPEC_STATE_SAFE, USB_ROLE_NONE,
		     TYPEC_ORIENTATION_NONE);
	return ret;
}

static void sprd_tcpm_typec_disconnect(struct sprd_tcpm_port *port)
{
	if (port->connected) {
		typec_unregister_partner(port->partner);
		port->partner = NULL;
		port->connected = false;
	}
}

static void sprd_tcpm_unregister_altmodes(struct sprd_tcpm_port *port)
{
	struct sprd_pd_mode_data *modep = &port->mode_data;
	int i;

	for (i = 0; i < modep->altmodes; i++) {
		typec_unregister_altmode(port->partner_altmode[i]);
		port->partner_altmode[i] = NULL;
	}

	memset(modep, 0, sizeof(*modep));
}

static void sprd_tcpm_reset_port(struct sprd_tcpm_port *port)
{
	sprd_tcpm_unregister_altmodes(port);
	sprd_tcpm_typec_disconnect(port);
	port->attached = false;
	port->pd_capable = false;
	port->pps_data.supported = false;

	/*
	 * First Rx ID should be 0; set this to a sentinel of -1 so that
	 * we can check sprd_tcpm_pd_rx_handler() if we had seen it before.
	 */
	port->rx_msgid = -1;

	port->tcpc->set_pd_rx(port->tcpc, false);
	sprd_tcpm_init_vbus(port);	/* also disables charging */
	sprd_tcpm_init_vconn(port);
	sprd_tcpm_set_current_limit(port, 0, 0);
	sprd_tcpm_set_polarity(port, SPRD_TYPEC_POLARITY_CC1);
	sprd_tcpm_mux_set(port, TYPEC_STATE_SAFE, USB_ROLE_NONE,
		     TYPEC_ORIENTATION_NONE);
	sprd_tcpm_set_attached_state(port, false);
	port->try_src_count = 0;
	port->try_snk_count = 0;
	port->usb_type = POWER_SUPPLY_USB_TYPE_C;
	port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;

	power_supply_changed(port->psy);
}

static void sprd_tcpm_detach(struct sprd_tcpm_port *port)
{
	if (sprd_tcpm_port_is_disconnected(port))
		port->hard_reset_count = 0;

	if (!port->attached)
		return;

	sprd_tcpm_reset_port(port);
}

static void sprd_tcpm_src_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static int sprd_tcpm_snk_attach(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_polarity(port, port->cc2 != SPRD_TYPEC_CC_OPEN ?
				     SPRD_TYPEC_POLARITY_CC2 : SPRD_TYPEC_POLARITY_CC1);
	if (ret < 0)
		return ret;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SINK, TYPEC_DEVICE);
	if (ret < 0)
		return ret;

	port->pd_capable = false;

	port->partner = NULL;

	port->attached = true;
	port->send_discover = true;

	return 0;
}

static void sprd_tcpm_snk_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static int sprd_tcpm_acc_attach(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->attached)
		return 0;

	ret = sprd_tcpm_set_roles(port, true, TYPEC_SOURCE, TYPEC_HOST);
	if (ret < 0)
		return ret;

	port->partner = NULL;

	sprd_tcpm_typec_connect(port);

	port->attached = true;

	return 0;
}

static void sprd_tcpm_acc_detach(struct sprd_tcpm_port *port)
{
	sprd_tcpm_detach(port);
}

static inline enum sprd_tcpm_state sprd_hard_reset_state(struct sprd_tcpm_port *port)
{
	if (port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT)
		return HARD_RESET_SEND;
	if (port->pd_capable)
		return ERROR_RECOVERY;
	if (port->pwr_role == TYPEC_SOURCE)
		return SRC_UNATTACHED;
	if (port->state == SNK_WAIT_CAPABILITIES)
		return SNK_READY;
	return SNK_UNATTACHED;
}

static inline enum sprd_tcpm_state sprd_unattached_state(struct sprd_tcpm_port *port)
{
	if (port->port_type == TYPEC_PORT_DRP) {
		if (port->pwr_role == TYPEC_SOURCE)
			return SRC_UNATTACHED;
		else
			return SNK_UNATTACHED;
	} else if (port->port_type == TYPEC_PORT_SRC) {
		return SRC_UNATTACHED;
	}

	return SNK_UNATTACHED;
}

static void sprd_tcpm_check_send_discover(struct sprd_tcpm_port *port)
{
	if (port->data_role == TYPEC_HOST && port->send_discover &&
	    port->pd_capable) {
		sprd_tcpm_send_vdm(port, USB_SID_PD, CMD_DISCOVER_IDENT, NULL, 0);
		port->send_discover = false;
	}
}

static void sprd_tcpm_swap_complete(struct sprd_tcpm_port *port, int result)
{
	if (port->swap_pending) {
		port->swap_status = result;
		port->swap_pending = false;
		port->non_pd_role_swap = false;
		complete(&port->swap_complete);
	}
}

static enum typec_pwr_opmode sprd_tcpm_get_pwr_opmode(enum sprd_typec_cc_status cc)
{
	switch (cc) {
	case SPRD_TYPEC_CC_RP_1_5:
		return TYPEC_PWR_MODE_1_5A;
	case SPRD_TYPEC_CC_RP_3_0:
		return TYPEC_PWR_MODE_3_0A;
	case SPRD_TYPEC_CC_RP_DEF:
	default:
		return TYPEC_PWR_MODE_USB;
	}
}

static void sprd_run_state_machine(struct sprd_tcpm_port *port)
{
	int ret;
	enum typec_pwr_opmode opmode;
	unsigned int msecs;

	port->enter_state = port->state;
	switch (port->state) {
	case TOGGLING:
		break;
	/* SRC states */
	case SRC_UNATTACHED:
		if (!port->non_pd_role_swap)
			sprd_tcpm_swap_complete(port, -ENOTCONN);
		sprd_tcpm_src_detach(port);
		if (sprd_tcpm_start_toggling(port, sprd_tcpm_rp_cc(port))) {
			sprd_tcpm_set_state(port, TOGGLING, 0);
			break;
		}
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		if (port->port_type == TYPEC_PORT_DRP)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_DRP_SNK);
		break;
	case SRC_ATTACH_WAIT:
		if (sprd_tcpm_port_is_debug(port))
			sprd_tcpm_set_state(port, DEBUG_ACC_ATTACHED,
					    SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_audio(port))
			sprd_tcpm_set_state(port, AUDIO_ACC_ATTACHED,
					    SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_ATTACHED,
					    SPRD_PD_T_CC_DEBOUNCE);
		break;

	case SNK_TRY:
		port->try_snk_count++;
		/*
		 * Requirements:
		 * - Do not drive vconn or vbus
		 * - Terminate CC pins (both) to Rd
		 * Action:
		 * - Wait for tDRPTry (SPRD_PD_T_DRP_TRY).
		 *   Until then, ignore any state changes.
		 */
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		sprd_tcpm_set_state(port, SNK_TRY_WAIT, SPRD_PD_T_DRP_TRY);
		break;
	case SNK_TRY_WAIT:
		if (sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_TRY_WAIT_DEBOUNCE, 0);
		} else {
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
			port->max_wait = 0;
		}
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS,
				    SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS:
		if (port->vbus_present && sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
		} else {
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
			port->max_wait = 0;
		}
		break;
	case SRC_TRYWAIT:
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		if (port->max_wait == 0) {
			port->max_wait = jiffies +
					 msecs_to_jiffies(SPRD_PD_T_DRP_TRY);
			sprd_tcpm_set_state(port, SRC_TRYWAIT_UNATTACHED, SPRD_PD_T_DRP_TRY);
		} else {
			if (time_is_after_jiffies(port->max_wait))
				sprd_tcpm_set_state(port, SRC_TRYWAIT_UNATTACHED,
					       jiffies_to_msecs(port->max_wait -
								jiffies));
			else
				sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		}
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_ATTACHED, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SRC_TRYWAIT_UNATTACHED:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;

	case SRC_ATTACHED:
		ret = sprd_tcpm_src_attach(port);
		sprd_tcpm_set_state(port, SRC_UNATTACHED,
				    ret < 0 ? 0 : SPRD_PD_T_PS_SOURCE_ON);
		break;
	case SRC_STARTUP:
		opmode =  sprd_tcpm_get_pwr_opmode(sprd_tcpm_rp_cc(port));
		typec_set_pwr_opmode(port->typec_port, opmode);
		port->pwr_opmode = TYPEC_PWR_MODE_USB;
		port->caps_count = 0;
		port->negotiated_rev = SPRD_PD_MAX_REV;
		port->message_id = 0;
		port->rx_msgid = -1;
		port->explicit_contract = false;
		sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		break;
	case SRC_SEND_CAPABILITIES:
		port->caps_count++;
		if (port->caps_count > SPRD_PD_N_CAPS_COUNT) {
			sprd_tcpm_set_state(port, SRC_READY, 0);
			break;
		}
		ret = sprd_tcpm_pd_send_source_caps(port);
		if (ret < 0) {
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES,
					    SPRD_PD_T_SEND_SOURCE_CAP);
		} else {
			/*
			 * Per standard, we should clear the reset counter here.
			 * However, that can result in state machine hang-ups.
			 * Reset it only in READY state to improve stability.
			 */
			/* port->hard_reset_count = 0; */
			port->caps_count = 0;
			port->pd_capable = true;
			sprd_tcpm_set_state_cond(port, SRC_SEND_CAPABILITIES_TIMEOUT,
						 SPRD_PD_T_SEND_SOURCE_CAP);
		}
		break;
	case SRC_SEND_CAPABILITIES_TIMEOUT:
		/*
		 * Error recovery for a PD_DATA_SOURCE_CAP reply timeout.
		 *
		 * PD 2.0 sinks are supposed to accept src-capabilities with a
		 * 3.0 header and simply ignore any src PDOs which the sink does
		 * not understand such as PPS but some 2.0 sinks instead ignore
		 * the entire PD_DATA_SOURCE_CAP message, causing contract
		 * negotiation to fail.
		 *
		 * After SPRD_PD_N_HARD_RESET_COUNT hard-reset attempts, we try
		 * sending src-capabilities with a lower PD revision to
		 * make these broken sinks work.
		 */
		if (port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT) {
			sprd_tcpm_set_state(port, HARD_RESET_SEND, 0);
		} else if (port->negotiated_rev > SPRD_PD_REV20) {
			port->negotiated_rev--;
			port->hard_reset_count = 0;
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		} else {
			sprd_tcpm_set_state(port, sprd_hard_reset_state(port), 0);
		}
		break;
	case SRC_NEGOTIATE_CAPABILITIES:
		ret = sprd_tcpm_pd_check_request(port);
		if (ret < 0) {
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_REJECT);
			if (!port->explicit_contract) {
				sprd_tcpm_set_state(port,
					       SRC_WAIT_NEW_CAPABILITIES, 0);
			} else {
				sprd_tcpm_set_state(port, SRC_READY, 0);
			}
		} else {
			sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
			sprd_tcpm_set_state(port, SRC_TRANSITION_SUPPLY, SPRD_PD_T_SRC_TRANSITION);
		}
		break;
	case SRC_TRANSITION_SUPPLY:
		/* XXX: regulator_set_voltage(vbus, ...) */
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		port->explicit_contract = true;
		typec_set_pwr_opmode(port->typec_port, TYPEC_PWR_MODE_PD);
		port->pwr_opmode = TYPEC_PWR_MODE_PD;
		sprd_tcpm_set_state_cond(port, SRC_READY, 0);
		break;
	case SRC_READY:
#if 1
		port->hard_reset_count = 0;
#endif
		port->try_src_count = 0;

		sprd_tcpm_swap_complete(port, 0);
		sprd_tcpm_typec_connect(port);

		sprd_tcpm_check_send_discover(port);
		/*
		 * 6.3.5
		 * Sending ping messages is not necessary if
		 * - the source operates at vSafe5V
		 * or
		 * - The system is not operating in PD mode
		 * or
		 * - Both partners are connected using a Type-C connector
		 *
		 * There is no actual need to send PD messages since the local
		 * port type-c and the spec does not clearly say whether PD is
		 * possible when type-c is connected to Type-A/B
		 */
		break;
	case SRC_WAIT_NEW_CAPABILITIES:
		/* Nothing to do... */
		break;

	/* SNK states */
	case SNK_UNATTACHED:
		if (!port->non_pd_role_swap)
			sprd_tcpm_swap_complete(port, -ENOTCONN);
		sprd_tcpm_pps_complete(port, -ENOTCONN);
		sprd_tcpm_snk_detach(port);
		if (sprd_tcpm_start_toggling(port, SPRD_TYPEC_CC_RD)) {
			sprd_tcpm_set_state(port, TOGGLING, 0);
			break;
		}
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		if (port->port_type == TYPEC_PORT_DRP)
			sprd_tcpm_set_state(port, SRC_UNATTACHED, SPRD_PD_T_DRP_SRC);
		break;
	case SNK_ATTACH_WAIT:
		if ((port->cc1 == SPRD_TYPEC_CC_OPEN &&
		     port->cc2 != SPRD_TYPEC_CC_OPEN) ||
		    (port->cc1 != SPRD_TYPEC_CC_OPEN &&
		     port->cc2 == SPRD_TYPEC_CC_OPEN))
			sprd_tcpm_set_state(port, SNK_DEBOUNCED, SPRD_PD_T_CC_DEBOUNCE);
		else if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_DEBOUNCED:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		else if (port->vbus_present)
			sprd_tcpm_set_state(port,
					    sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED,
					    0);
		else
			/* Wait for VBUS, but not forever */
			sprd_tcpm_set_state(port, PORT_RESET, SPRD_PD_T_PS_SOURCE_ON);
		break;

	case SRC_TRY:
		port->try_src_count++;
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		port->max_wait = 0;
		sprd_tcpm_set_state(port, SRC_TRY_WAIT, 0);
		break;
	case SRC_TRY_WAIT:
		if (port->max_wait == 0) {
			port->max_wait = jiffies +
					 msecs_to_jiffies(SPRD_PD_T_DRP_TRY);
			msecs = SPRD_PD_T_DRP_TRY;
		} else {
			if (time_is_after_jiffies(port->max_wait))
				msecs = jiffies_to_msecs(port->max_wait -
							 jiffies);
			else
				msecs = 0;
		}
		sprd_tcpm_set_state(port, SNK_TRYWAIT, msecs);
		break;
	case SRC_TRY_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_ATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_TRYWAIT:
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		sprd_tcpm_set_state(port, SNK_TRYWAIT_VBUS, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SNK_TRYWAIT_VBUS:
		/*
		 * TCPM stays in this state indefinitely until VBUS
		 * is detected as long as Rp is not detected for
		 * more than a time period of tPDDebounce.
		 */
		if (port->vbus_present && sprd_tcpm_port_is_sink(port)) {
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
			break;
		}
		if (!sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, SPRD_PD_T_PD_DEBOUNCE);
		break;
	case SNK_ATTACHED:
		ret = sprd_tcpm_snk_attach(port);
		if (ret < 0)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		else
			sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;
	case SNK_STARTUP:
		opmode =  sprd_tcpm_get_pwr_opmode(port->polarity ? port->cc2 : port->cc1);
		typec_set_pwr_opmode(port->typec_port, opmode);
		port->pwr_opmode = TYPEC_PWR_MODE_USB;
		port->negotiated_rev = SPRD_PD_MAX_REV;
		port->message_id = 0;
		port->rx_msgid = -1;
		port->explicit_contract = false;
		sprd_tcpm_set_state(port, SNK_DISCOVERY, 0);
		break;
	case SNK_DISCOVERY:
		if (port->vbus_present) {
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
			sprd_tcpm_set_charge(port, true);
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
			break;
		}
		/*
		 * For DRP, timeouts differ. Also, handling is supposed to be
		 * different and much more complex (dead battery detection;
		 * see USB power delivery specification, section 8.3.3.6.1.5.1).
		 */
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port),
				    port->port_type == TYPEC_PORT_DRP ?
				    SPRD_PD_T_DB_DETECT : SPRD_PD_T_NO_RESPONSE);
		break;
	case SNK_DISCOVERY_DEBOUNCE:
		sprd_tcpm_set_state(port, SNK_DISCOVERY_DEBOUNCE_DONE, SPRD_PD_T_CC_DEBOUNCE);
		break;
	case SNK_DISCOVERY_DEBOUNCE_DONE:
		if (!sprd_tcpm_port_is_disconnected(port) &&
		    sprd_tcpm_port_is_sink(port) &&
		    time_is_after_jiffies(port->delayed_runtime)) {
			sprd_tcpm_set_state(port, SNK_DISCOVERY,
					    jiffies_to_msecs(port->delayed_runtime - jiffies));
			break;
		}
		sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		break;
	case SNK_WAIT_CAPABILITIES:
		ret = port->tcpc->set_pd_rx(port->tcpc, true);
		if (ret < 0) {
			sprd_tcpm_set_state(port, SNK_READY, 0);
			break;
		}
		/*
		 * If VBUS has never been low, and we time out waiting
		 * for source cap, try a soft reset first, in case we
		 * were already in a stable contract before this boot.
		 * Do this only once.
		 */
		if (port->vbus_never_low) {
			port->vbus_never_low = false;
			sprd_tcpm_set_state(port, SOFT_RESET_SEND, SPRD_PD_T_SINK_WAIT_CAP);
		} else {
			sprd_tcpm_set_state(port,
					    sprd_hard_reset_state(port),
					    SPRD_PD_T_SINK_WAIT_CAP);
		}
		break;
	case SNK_NEGOTIATE_CAPABILITIES:
		port->pd_capable = true;
		port->hard_reset_count = 0;
		ret = sprd_tcpm_pd_send_request(port);
		if (ret < 0) {
			/* Let the Source send capabilities again. */
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
		} else {
			sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port),
						 SPRD_PD_T_SENDER_RESPONSE);
		}
		break;
	case SNK_NEGOTIATE_PPS_CAPABILITIES:
		ret = sprd_tcpm_pd_send_pps_request(port);
		if (ret < 0) {
			port->pps_status = ret;
			/*
			 * If this was called due to updates to sink
			 * capabilities, and pps is no longer valid, we should
			 * safely fall back to a standard PDO.
			 */
			if (port->update_sink_caps)
				sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
			else
				sprd_tcpm_set_state(port, SNK_READY, 0);
		} else {
			sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port),
						 SPRD_PD_T_SENDER_RESPONSE);
		}
		break;
	case SNK_TRANSITION_SINK:
	case SNK_TRANSITION_SINK_VBUS:
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port), SPRD_PD_T_PS_TRANSITION);
		break;
	case SNK_READY:
		port->try_snk_count = 0;
		port->update_sink_caps = false;
		if (port->explicit_contract) {
			typec_set_pwr_opmode(port->typec_port, TYPEC_PWR_MODE_PD);
			port->pwr_opmode = TYPEC_PWR_MODE_PD;
		}

		sprd_tcpm_swap_complete(port, 0);
		sprd_tcpm_typec_connect(port);
		sprd_tcpm_check_send_discover(port);
		sprd_tcpm_pps_complete(port, port->pps_status);

		/*
		 * When avoiding PPS charging, the upper layer is notified
		 * repeatedly if the USB type is changed.
		*/
		if (port->usb_type != port->last_usb_type) {
			port->last_usb_type = port->usb_type;
			power_supply_changed(port->psy);
		}
		break;

	/* Accessory states */
	case ACC_UNATTACHED:
		sprd_tcpm_acc_detach(port);
		sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		break;
	case DEBUG_ACC_ATTACHED:
	case AUDIO_ACC_ATTACHED:
		ret = sprd_tcpm_acc_attach(port);
		if (ret < 0)
			sprd_tcpm_set_state(port, ACC_UNATTACHED, 0);
		break;
	case AUDIO_ACC_DEBOUNCE:
		sprd_tcpm_set_state(port, ACC_UNATTACHED, SPRD_PD_T_CC_DEBOUNCE);
		break;

	/* Hard_Reset states */
	case HARD_RESET_SEND:
		sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_HARD_RESET, NULL);
		sprd_tcpm_set_state(port, HARD_RESET_START, 0);
		break;
	case HARD_RESET_START:
		port->hard_reset_count++;
		port->tcpc->set_pd_rx(port->tcpc, false);
		sprd_tcpm_unregister_altmodes(port);
		port->send_discover = true;
		port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_OFF,
					    SPRD_PD_T_PS_HARD_RESET);
		else
			sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_OFF, 0);
		break;
	case SRC_HARD_RESET_VBUS_OFF:
		sprd_tcpm_set_vconn(port, true);
		sprd_tcpm_set_vbus(port, false);
		sprd_tcpm_set_roles(port, port->self_powered, TYPEC_SOURCE, TYPEC_HOST);
		sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_ON, SPRD_PD_T_SRC_RECOVER);
		break;
	case SRC_HARD_RESET_VBUS_ON:
		sprd_tcpm_set_vbus(port, true);
		port->tcpc->set_pd_rx(port->tcpc, true);
		sprd_tcpm_set_attached_state(port, true);
		sprd_tcpm_set_state(port, SRC_UNATTACHED, SPRD_PD_T_PS_SOURCE_ON);
		break;
	case SNK_HARD_RESET_SINK_OFF:
		memset(&port->pps_data, 0, sizeof(port->pps_data));
		sprd_tcpm_set_vconn(port, false);
		if (port->pd_capable)
			sprd_tcpm_set_charge(port, false);
		sprd_tcpm_set_roles(port, port->self_powered, TYPEC_SINK, TYPEC_DEVICE);
		/*
		 * VBUS may or may not toggle, depending on the adapter.
		 * If it doesn't toggle, transition to SNK_HARD_RESET_SINK_ON
		 * directly after timeout.
		 */
		sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_ON, SPRD_PD_T_SAFE_0V +
				    SPRD_PD_T_SRC_RECOVER_MAX + SPRD_PD_T_SRC_TURN_ON);
		break;
	case SNK_HARD_RESET_WAIT_VBUS:
		/* Assume we're disconnected if VBUS doesn't come back. */
		sprd_tcpm_set_state(port, SNK_UNATTACHED,
				    SPRD_PD_T_SRC_RECOVER_MAX + SPRD_PD_T_SRC_TURN_ON);
		break;
	case SNK_HARD_RESET_SINK_ON:
		/* Note: There is no guarantee that VBUS is on in this state */
		/*
		 * XXX:
		 * The specification suggests that dual mode ports in sink
		 * mode should transition to state PE_SRC_Transition_to_default.
		 * See USB power delivery specification chapter 8.3.3.6.1.3.
		 * This would mean to to
		 * - turn off VCONN, reset power supply
		 * - request hardware reset
		 * - turn on VCONN
		 * - Transition to state PE_Src_Startup
		 * SNK only ports shall transition to state Snk_Startup
		 * (see chapter 8.3.3.3.8).
		 * Similar, dual-mode ports in source mode should transition
		 * to PE_SNK_Transition_to_default.
		 */
		if (port->pd_capable) {
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
			sprd_tcpm_set_charge(port, true);
		}
		sprd_tcpm_set_attached_state(port, true);
		sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;

	/* Soft_Reset states */
	case SOFT_RESET:
		port->message_id = 0;
		port->rx_msgid = -1;
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_SEND_CAPABILITIES, 0);
		else
			sprd_tcpm_set_state(port, SNK_WAIT_CAPABILITIES, 0);
		break;
	case SOFT_RESET_SEND:
		port->message_id = 0;
		port->rx_msgid = -1;
		if (sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_SOFT_RESET))
			sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port), 0);
		else
			sprd_tcpm_set_state_cond(port, sprd_hard_reset_state(port),
						 SPRD_PD_T_SENDER_RESPONSE);
		break;

	/* DR_Swap states */
	case DR_SWAP_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_DR_SWAP);
		sprd_tcpm_set_state_cond(port, DR_SWAP_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE);
		break;
	case DR_SWAP_ACCEPT:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		sprd_tcpm_set_state_cond(port, DR_SWAP_CHANGE_DR, 0);
		break;
	case DR_SWAP_SEND_TIMEOUT:
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case DR_SWAP_CHANGE_DR:
		if (port->data_role == TYPEC_HOST) {
			sprd_tcpm_unregister_altmodes(port);
			sprd_tcpm_set_roles(port, true, port->pwr_role, TYPEC_DEVICE);
		} else {
			sprd_tcpm_set_roles(port, true, port->pwr_role, TYPEC_HOST);
			port->send_discover = true;
		}
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;

	/* PR_Swap states */
	case PR_SWAP_ACCEPT:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		sprd_tcpm_set_state(port, PR_SWAP_START, 0);
		break;
	case PR_SWAP_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PR_SWAP);
		sprd_tcpm_set_state_cond(port, PR_SWAP_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE);
		break;
	case PR_SWAP_SEND_TIMEOUT:
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case PR_SWAP_START:
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_TRANSITION_OFF,
					    SPRD_PD_T_SRC_TRANSITION);
		else
			sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SINK_OFF, 0);
		break;
	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
		sprd_tcpm_set_vbus(port, false);
		port->explicit_contract = false;
		/* allow time for Vbus discharge, must be < tSrcSwapStdby */
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF, SPRD_PD_T_SRCSWAPSTDBY);
		break;
	case PR_SWAP_SRC_SNK_SOURCE_OFF:
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_RD);
		/* allow CC debounce */
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED,
				    SPRD_PD_T_CC_DEBOUNCE);
		break;
	case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
		/*
		 * USB-PD standard, 6.2.1.4, Port Power Role:
		 * "During the Power Role Swap Sequence, for the initial Source
		 * Port, the Port Power Role field shall be set to Sink in the
		 * PS_RDY Message indicating that the initial Source’s power
		 * supply is turned off"
		 */
		sprd_tcpm_set_pwr_role(port, TYPEC_SINK);
		if (sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY)) {
			sprd_tcpm_set_state(port, ERROR_RECOVERY, 0);
			break;
		}
		sprd_tcpm_set_state_cond(port, SNK_UNATTACHED, SPRD_PD_T_PS_SOURCE_ON);
		break;
	case PR_SWAP_SRC_SNK_SINK_ON:
		sprd_tcpm_set_state(port, SNK_STARTUP, 0);
		break;
	case PR_SWAP_SNK_SRC_SINK_OFF:
		sprd_tcpm_set_charge(port, false);
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port), SPRD_PD_T_PS_SOURCE_OFF);
		break;
	case PR_SWAP_SNK_SRC_SOURCE_ON:
		sprd_tcpm_set_cc(port, sprd_tcpm_rp_cc(port));
		sprd_tcpm_set_vbus(port, true);
		/*
		 * allow time VBUS ramp-up, must be < tNewSrc
		 * Also, this window overlaps with CC debounce as well.
		 * So, Wait for the max of two which is SPRD_PD_T_NEWSRC
		 */
		sprd_tcpm_set_state(port, PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP,
				    SPRD_PD_T_NEWSRC);
		break;
	case PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP:
		/*
		 * USB PD standard, 6.2.1.4:
		 * "Subsequent Messages initiated by the Policy Engine,
		 * such as the PS_RDY Message sent to indicate that Vbus
		 * is ready, will have the Port Power Role field set to
		 * Source."
		 */
		sprd_tcpm_set_pwr_role(port, TYPEC_SOURCE);
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		sprd_tcpm_set_state(port, SRC_STARTUP, SPRD_PD_T_SWAP_SRC_START);
		break;

	case VCONN_SWAP_ACCEPT:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_ACCEPT);
		sprd_tcpm_set_state(port, VCONN_SWAP_START, 0);
		break;
	case VCONN_SWAP_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_VCONN_SWAP);
		sprd_tcpm_set_state(port, VCONN_SWAP_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE);
		break;
	case VCONN_SWAP_SEND_TIMEOUT:
		sprd_tcpm_swap_complete(port, -ETIMEDOUT);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case VCONN_SWAP_START:
		if (port->vconn_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, VCONN_SWAP_WAIT_FOR_VCONN, 0);
		else
			sprd_tcpm_set_state(port, VCONN_SWAP_TURN_ON_VCONN, 0);
		break;
	case VCONN_SWAP_WAIT_FOR_VCONN:
		sprd_tcpm_set_state(port, sprd_hard_reset_state(port), SPRD_PD_T_VCONN_SOURCE_ON);
		break;
	case VCONN_SWAP_TURN_ON_VCONN:
		sprd_tcpm_set_vconn(port, true);
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_PS_RDY);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case VCONN_SWAP_TURN_OFF_VCONN:
		sprd_tcpm_set_vconn(port, false);
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;

	case DR_SWAP_CANCEL:
	case PR_SWAP_CANCEL:
	case VCONN_SWAP_CANCEL:
		sprd_tcpm_swap_complete(port, port->swap_status);
		if (port->pwr_role == TYPEC_SOURCE)
			sprd_tcpm_set_state(port, SRC_READY, 0);
		else
			sprd_tcpm_set_state(port, SNK_READY, 0);
		break;

	case BIST_RX:
		switch (BDO_MODE_MASK(port->bist_request)) {
		case BDO_MODE_CARRIER2:
			sprd_tcpm_pd_transmit(port, SPRD_TCPC_TX_BIST_MODE_2, NULL);
			break;
		default:
			break;
		}
		/* Always switch to unattached state */
		sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		break;
	case GET_STATUS_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_GET_STATUS);
		sprd_tcpm_set_state(port, GET_STATUS_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE);
		break;
	case GET_STATUS_SEND_TIMEOUT:
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case GET_PPS_STATUS_SEND:
		sprd_tcpm_pd_send_control(port, SPRD_PD_CTRL_GET_PPS_STATUS);
		sprd_tcpm_set_state(port, GET_PPS_STATUS_SEND_TIMEOUT, SPRD_PD_T_SENDER_RESPONSE);
		break;
	case GET_PPS_STATUS_SEND_TIMEOUT:
		sprd_tcpm_set_state(port, sprd_ready_state(port), 0);
		break;
	case ERROR_RECOVERY:
		sprd_tcpm_swap_complete(port, -EPROTO);
		sprd_tcpm_pps_complete(port, -EPROTO);
		sprd_tcpm_set_state(port, PORT_RESET, 0);
		break;
	case PORT_RESET:
		sprd_tcpm_reset_port(port);
		sprd_tcpm_set_cc(port, SPRD_TYPEC_CC_OPEN);
		sprd_tcpm_set_state(port, PORT_RESET_WAIT_OFF, SPRD_PD_T_ERROR_RECOVERY);
		break;
	case PORT_RESET_WAIT_OFF:
		sprd_tcpm_set_state(port,
				    sprd_tcpm_default_state(port),
				    port->vbus_present ? SPRD_PD_T_PS_SOURCE_OFF : 0);
		break;
	default:
		WARN(1, "Unexpected port state %d\n", port->state);
		break;
	}
}

static void sprd_tcpm_state_machine_work(struct work_struct *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
						   state_machine.work);
	enum sprd_tcpm_state prev_state;

	mutex_lock(&port->lock);
	port->state_machine_running = true;

	if (port->queued_message && sprd_tcpm_send_queued_message(port))
		goto done;

	/* If we were queued due to a delayed state change, update it now */
	if (port->delayed_state) {
		sprd_tcpm_log(port, "state change %s -> %s [delayed %ld ms]",
			      sprd_tcpm_states[port->state],
			      sprd_tcpm_states[port->delayed_state], port->delay_ms);
		port->prev_state = port->state;
		port->state = port->delayed_state;
		port->delayed_state = INVALID_STATE;
	}

	/*
	 * Continue running as long as we have (non-delayed) state changes
	 * to make.
	 */
	do {
		prev_state = port->state;
		sprd_run_state_machine(port);
		if (port->queued_message)
			sprd_tcpm_send_queued_message(port);
	} while (port->state != prev_state && !port->delayed_state);

done:
	port->state_machine_running = false;
	mutex_unlock(&port->lock);
}

static void _sprd_tcpm_cc_change(struct sprd_tcpm_port *port,
				 enum sprd_typec_cc_status cc1,
				 enum sprd_typec_cc_status cc2)
{
	enum sprd_typec_cc_status old_cc1, old_cc2;
	enum sprd_tcpm_state new_state;

	old_cc1 = port->cc1;
	old_cc2 = port->cc2;
	port->cc1 = cc1;
	port->cc2 = cc2;

	sprd_tcpm_log_force(port,
			    "CC1: %u -> %u, CC2: %u -> %u [state %s, polarity %d, %s]",
			    old_cc1, cc1, old_cc2, cc2, sprd_tcpm_states[port->state],
			    port->polarity,
			    sprd_tcpm_port_is_disconnected(port) ? "disconnected" : "connected");

	switch (port->state) {
	case TOGGLING:
		if (sprd_tcpm_port_is_debug(port) || sprd_tcpm_port_is_audio(port) ||
		    sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		else if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SRC_UNATTACHED:
	case ACC_UNATTACHED:
		if (sprd_tcpm_port_is_debug(port) || sprd_tcpm_port_is_audio(port) ||
		    sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		break;
	case SRC_ATTACH_WAIT:
		if (sprd_tcpm_port_is_disconnected(port) ||
		    sprd_tcpm_port_is_audio_detached(port))
			sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		else if (cc1 != old_cc1 || cc2 != old_cc2)
			sprd_tcpm_set_state(port, SRC_ATTACH_WAIT, 0);
		break;
	case SRC_ATTACHED:
	case SRC_SEND_CAPABILITIES:
	case SRC_READY:
		if (sprd_tcpm_port_is_disconnected(port) ||
		    !sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_UNATTACHED, 0);
		break;
	case SNK_UNATTACHED:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SNK_ATTACH_WAIT:
		if ((port->cc1 == SPRD_TYPEC_CC_OPEN &&
		     port->cc2 != SPRD_TYPEC_CC_OPEN) ||
		    (port->cc1 != SPRD_TYPEC_CC_OPEN &&
		     port->cc2 == SPRD_TYPEC_CC_OPEN))
			new_state = SNK_DEBOUNCED;
		else if (sprd_tcpm_port_is_disconnected(port))
			new_state = SNK_UNATTACHED;
		else
			break;
		if (new_state != port->delayed_state)
			sprd_tcpm_set_state(port, SNK_ATTACH_WAIT, 0);
		break;
	case SNK_DEBOUNCED:
		if (sprd_tcpm_port_is_disconnected(port))
			new_state = SNK_UNATTACHED;
		else if (port->vbus_present)
			new_state = sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED;
		else
			new_state = SNK_UNATTACHED;
		if (new_state != port->delayed_state)
			sprd_tcpm_set_state(port, SNK_DEBOUNCED, 0);
		break;
	case SNK_READY:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		else if (!port->pd_capable &&
			 (cc1 != old_cc1 || cc2 != old_cc2))
			sprd_tcpm_set_current_limit(port,
						    sprd_tcpm_get_current_limit(port),
						    5000);
		break;

	case AUDIO_ACC_ATTACHED:
		if (cc1 == SPRD_TYPEC_CC_OPEN || cc2 == SPRD_TYPEC_CC_OPEN)
			sprd_tcpm_set_state(port, AUDIO_ACC_DEBOUNCE, 0);
		break;
	case AUDIO_ACC_DEBOUNCE:
		if (sprd_tcpm_port_is_audio(port))
			sprd_tcpm_set_state(port, AUDIO_ACC_ATTACHED, 0);
		break;

	case DEBUG_ACC_ATTACHED:
		if (cc1 == SPRD_TYPEC_CC_OPEN || cc2 == SPRD_TYPEC_CC_OPEN)
			sprd_tcpm_set_state(port, ACC_UNATTACHED, 0);
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;

	case SNK_DISCOVERY:
		/* CC line is unstable, wait for debounce */
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, SNK_DISCOVERY_DEBOUNCE, 0);
		break;
	case SNK_DISCOVERY_DEBOUNCE:
		break;

	case SRC_TRYWAIT:
		/* Hand over to state machine if needed */
		if (!port->vbus_present && sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT_DEBOUNCE, 0);
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		if (port->vbus_present || !sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		if (!sprd_tcpm_port_is_sink(port)) {
			port->max_wait = 0;
			sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		}
		break;
	case SRC_TRY_WAIT:
		if (sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRY_DEBOUNCE, 0);
		break;
	case SRC_TRY_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_TRY_WAIT, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_VBUS, 0);
		break;
	case SNK_TRYWAIT_VBUS:
		if (!sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRYWAIT:
		/* Do nothing, waiting for tCCDebounce */
		break;
	case PR_SWAP_SNK_SRC_SINK_OFF:
	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
	case PR_SWAP_SRC_SNK_SOURCE_OFF:
	case PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED:
	case PR_SWAP_SNK_SRC_SOURCE_ON:
		/*
		 * CC state change is expected in PR_SWAP
		 * Ignore it.
		 */
		break;

	case PORT_RESET:
	case PORT_RESET_WAIT_OFF:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore CC changes here.
		 */
		break;

	default:
		if (sprd_tcpm_port_is_disconnected(port))
			sprd_tcpm_set_state(port, sprd_unattached_state(port), 0);
		break;
	}
}

static void _sprd_tcpm_pd_vbus_on(struct sprd_tcpm_port *port)
{
	int i;

	port->nr_snk_pdo = port->nr_snk_default_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		port->snk_pdo[i] = port->snk_default_pdo[i];

	port->operating_snk_mw = port->operating_snk_default_mw;

	sprd_tcpm_log_force(port, "VBUS on");
	port->vbus_present = true;
	switch (port->state) {
	case SNK_TRANSITION_SINK_VBUS:
		port->explicit_contract = true;
		sprd_tcpm_set_state(port, SNK_READY, 0);
		break;
	case SNK_DISCOVERY:
		sprd_tcpm_set_state(port, SNK_DISCOVERY, 0);
		break;

	case SNK_DEBOUNCED:
		sprd_tcpm_set_state(port,
				    sprd_tcpm_try_src(port) ? SRC_TRY : SNK_ATTACHED,
				    0);
		break;
	case SNK_HARD_RESET_WAIT_VBUS:
		sprd_tcpm_set_state(port, SNK_HARD_RESET_SINK_ON, 0);
		break;
	case SRC_ATTACHED:
		sprd_tcpm_set_state(port, SRC_STARTUP, 0);
		break;
	case SRC_HARD_RESET_VBUS_ON:
		sprd_tcpm_set_state(port, SRC_STARTUP, 0);
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;
	case SRC_TRYWAIT:
		/* Do nothing, Waiting for Rd to be detected */
		break;
	case SRC_TRYWAIT_DEBOUNCE:
		sprd_tcpm_set_state(port, SRC_TRYWAIT, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		/* Do nothing, waiting for PD_DEBOUNCE to do be done */
		break;
	case SNK_TRYWAIT:
		/* Do nothing, waiting for tCCDebounce */
		break;
	case SNK_TRYWAIT_VBUS:
		if (sprd_tcpm_port_is_sink(port))
			sprd_tcpm_set_state(port, SNK_ATTACHED, 0);
		break;
	case SNK_TRYWAIT_DEBOUNCE:
		/* Do nothing, waiting for Rp */
		break;
	case SRC_TRY_WAIT:
	case SRC_TRY_DEBOUNCE:
		/* Do nothing, waiting for sink detection */
		break;

	case PORT_RESET:
	case PORT_RESET_WAIT_OFF:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore vbus changes here.
		 */
		break;

	default:
		break;
	}
}

static void _sprd_tcpm_pd_vbus_off(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log_force(port, "VBUS off");
	port->vbus_present = false;
	port->vbus_never_low = false;
	switch (port->state) {
	case SNK_HARD_RESET_SINK_OFF:
		sprd_tcpm_set_state(port, SNK_HARD_RESET_WAIT_VBUS, 0);
		break;
	case SRC_HARD_RESET_VBUS_OFF:
		sprd_tcpm_set_state(port, SRC_HARD_RESET_VBUS_ON, 0);
		break;
	case HARD_RESET_SEND:
		break;

	case SNK_TRY:
		/* Do nothing, waiting for timeout */
		break;
	case SRC_TRYWAIT:
		/* Hand over to state machine if needed */
		if (sprd_tcpm_port_is_source(port))
			sprd_tcpm_set_state(port, SRC_TRYWAIT_DEBOUNCE, 0);
		break;
	case SNK_TRY_WAIT_DEBOUNCE:
		/* Do nothing, waiting for PD_DEBOUNCE to do be done */
		break;
	case SNK_TRYWAIT:
	case SNK_TRYWAIT_VBUS:
	case SNK_TRYWAIT_DEBOUNCE:
		break;
	case SNK_ATTACH_WAIT:
		sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;

	case SNK_NEGOTIATE_CAPABILITIES:
		break;

	case PR_SWAP_SRC_SNK_TRANSITION_OFF:
		sprd_tcpm_set_state(port, PR_SWAP_SRC_SNK_SOURCE_OFF, 0);
		break;

	case PR_SWAP_SNK_SRC_SINK_OFF:
		/* Do nothing, expected */
		break;

	case PORT_RESET_WAIT_OFF:
		sprd_tcpm_set_state(port, sprd_tcpm_default_state(port), 0);
		break;

	case SRC_TRY_WAIT:
	case SRC_TRY_DEBOUNCE:
		/* Do nothing, waiting for sink detection */
		break;

	case PORT_RESET:
		/*
		 * State set back to default mode once the timer completes.
		 * Ignore vbus changes here.
		 */
		break;

	default:
		if (port->pwr_role == TYPEC_SINK &&
		    port->attached)
			sprd_tcpm_set_state(port, SNK_UNATTACHED, 0);
		break;
	}
}

static void _sprd_tcpm_pd_hard_reset(struct sprd_tcpm_port *port)
{
	sprd_tcpm_log_force(port, "Received hard reset");
	/*
	 * If we keep receiving hard reset requests, executing the hard reset
	 * must have failed. Revert to error recovery if that happens.
	 */
	sprd_tcpm_set_state(port,
			    port->hard_reset_count < SPRD_PD_N_HARD_RESET_COUNT ?
			    HARD_RESET_START : ERROR_RECOVERY,
			    0);
}

static void sprd_tcpm_pd_event_handler(struct work_struct *work)
{
	struct sprd_tcpm_port *port = container_of(work, struct sprd_tcpm_port,
					      event_work);
	u32 events;

	mutex_lock(&port->lock);

	spin_lock(&port->pd_event_lock);
	while (port->pd_events) {
		events = port->pd_events;
		port->pd_events = 0;
		spin_unlock(&port->pd_event_lock);
		if (events & SPRD_TCPM_RESET_EVENT)
			_sprd_tcpm_pd_hard_reset(port);
		if (events & SPRD_TCPM_VBUS_EVENT) {
			bool vbus;

			vbus = port->tcpc->get_vbus(port->tcpc);
			if (vbus)
				_sprd_tcpm_pd_vbus_on(port);
			else
				_sprd_tcpm_pd_vbus_off(port);
		}
		if (events & SPRD_TCPM_CC_EVENT) {
			enum sprd_typec_cc_status cc1, cc2;

			if (port->tcpc->get_cc(port->tcpc, &cc1, &cc2) == 0)
				_sprd_tcpm_cc_change(port, cc1, cc2);
		}
		spin_lock(&port->pd_event_lock);
	}
	spin_unlock(&port->pd_event_lock);
	mutex_unlock(&port->lock);
}

void sprd_tcpm_cc_change(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events |= SPRD_TCPM_CC_EVENT;
	spin_unlock(&port->pd_event_lock);
	queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_cc_change);

void sprd_tcpm_vbus_change(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events |= SPRD_TCPM_VBUS_EVENT;
	spin_unlock(&port->pd_event_lock);
	queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_vbus_change);

void sprd_tcpm_pd_hard_reset(struct sprd_tcpm_port *port)
{
	spin_lock(&port->pd_event_lock);
	port->pd_events = SPRD_TCPM_RESET_EVENT;
	spin_unlock(&port->pd_event_lock);
	queue_work(port->wq, &port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_pd_hard_reset);

static int sprd_tcpm_dr_set(struct typec_port *p, enum typec_data_role data)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (port->port_type != TYPEC_PORT_DRP) {
		ret = -EINVAL;
		goto port_unlock;
	}
	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (port->data_role == data) {
		ret = 0;
		goto port_unlock;
	}

	/*
	 * XXX
	 * 6.3.9: If an alternate mode is active, a request to swap
	 * alternate modes shall trigger a port reset.
	 * Reject data role swap request in this case.
	 */

	if (!port->pd_capable) {
		/*
		 * If the partner is not PD capable, reset the port to
		 * trigger a role change. This can only work if a preferred
		 * role is configured, and if it matches the requested role.
		 */
		if (port->try_role == TYPEC_NO_PREFERRED_ROLE ||
		    port->try_role == port->pwr_role) {
			ret = -EINVAL;
			goto port_unlock;
		}
		port->non_pd_role_swap = true;
		sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else {
		sprd_tcpm_set_state(port, DR_SWAP_SEND, 0);
	}

	port->swap_status = 0;
	port->swap_pending = true;
	reinit_completion(&port->swap_complete);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	port->non_pd_role_swap = false;
	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_pr_set(struct typec_port *p, enum typec_role role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (port->port_type != TYPEC_PORT_DRP) {
		ret = -EINVAL;
		goto port_unlock;
	}
	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (role == port->pwr_role) {
		ret = 0;
		goto port_unlock;
	}

	port->swap_status = 0;
	port->swap_pending = true;
	reinit_completion(&port->swap_complete);
	sprd_tcpm_set_state(port, PR_SWAP_SEND, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_vconn_set(struct typec_port *p, enum typec_role role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (port->state != SRC_READY && port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (role == port->vconn_role) {
		ret = 0;
		goto port_unlock;
	}

	port->swap_status = 0;
	port->swap_pending = true;
	reinit_completion(&port->swap_complete);
	sprd_tcpm_set_state(port, VCONN_SWAP_SEND, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->swap_complete,
					 msecs_to_jiffies(SPRD_PD_ROLE_SWAP_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->swap_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);
	return ret;
}

static int sprd_tcpm_try_role(struct typec_port *p, int role)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);
	struct tcpc_dev	*tcpc = port->tcpc;
	int ret = 0;

	mutex_lock(&port->lock);
	if (tcpc->try_role)
		ret = tcpc->try_role(tcpc, role);
	if (!ret && (!tcpc->config || !tcpc->config->try_role_hw))
		port->try_role = role;
	port->try_src_count = 0;
	port->try_snk_count = 0;
	mutex_unlock(&port->lock);

	return ret;
}

static int sprd_tcpm_pps_set_op_curr(struct sprd_tcpm_port *port, u16 req_op_curr)
{
	unsigned int target_mw;
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.active) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (req_op_curr > port->pps_data.max_curr) {
		ret = -EINVAL;
		goto port_unlock;
	}

	target_mw = (req_op_curr * port->supply_voltage) / 1000;
	if (target_mw < port->operating_snk_mw) {
		ret = -EINVAL;
		goto port_unlock;
	}

	/* Round down operating current to align with PPS valid steps */
	req_op_curr = req_op_curr - (req_op_curr % SPRD_RDO_PROG_CURR_MA_STEP);

	reinit_completion(&port->pps_complete);
	port->pps_data.req_op_curr = req_op_curr;
	port->pps_status = 0;
	port->pps_pending = true;
	sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					  msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static int sprd_tcpm_pps_set_out_volt(struct sprd_tcpm_port *port, u16 req_out_volt)
{
	unsigned int target_mw;
	int ret;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.active) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	if (req_out_volt < port->pps_data.min_volt ||
	    req_out_volt > port->pps_data.max_volt) {
		ret = -EINVAL;
		goto port_unlock;
	}

	target_mw = (port->current_limit * req_out_volt) / 1000;
	if (target_mw < port->operating_snk_mw) {
		ret = -EINVAL;
		goto port_unlock;
	}

	/* Round down output voltage to align with PPS valid steps */
	req_out_volt = req_out_volt - (req_out_volt % SPRD_RDO_PROG_VOLT_MV_STEP);

	reinit_completion(&port->pps_complete);
	port->pps_data.req_out_volt = req_out_volt;
	port->pps_status = 0;
	port->pps_pending = true;
	sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					 msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static int sprd_tcpm_pps_activate(struct sprd_tcpm_port *port, bool activate)
{
	int ret = 0;

	mutex_lock(&port->swap_lock);
	mutex_lock(&port->lock);

	if (!port->pps_data.supported) {
		ret = -EOPNOTSUPP;
		goto port_unlock;
	}

	/* Trying to deactivate PPS when already deactivated so just bail */
	if (!port->pps_data.active && !activate)
		goto port_unlock;

	if (port->state != SNK_READY) {
		ret = -EAGAIN;
		goto port_unlock;
	}

	reinit_completion(&port->pps_complete);
	port->pps_status = 0;
	port->pps_pending = true;

	/* Trigger PPS request or move back to standard PDO contract */
	if (activate) {
		port->pps_data.req_out_volt = port->supply_voltage;
		port->pps_data.req_op_curr = port->current_limit;
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
	} else {
		sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
	}
	mutex_unlock(&port->lock);

	if (!wait_for_completion_timeout(&port->pps_complete,
					 msecs_to_jiffies(SPRD_PD_PPS_CTRL_TIMEOUT)))
		ret = -ETIMEDOUT;
	else
		ret = port->pps_status;

	goto swap_unlock;

port_unlock:
	mutex_unlock(&port->lock);
swap_unlock:
	mutex_unlock(&port->swap_lock);

	return ret;
}

static void sprd_tcpm_init(struct sprd_tcpm_port *port)
{
	enum sprd_typec_cc_status cc1, cc2;

	port->tcpc->init(port->tcpc);

	sprd_tcpm_reset_port(port);

	/*
	 * XXX
	 * Should possibly wait for VBUS to settle if it was enabled locally
	 * since sprd_tcpm_reset_port() will disable VBUS.
	 */
	port->vbus_present = port->tcpc->get_vbus(port->tcpc);
	if (port->vbus_present)
		port->vbus_never_low = true;

	sprd_tcpm_set_state(port, sprd_tcpm_default_state(port), 0);

	if (port->tcpc->get_cc(port->tcpc, &cc1, &cc2) == 0)
		_sprd_tcpm_cc_change(port, cc1, cc2);

	/*
	 * Some adapters need a clean slate at startup, and won't recover
	 * otherwise. So do not try to be fancy and force a clean disconnect.
	 */
	sprd_tcpm_set_state(port, PORT_RESET, 0);
}

static int sprd_tcpm_port_type_set(struct typec_port *p, enum typec_port_type type)
{
	struct sprd_tcpm_port *port = typec_get_drvdata(p);

	mutex_lock(&port->lock);
	if (type == port->port_type)
		goto port_unlock;

	port->port_type = type;

	if (!port->connected) {
		sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else if (type == TYPEC_PORT_SNK) {
		if (!(port->pwr_role == TYPEC_SINK &&
		      port->data_role == TYPEC_DEVICE))
			sprd_tcpm_set_state(port, PORT_RESET, 0);
	} else if (type == TYPEC_PORT_SRC) {
		if (!(port->pwr_role == TYPEC_SOURCE &&
		      port->data_role == TYPEC_HOST))
			sprd_tcpm_set_state(port, PORT_RESET, 0);
	}

port_unlock:
	mutex_unlock(&port->lock);
	return 0;
}

static const struct typec_operations sprd_tcpm_ops = {
	.try_role = sprd_tcpm_try_role,
	.dr_set = sprd_tcpm_dr_set,
	.pr_set = sprd_tcpm_pr_set,
	.vconn_set = sprd_tcpm_vconn_set,
	.port_type_set = sprd_tcpm_port_type_set
};

void sprd_tcpm_tcpc_reset(struct sprd_tcpm_port *port)
{
	mutex_lock(&port->lock);
	/* XXX: Maintain PD connection if possible? */
	sprd_tcpm_init(port);
	mutex_unlock(&port->lock);
}

static int sprd_tcpm_copy_pdos(u32 *dest_pdo, const u32 *src_pdo, unsigned int nr_pdo)
{
	unsigned int i;

	if (nr_pdo > SPRD_PDO_MAX_OBJECTS)
		nr_pdo = SPRD_PDO_MAX_OBJECTS;

	for (i = 0; i < nr_pdo; i++)
		dest_pdo[i] = src_pdo[i];

	return nr_pdo;
}

static int sprd_tcpm_copy_vdos(u32 *dest_vdo, const u32 *src_vdo, unsigned int nr_vdo)
{
	unsigned int i;

	if (nr_vdo > VDO_MAX_OBJECTS)
		nr_vdo = VDO_MAX_OBJECTS;

	for (i = 0; i < nr_vdo; i++)
		dest_vdo[i] = src_vdo[i];

	return nr_vdo;
}

static int sprd_tcpm_fw_get_caps(struct sprd_tcpm_port *port, struct fwnode_handle *fwnode)
{
	const char *cap_str;
	int ret, i;
	u32 uw;

	if (!fwnode)
		return -EINVAL;

	/* USB data support is optional */
	ret = fwnode_property_read_string(fwnode, "data-role", &cap_str);
	if (ret == 0) {
		ret = typec_find_port_data_role(cap_str);
		if (ret < 0)
			return ret;
		port->typec_caps.data = ret;
	}

	ret = fwnode_property_read_string(fwnode, "power-role", &cap_str);
	if (ret < 0)
		return ret;

	ret = typec_find_port_power_role(cap_str);
	if (ret < 0)
		return ret;
	port->typec_caps.type = ret;
	port->port_type = port->typec_caps.type;

	if (port->port_type == TYPEC_PORT_SNK)
		goto sink;

	/* Get source pdos */
	ret = fwnode_property_count_u32(fwnode, "source-pdos");
	if (ret <= 0)
		return -EINVAL;

	port->nr_src_pdo = min(ret, SPRD_PDO_MAX_OBJECTS);
	ret = fwnode_property_read_u32_array(fwnode, "source-pdos",
					     port->src_pdo, port->nr_src_pdo);
	if ((ret < 0) || sprd_tcpm_validate_caps(port, port->src_pdo, port->nr_src_pdo))
		return -EINVAL;

	if (port->port_type == TYPEC_PORT_SRC)
		return 0;

	/* Get the preferred power role for DRP */
	ret = fwnode_property_read_string(fwnode, "try-power-role", &cap_str);
	if (ret < 0)
		return ret;

	port->typec_caps.prefer_role = typec_find_power_role(cap_str);
	if (port->typec_caps.prefer_role < 0)
		return -EINVAL;
sink:
	/* Get sink pdos */
	ret = fwnode_property_count_u32(fwnode, "sink-pdos");
	if (ret <= 0)
		return -EINVAL;

	port->nr_snk_pdo = min(ret, SPRD_PDO_MAX_OBJECTS);
	ret = fwnode_property_read_u32_array(fwnode, "sink-pdos",
					     port->snk_pdo, port->nr_snk_pdo);
	if ((ret < 0) || sprd_tcpm_validate_caps(port, port->snk_pdo,
					    port->nr_snk_pdo))
		return -EINVAL;

	port->nr_snk_default_pdo = port->nr_snk_pdo;
	for (i = 0; i < port->nr_snk_default_pdo; i++)
		port->snk_default_pdo[i] = port->snk_pdo[i];

	if (fwnode_property_read_u32(fwnode, "op-sink-microwatt", &uw) < 0)
		return -EINVAL;
	port->operating_snk_default_mw = uw / 1000;
	port->operating_snk_mw = port->operating_snk_default_mw;

	port->self_powered = fwnode_property_read_bool(fwnode, "self-powered");

	return 0;
}

int sprd_tcpm_update_sink_capabilities(struct sprd_tcpm_port *port, const u32 *pdo,
				       unsigned int nr_pdo,
				       unsigned int operating_snk_mw)
{
	if (sprd_tcpm_validate_caps(port, pdo, nr_pdo))
		return -EINVAL;

	mutex_lock(&port->lock);
	port->nr_snk_pdo = sprd_tcpm_copy_pdos(port->snk_pdo, pdo, nr_pdo);
	port->operating_snk_mw = operating_snk_mw;
	port->update_sink_caps = true;

	switch (port->state) {
	case SNK_NEGOTIATE_CAPABILITIES:
	case SNK_NEGOTIATE_PPS_CAPABILITIES:
	case SNK_READY:
	case SNK_TRANSITION_SINK:
	case SNK_TRANSITION_SINK_VBUS:
		if (port->pps_data.active)
			sprd_tcpm_set_state(port, SNK_NEGOTIATE_PPS_CAPABILITIES, 0);
		else
			sprd_tcpm_set_state(port, SNK_NEGOTIATE_CAPABILITIES, 0);
		break;
	default:
		break;
	}
	mutex_unlock(&port->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(sprd_tcpm_update_sink_capabilities);

void sprd_tcpm_get_source_capabilities(struct sprd_tcpm_port *port,
				       struct adapter_power_cap *pd_source_cap)
{
	int i;

	if (!port) {
		pd_source_cap->nr_source_caps = 0;
		pr_warn("port Null!!!\n");
		return;
	}

	/* Clears SRC_CAP in the disconnected state */
	if (sprd_tcpm_port_is_disconnected(port) &&
	    (port->state == SRC_UNATTACHED || port->state == SNK_UNATTACHED ||
	     port->state == TOGGLING)) {
		for (i = 0; i < pd_source_cap->nr_source_caps; i++) {
			pd_source_cap->max_mv[i] = 0;
			pd_source_cap->min_mv[i] = 0;
			pd_source_cap->ma[i] = 0;
			pd_source_cap->pwr_mw_limit[i] = 0;
		}
		pd_source_cap->nr_source_caps = 0;
		return;
	}

	pd_source_cap->nr_source_caps = port->nr_source_caps;
	for (i = 0; i < port->nr_source_caps; i++) {
		u32 pdo = port->source_caps[i];
		enum sprd_pd_pdo_type type = sprd_pdo_type(pdo);

		pd_source_cap->type[i] = type;
		switch (type) {
		case SPRD_PDO_TYPE_FIXED:
			pd_source_cap->max_mv[i] = sprd_pdo_fixed_voltage(pdo);
			pd_source_cap->min_mv[i] = pd_source_cap->max_mv[i];
			pd_source_cap->ma[i] = sprd_pdo_max_current(pdo);
			break;
		case SPRD_PDO_TYPE_VAR:
			pd_source_cap->max_mv[i] = sprd_pdo_max_voltage(pdo);
			pd_source_cap->min_mv[i] = sprd_pdo_min_voltage(pdo);
			pd_source_cap->ma[i] = sprd_pdo_max_current(pdo);
			break;
		case SPRD_PDO_TYPE_BATT:
			pd_source_cap->max_mv[i] = sprd_pdo_max_voltage(pdo);
			pd_source_cap->min_mv[i] = sprd_pdo_min_voltage(pdo);
			pd_source_cap->pwr_mw_limit[i] = sprd_pdo_max_power(pdo);
			break;
		case SPRD_PDO_TYPE_APDO:
			if (sprd_pdo_apdo_type(pdo) == SPRD_APDO_TYPE_PPS) {
				pd_source_cap->max_mv[i] = sprd_pdo_pps_apdo_max_voltage(pdo);
				pd_source_cap->min_mv[i] = sprd_pdo_pps_apdo_min_voltage(pdo);
				pd_source_cap->ma[i] = sprd_pdo_pps_apdo_max_current(pdo);
			} else {
				pd_source_cap->nr_source_caps = pd_source_cap->nr_source_caps - 1;
			}
			break;
		default:
			pd_source_cap->nr_source_caps = pd_source_cap->nr_source_caps - 1;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(sprd_tcpm_get_source_capabilities);

/* Power Supply access to expose source power information */
enum sprd_tcpm_psy_online_states {
	TCPM_PSY_OFFLINE = 0,
	TCPM_PSY_FIXED_ONLINE,
	TCPM_PSY_PROG_ONLINE,
};

static enum power_supply_property sprd_tcpm_psy_props[] = {
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int sprd_tcpm_psy_get_online(struct sprd_tcpm_port *port,
				    union power_supply_propval *val)
{
	if (port->vbus_charge) {
		if (port->pps_data.active)
			val->intval = TCPM_PSY_PROG_ONLINE;
		else
			val->intval = TCPM_PSY_FIXED_ONLINE;
	} else {
		val->intval = TCPM_PSY_OFFLINE;
	}

	return 0;
}

static int sprd_tcpm_psy_get_voltage_min(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.min_volt * 1000;
	else
		val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_voltage_max(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.max_volt * 1000;
	else
		val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_voltage_now(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	val->intval = port->supply_voltage * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_current_max(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	if (port->pps_data.active)
		val->intval = port->pps_data.max_curr * 1000;
	else
		val->intval = port->current_limit * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_current_now(struct sprd_tcpm_port *port,
					 union power_supply_propval *val)
{
	val->intval = port->current_limit * 1000;

	return 0;
}

static int sprd_tcpm_psy_get_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct sprd_tcpm_port *port = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = port->usb_type;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sprd_tcpm_psy_get_online(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		ret = sprd_tcpm_psy_get_voltage_min(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = sprd_tcpm_psy_get_voltage_max(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sprd_tcpm_psy_get_voltage_now(port, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = sprd_tcpm_psy_get_current_max(port, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sprd_tcpm_psy_get_current_now(port, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_set_online(struct sprd_tcpm_port *port,
				    const union power_supply_propval *val)
{
	int ret;

	switch (val->intval) {
	case TCPM_PSY_FIXED_ONLINE:
		ret = sprd_tcpm_pps_activate(port, false);
		break;
	case TCPM_PSY_PROG_ONLINE:
		ret = sprd_tcpm_pps_activate(port, true);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_set_prop(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct sprd_tcpm_port *port = power_supply_get_drvdata(psy);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sprd_tcpm_psy_set_online(port, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (val->intval < port->pps_data.min_volt * 1000 ||
		    val->intval > port->pps_data.max_volt * 1000)
			ret = -EINVAL;
		else
			ret = sprd_tcpm_pps_set_out_volt(port, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval > port->pps_data.max_curr * 1000)
			ret = -EINVAL;
		else
			ret = sprd_tcpm_pps_set_op_curr(port, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprd_tcpm_psy_prop_writeable(struct power_supply *psy,
					enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_usb_type sprd_tcpm_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
};

static const char *tcpm_psy_name_prefix = "sprd-tcpm-source-psy-";

static int devm_sprd_tcpm_psy_register(struct sprd_tcpm_port *port)
{
	struct power_supply_config psy_cfg = {};
	const char *port_dev_name = "sc27xx-pd";
	size_t psy_name_len = strlen(tcpm_psy_name_prefix) +
				     strlen(port_dev_name) + 1;
	char *psy_name;

	psy_cfg.drv_data = port;
	psy_cfg.fwnode = dev_fwnode(port->dev);
	psy_name = devm_kzalloc(port->dev, psy_name_len, GFP_KERNEL);
	if (!psy_name)
		return -ENOMEM;

	snprintf(psy_name, psy_name_len, "%s%s", tcpm_psy_name_prefix,
		 port_dev_name);
	port->psy_desc.name = psy_name;
	port->psy_desc.type = POWER_SUPPLY_TYPE_USB,
	port->psy_desc.usb_types = sprd_tcpm_psy_usb_types;
	port->psy_desc.num_usb_types = ARRAY_SIZE(sprd_tcpm_psy_usb_types);
	port->psy_desc.properties = sprd_tcpm_psy_props,
	port->psy_desc.num_properties = ARRAY_SIZE(sprd_tcpm_psy_props),
	port->psy_desc.get_property = sprd_tcpm_psy_get_prop,
	port->psy_desc.set_property = sprd_tcpm_psy_set_prop,
	port->psy_desc.property_is_writeable = sprd_tcpm_psy_prop_writeable,

	port->usb_type = POWER_SUPPLY_USB_TYPE_C;
	port->last_usb_type = POWER_SUPPLY_USB_TYPE_C;

	port->psy = devm_power_supply_register(port->dev, &port->psy_desc,
					       &psy_cfg);

	return PTR_ERR_OR_ZERO(port->psy);
}

static int sprd_tcpm_copy_caps(struct sprd_tcpm_port *port, const struct tcpc_config *tcfg)
{
	if (sprd_tcpm_validate_caps(port, tcfg->src_pdo, tcfg->nr_src_pdo) ||
	    sprd_tcpm_validate_caps(port, tcfg->snk_pdo, tcfg->nr_snk_pdo))
		return -EINVAL;

	port->nr_src_pdo = sprd_tcpm_copy_pdos(port->src_pdo, tcfg->src_pdo, tcfg->nr_src_pdo);
	port->nr_snk_pdo = sprd_tcpm_copy_pdos(port->snk_pdo, tcfg->snk_pdo, tcfg->nr_snk_pdo);

	port->nr_snk_vdo = sprd_tcpm_copy_vdos(port->snk_vdo, tcfg->snk_vdo, tcfg->nr_snk_vdo);

	port->operating_snk_mw = tcfg->operating_snk_mw;

	port->typec_caps.prefer_role = tcfg->default_role;
	port->typec_caps.type = tcfg->type;
	port->typec_caps.data = tcfg->data;
	port->self_powered = tcfg->self_powered;

	return 0;
}

void sprd_tcpm_shutdown(struct sprd_tcpm_port *port)
{
	int ret;

	if (port->pps_data.active) {
		ret = sprd_tcpm_pps_activate(port, false);
		if (ret) {
			pr_err("failed to disable pps at shutdown, ret = %d", ret);
			return;
		}
	}

	cancel_delayed_work_sync(&port->state_machine);
	cancel_delayed_work_sync(&port->vdm_state_machine);
	cancel_work_sync(&port->event_work);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_shutdown);

struct sprd_tcpm_port *sprd_tcpm_register_port(struct device *dev, struct tcpc_dev *tcpc)
{
	struct sprd_tcpm_port *port;
	int i, err;

	if (!dev || !tcpc ||
	    !tcpc->get_vbus || !tcpc->set_cc || !tcpc->get_cc ||
	    !tcpc->set_polarity || !tcpc->set_vconn || !tcpc->set_vbus ||
	    !tcpc->set_pd_rx || !tcpc->set_roles || !tcpc->pd_transmit)
		return ERR_PTR(-EINVAL);

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return ERR_PTR(-ENOMEM);

	port->dev = dev;
	port->tcpc = tcpc;

	mutex_init(&port->lock);
	mutex_init(&port->swap_lock);

	port->wq = create_singlethread_workqueue(dev_name(dev));
	if (!port->wq)
		return ERR_PTR(-ENOMEM);
	INIT_DELAYED_WORK(&port->state_machine, sprd_tcpm_state_machine_work);
	INIT_DELAYED_WORK(&port->vdm_state_machine, sprd_vdm_state_machine_work);
	INIT_WORK(&port->event_work, sprd_tcpm_pd_event_handler);

	spin_lock_init(&port->pd_event_lock);

	init_completion(&port->tx_complete);
	init_completion(&port->swap_complete);
	init_completion(&port->pps_complete);
	sprd_tcpm_debugfs_init(port);

	err = sprd_tcpm_fw_get_caps(port, tcpc->fwnode);
	if ((err < 0) && tcpc->config)
		err = sprd_tcpm_copy_caps(port, tcpc->config);
	if (err < 0)
		goto out_destroy_wq;

	if (!tcpc->config || !tcpc->config->try_role_hw)
		port->try_role = port->typec_caps.prefer_role;
	else
		port->try_role = TYPEC_NO_PREFERRED_ROLE;

	port->typec_caps.fwnode = tcpc->fwnode;
	port->typec_caps.revision = 0x0120;	/* Type-C spec release 1.2 */
	port->typec_caps.pd_revision = 0x0300;	/* USB-PD spec release 3.0 */
	port->typec_caps.driver_data = port;
	port->typec_caps.ops = &sprd_tcpm_ops;

	port->partner_desc.identity = &port->partner_ident;
	port->port_type = port->typec_caps.type;

	port->role_sw = usb_role_switch_get(port->dev);
	if (IS_ERR(port->role_sw)) {
		err = PTR_ERR(port->role_sw);
		goto out_destroy_wq;
	}

	err = devm_sprd_tcpm_psy_register(port);
	if (err)
		goto out_role_sw_put;

	port->typec_port = typec_register_port(port->dev, &port->typec_caps);
	if (IS_ERR(port->typec_port)) {
		err = PTR_ERR(port->typec_port);
		goto out_role_sw_put;
	}

	if (tcpc->config && tcpc->config->alt_modes) {
		const struct typec_altmode_desc *paltmode = tcpc->config->alt_modes;

		i = 0;
		while (paltmode->svid && i < ARRAY_SIZE(port->port_altmode)) {
			struct typec_altmode *alt;

			alt = typec_port_register_altmode(port->typec_port,
							  paltmode);
			if (IS_ERR(alt)) {
				sprd_tcpm_log(port,
					      "%s: failed to register port alternate mode 0x%x",
					      dev_name(dev), paltmode->svid);
				break;
			}
			typec_altmode_set_drvdata(alt, port);
			alt->ops = &sprd_tcpm_altmode_ops;
			port->port_altmode[i] = alt;
			i++;
			paltmode++;
		}
	}

	mutex_lock(&port->lock);
	sprd_tcpm_init(port);
	mutex_unlock(&port->lock);

	sprd_tcpm_log(port, "%s: registered", dev_name(dev));
	return port;

out_role_sw_put:
	usb_role_switch_put(port->role_sw);
out_destroy_wq:
	sprd_tcpm_debugfs_exit(port);
	destroy_workqueue(port->wq);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_register_port);

void sprd_tcpm_unregister_port(struct sprd_tcpm_port *port)
{
	int i;

	sprd_tcpm_reset_port(port);
	for (i = 0; i < ARRAY_SIZE(port->port_altmode); i++)
		typec_unregister_altmode(port->port_altmode[i]);
	typec_unregister_port(port->typec_port);
	usb_role_switch_put(port->role_sw);
	sprd_tcpm_debugfs_exit(port);
	destroy_workqueue(port->wq);
}
EXPORT_SYMBOL_GPL(sprd_tcpm_unregister_port);

MODULE_LICENSE("GPL");
