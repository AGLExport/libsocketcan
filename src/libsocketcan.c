/* libsocketcan.c
 *
 * (C) 2009 Luotao Fu <l.fu@pengutronix.de>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * @file
 * @brief library code
 */
#include "libsocketcan-utils.h"

#include <libsocketcan.h>

/**
 * @ingroup extern
 * can_do_start - start the can interface
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 *
 * This starts the can interface with the given name. It simply changes the if
 * state of the interface to up. All initialisation works will be done in
 * kernel. The if state can also be queried by a simple ifconfig.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_do_start(const char *name)
{
	return set_link(name, IF_UP, NULL);
}

/**
 * @ingroup extern
 * can_do_stop - stop the can interface
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 *
 * This stops the can interface with the given name. It simply changes the if
 * state of the interface to down. Any running communication would be stopped.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_do_stop(const char *name)
{
	return set_link(name, IF_DOWN, NULL);
}

/**
 * @ingroup extern
 * can_do_restart - restart the can interface
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 *
 * This triggers the start mode of the can device.
 *
 * NOTE:
 * - restart mode can only be triggerd if the device is in BUS_OFF and the auto
 * restart not turned on (restart_ms == 0)
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_do_restart(const char *name)
{
	int state;
	__u32 restart_ms;

	/* first we check if we can restart the device at all */
	if ((can_get_state(name, &state)) < 0) {
		fprintf(stderr, "cannot get bustate, "
			"something is seriously wrong\n");
		return -1;
	} else if (state != CAN_STATE_BUS_OFF) {
		fprintf(stderr,
			"Device is not in BUS_OFF," " no use to restart\n");
		return -1;
	}

	if ((can_get_restart_ms(name, &restart_ms)) < 0) {
		fprintf(stderr, "cannot get restart_ms, "
			"something is seriously wrong\n");
		return -1;
	} else if (restart_ms > 0) {
		fprintf(stderr,
			"auto restart with %ums interval is turned on,"
			" no use to restart\n", restart_ms);
		return -1;
	}

	struct req_info req_info = {
		.restart = 1,
	};

	return set_link(name, 0, &req_info);
}

