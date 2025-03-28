/* libsocketcan-utils.h
 *
 * (C) 2009 Luotao Fu <l.fu@pengutronix.de>
 * (C) 2025 Naoto Yamaguchi <naoto.yamaguchi@aisin.co.jp>
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
#ifndef LIBSOCKETCAN_UTILS_H
#define LIBSOCKETCAN_UTILS_H

#ifdef HAVE_CONFIG_H
#include "libsocketcan_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>

#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#include <libsocketcan.h>

/* Define DISABLE_ERROR_LOG to disable printing of error messages to stderr. */
#ifdef DISABLE_ERROR_LOG
#define perror(x)				while (0) { perror(x); }
#define fprintf(stream, format, args...)	while (0) { fprintf(stream, format, ##args); }
#endif

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

#define IF_UP (1)
#define IF_DOWN (2)

#define GET_STATE (1)
#define GET_RESTART_MS (2)
#define GET_BITTIMING (3)
#define GET_CTRLMODE (4)
#define GET_CLOCK (5)
#define GET_BITTIMING_CONST (6)
#define GET_BERR_COUNTER (7)
#define GET_XSTATS (8)
#define GET_LINK_STATS (9)

struct req_info {
	__u8 restart;
	__u8 disable_autorestart;
	__u32 restart_ms;
	struct can_ctrlmode *ctrlmode;
	struct can_bittiming *bittiming;
	struct can_bittiming *dbittiming;
};

void parse_rtattr(struct rtattr **tb, int max, struct rtattr *rta, int len);
int addattr32(struct nlmsghdr *n, size_t maxlen, int type, __u32 data);
int addattr_l(struct nlmsghdr *n, size_t maxlen, int type, const void *data, int alen);
int send_mod_request(int fd, struct nlmsghdr *n);
int send_dump_request(int fd, const char *name, int family, int type);
int open_nl_sock();
int do_get_nl_link(int fd, __u8 acquire, const char *name, void *res);
int get_link(const char *name, __u8 acquire, void *res);
int do_set_nl_link(int fd, __u8 if_state, const char *name, struct req_info *req_info);
int set_link(const char *name, __u8 if_state, struct req_info *req_info);
#endif //#ifndef LIBSOCKETCAN_UTILS_H