/**
 * @ingroup extern
 * can_set_restart_ms - set interval of auto restart.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param restart_ms interval of auto restart in milliseconds
 *
 * This sets how often the device shall automatically restart the interface in
 * case that a bus_off is detected.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_set_restart_ms(const char *name, __u32 restart_ms)
{
	struct req_info req_info = {
		.restart_ms = restart_ms,
	};

	if (restart_ms == 0)
		req_info.disable_autorestart = 1;

	return set_link(name, 0, &req_info);
}

/**
 * @ingroup extern
 * can_set_ctrlmode - setup the control mode.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 *
 * @param cm pointer of a can_ctrlmode struct
 *
 * This sets the control mode of the can device. There are currently the
 * following modes available (each mapped to its own macro):
 *
 * @code
 * #define CAN_CTRLMODE_LOOPBACK           0x01    // Loopback mode
 * #define CAN_CTRLMODE_LISTENONLY         0x02    // Listen-only mode
 * #define CAN_CTRLMODE_3_SAMPLES          0x04    // Triple sampling mode
 * #define CAN_CTRLMODE_ONE_SHOT           0x08    // One-Shot mode
 * #define CAN_CTRLMODE_BERR_REPORTING     0x10    // Bus-error reporting
 * #define CAN_CTRLMODE_FD                 0x20    // CAN FD mode
 * #define CAN_CTRLMODE_PRESUME_ACK        0x40    // Ignore missing CAN ACKs
 * @endcode
 *
 * You have to define the control mode struct yourself. A can_ctrlmode struct
 * is declared as:
 *
 * @code
 * struct can_ctrlmode {
 *	__u32 mask;
 *	__u32 flags;
 * }
 * @endcode
 *
 * You can use mask to select modes you want to control and flags to determine
 * if you want to turn the selected mode(s) on or off. E. g. the following
 * pseudocode will turn the loopback mode on and listenonly mode off:
 *
 * @code
 * struct can_ctrlmode cm;
 * memset(&cm, 0, sizeof(cm));
 * cm.mask = CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_LISTENONLY;
 * cm.flags = CAN_CTRLMODE_LOOPBACK;
 * can_set_ctrlmode(candev, &cm);
 * @endcode
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_set_ctrlmode(const char *name, struct can_ctrlmode *cm)
{
	struct req_info req_info = {
		.ctrlmode = cm,
	};

	return set_link(name, 0, &req_info);
}

/**
 * @ingroup extern
 * can_set_bittiming - setup the bittiming.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bt pointer to a can_bittiming struct
 *
 * This sets the bittiming of the can device. This is for advantage usage. In
 * normal cases you should use can_set_bitrate to simply define the bitrate and
 * let the driver automatically calculate the bittiming. You will only need this
 * function if you wish to define the bittiming in expert mode with fully
 * manually defined timing values.
 * You have to define the bittiming struct yourself. a can_bittiming struct
 * consists of:
 *
 * @code
 * struct can_bittiming {
 *	__u32 bitrate;
 *	__u32 sample_point;
 *	__u32 tq;
 *	__u32 prop_seg;
 *	__u32 phase_seg1;
 *	__u32 phase_seg2;
 *	__u32 sjw;
 *	__u32 brp;
 * }
 * @endcode
 *
 * to define a customized bittiming, you have to define tq, prop_seq,
 * phase_seg1, phase_seg2 and sjw. See http://www.can-cia.org/index.php?id=88
 * for more information about bittiming and synchronizations on can bus.
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_set_bittiming(const char *name, struct can_bittiming *bt)
{
	struct req_info req_info = {
		.bittiming = bt,
	};

	return set_link(name, 0, &req_info);
}

/**
 * @ingroup extern
 * can_set_data_bittiming - setup the bittiming for CAN FD Data transmission.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bt pointer to a can_bittiming struct
 *
 * This sets the bittiming of the can device for the data transmission if CAN FD is enabled. This is for advantage usage. In
 * normal cases you should use can_set_bitrate to simply define the bitrate and
 * let the driver automatically calculate the bittiming. You will only need this
 * function if you wish to define the bittiming in expert mode with fully
 * manually defined timing values.
 * You have to define the bittiming struct yourself. a can_bittiming struct
 * consists of:
 *
 * @code
 * struct can_bittiming {
 *	__u32 bitrate;
 *	__u32 sample_point;
 *	__u32 tq;
 *	__u32 prop_seg;
 *	__u32 phase_seg1;
 *	__u32 phase_seg2;
 *	__u32 sjw;
 *	__u32 brp;
 * }
 * @endcode
 *
 * to define a customized bittiming, you have to define tq, prop_seq,
 * phase_seg1, phase_seg2 and sjw. See http://www.can-cia.org/index.php?id=88
 * for more information about bittiming and synchronizations on can bus.
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_set_canfd_bittiming(const char *name, struct can_bittiming *bt, struct can_bittiming *dbt)
{
	struct can_ctrlmode ctrl = {
		.mask = CAN_CTRLMODE_FD,
		.flags = CAN_CTRLMODE_FD,
	};
	struct req_info req_info = {
		.bittiming = bt,
		.dbittiming = dbt,
		.ctrlmode = &ctrl
	};

	return set_link(name, 0, &req_info);
}

/**
 * @ingroup extern
 * can_set_bitrate - setup the bitrate.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bitrate bitrate of the can bus
 *
 * This is the recommended way to setup the bus bit timing. You only have to
 * give a bitrate value here. The exact bit timing will be calculated
 * automatically. To use this function, make sure that CONFIG_CAN_CALC_BITTIMING
 * is set to y in your kernel configuration. bitrate can be a value between
 * 1000(1kbit/s) and 1000000(1000kbit/s).
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_set_bitrate(const char *name, __u32 bitrate)
{
	struct can_bittiming bt;

	memset(&bt, 0, sizeof(bt));
	bt.bitrate = bitrate;

	return can_set_bittiming(name, &bt);
}

/**
 * @ingroup extern
 * can_set_bitrate_samplepoint - setup the bitrate.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bitrate bitrate of the can bus
 * @param sample_point sample point value
 *
 * This one is similar to can_set_bitrate, only you can additionally set up the
 * time point for sampling (sample point) customly instead of using the
 * CIA recommended value. sample_point can be a value between 0 and 999.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_set_bitrate_samplepoint(const char *name, __u32 bitrate,
				__u32 sample_point)
{
	struct can_bittiming bt;

	memset(&bt, 0, sizeof(bt));
	bt.bitrate = bitrate;
	bt.sample_point = sample_point;

	return can_set_bittiming(name, &bt);
}

/**
 * @ingroup extern
 * can_get_state - get the current state of the device
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param state pointer to store the state
 *
 * This one stores the current state of the can interface into the given
 * pointer. Valid states are:
 * - CAN_STATE_ERROR_ACTIVE
 * - CAN_STATE_ERROR_WARNING
 * - CAN_STATE_ERROR_PASSIVE
 * - CAN_STATE_BUS_OFF
 * - CAN_STATE_STOPPED
 * - CAN_STATE_SLEEPING
 *
 * The first four states is determined by the value of RX/TX error counter.
 * Please see relevant can specification for more information about this. A
 * device in STATE_STOPPED is an inactive device. STATE_SLEEPING is not
 * implemented on all devices.
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_get_state(const char *name, int *state)
{
	return get_link(name, GET_STATE, state);
}

/**
 * @ingroup extern
 * can_get_restart_ms - get the current interval of auto restarting.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param restart_ms pointer to store the value.
 *
 * This one stores the current interval of auto restarting into the given
 * pointer.
 *
 * The socketcan framework can automatically restart a device if it is in
 * bus_off in a given interval. This function gets this value in milliseconds.
 * restart_ms == 0 means that autorestarting is turned off.
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_get_restart_ms(const char *name, __u32 *restart_ms)
{
	return get_link(name, GET_RESTART_MS, restart_ms);
}

/**
 * @ingroup extern
 * can_get_bittiming - get the current bittimnig configuration.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bt pointer to the bittiming struct.
 *
 * This one stores the current bittiming configuration.
 *
 * Please see can_set_bittiming for more information about bit timing.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_bittiming(const char *name, struct can_bittiming *bt)
{
	return get_link(name, GET_BITTIMING, bt);
}

/**
 * @ingroup extern
 * can_get_ctrlmode - get the current control mode.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param cm pointer to the ctrlmode struct.
 *
 * This one stores the current control mode configuration.
 *
 * Please see can_set_ctrlmode for more information about control modes.
 *
 * @return 0 if success
 * @return -1 if failed
 */

int can_get_ctrlmode(const char *name, struct can_ctrlmode *cm)
{
	return get_link(name, GET_CTRLMODE, cm);
}

/**
 * @ingroup extern
 * can_get_clock - get the current clock struct.
 *
 * @param name: name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param clock pointer to the clock struct.
 *
 * This one stores the current clock configuration. At the time of writing the
 * can_clock struct only contains information about the clock frequecy. This
 * information is e.g. essential while setting up the bit timing.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_clock(const char *name, struct can_clock *clock)
{
	return get_link(name, GET_CLOCK, clock);
}

/**
 * @ingroup extern
 * can_get_bittiming_const - get the current bittimnig constant.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param btc pointer to the bittiming constant struct.
 *
 * This one stores the hardware dependent bittiming constant. The
 * can_bittiming_const struct consists:
 *
 * @code
 * struct can_bittiming_const {
 *	char name[16];
 *	__u32 tseg1_min;
 *	__u32 tseg1_max;
 *	__u32 tseg2_min;
 *	__u32 tseg2_max;
 *	__u32 sjw_max;
 *	__u32 brp_min;
 *	__u32 brp_max;
 *	__u32 brp_inc;
 *	};
 * @endcode
 *
 * The information in this struct is used to calculate the bus bit timing
 * automatically.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_bittiming_const(const char *name, struct can_bittiming_const *btc)
{
	return get_link(name, GET_BITTIMING_CONST, btc);
}


/**
 * @ingroup extern
 * can_get_berr_counter - get the tx/rx error counter.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bc pointer to the error counter struct..
 *
 * This one gets the current rx/tx error counter from the hardware.
 *
 * @code
 * struct can_berr_counter {
 *	__u16 txerr;
 *	__u16 rxerr;
 *	};
 * @endcode
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_berr_counter(const char *name, struct can_berr_counter *bc)
{
	return get_link(name, GET_BERR_COUNTER, bc);
}

/**
 * @ingroup extern
 * can_get_device_stats - get the can_device_stats.
 *
 * @param name name of the can device. This is the netdev name, as ifconfig -a shows
 * in your system. usually it contains prefix "can" and the numer of the can
 * line. e.g. "can0"
 * @param bc pointer to the error counter struct..
 *
 * This one gets the current can_device_stats.
 *
 * Please see struct can_device_stats for more information.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_device_stats(const char *name, struct can_device_stats *cds)
{
	return get_link(name, GET_XSTATS, cds);
}

/**
 * @ingroup extern
 * can_get_link_statistics - get RX/TX statistics (64 bits version)
 *
 * @param name name of the can device. This is the netdev name, as ip link shows
 * in your system. usually it contains prefix "can" and the number of the can
 * line. e.g. "can0"
 * @param rls pointer to the rtnl_link_stats64 struct which is from if_link.h.
 *
 * This one gets the current rtnl_link_stats64. Compares to the rtnl_link_stats,
 * The rtnl_link_stats64 is introduced since linux kernel 2.6. After that the
 * rtnl_link_stats is synchronous with struct rtnl_link_stats64 by truncating
 * each member from 64 bits to 32 bits. actually, the struct rtnl_link_stats
 * is kept for compatibility.
 *
 * Please see struct rtnl_link_stats64 (/usr/include/linux/if_link.h) for more
 * information.
 *
 * @return 0 if success
 * @return -1 if failed
 */
int can_get_link_stats(const char *name, struct rtnl_link_stats64 *rls)
{
	return get_link(name, GET_LINK_STATS, rls);
}
