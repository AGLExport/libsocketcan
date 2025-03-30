#include <linux/can/gw.h>
#include <sys/socket.h>

#include "libsocketcan-utils.h"

#include <libsocketcangw.h>

struct modattr {
	struct can_frame cf;
	__u8 modtype;
	__u8 instruction;
} __attribute__((packed));

struct fdmodattr {
	struct canfd_frame cf;
	__u8 modtype;
	__u8 instruction;
} __attribute__((packed));

struct s_request_data {
	struct nlmsghdr nh;
	struct rtcanmsg rtcan;
	char buf[1500];
};

#define RTCAN_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct rtcanmsg))))
#define RTCAN_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct rtcanmsg))

static int send_cangw_set_request(struct s_request_data *req)
{
	int result = 0;
	int sock_fd = -1;
	ssize_t ret = -1;
	struct nlmsghdr *nlh = NULL;
	struct nlmsgerr *rte = NULL;
	struct sockaddr_nl nladdr;
	unsigned char rxbuf[8192];

	// Open netlink socket interface
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock_fd < 0) {
		result = -1;
		goto do_return;
	}

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid    = 0;
	nladdr.nl_groups = 0;

	ret = sendto(sock_fd, req, req->nh.nlmsg_len, 0, (struct sockaddr*)&nladdr, sizeof(nladdr));
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	memset(rxbuf, 0, sizeof(rxbuf));
	ret = recv(sock_fd, &rxbuf, sizeof(rxbuf), 0);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	nlh = (struct nlmsghdr *)rxbuf;
	if (nlh->nlmsg_type != NLMSG_ERROR) {
		result = -2;
		goto do_return;
	}

	rte = (struct nlmsgerr *)NLMSG_DATA(nlh);
	if (rte->error < 0) {
		result = -3;
	}

do_return:
	if (sock_fd >= 0) {
		close(sock_fd);
	}

	return result;
}

static int parse_rtlist(unsigned char *rxbuf, int len)
{
	char src_ifname[IF_NAMESIZE]; /* interface name for if_indextoname() */
	char dst_ifname[IF_NAMESIZE]; /* interface name for if_indextoname() */
	struct rtcanmsg *rtc;
	struct rtattr *rta;
	struct nlmsghdr *nlh;
	int rtlen;
	socketcan_gw_rule_t rule;

	nlh = (struct nlmsghdr *)rxbuf;

	while (1) {
		printf("HOGE:");
		if (!NLMSG_OK(nlh, len)){
			printf("!NLMSG_OK\n");
			return 0;
		}

		if (nlh->nlmsg_type == NLMSG_ERROR) {
			printf("NLMSG_ERROR\n");
			return 1;
		}

		if (nlh->nlmsg_type == NLMSG_DONE) {
			printf("NLMSG_DONE\n");
			return 1;
		}

		memset(&rule, 0 ,sizeof(rule));
		rtc = (struct rtcanmsg *)NLMSG_DATA(nlh);
		if (rtc->can_family != AF_CAN) {
			printf("received msg from unknown family %d\n", rtc->can_family);
			return -EINVAL;
		}

		if (rtc->gwtype != CGW_TYPE_CAN_CAN) {
			printf("received msg with unknown gwtype %d\n", rtc->gwtype);
			return -EINVAL;
		}

		/* first parse for mandatory options */
		rta = (struct rtattr *) RTCAN_RTA(rtc);
		rtlen = RTCAN_PAYLOAD(nlh);
		for(;RTA_OK(rta, rtlen);rta=RTA_NEXT(rta,rtlen))
		{
			switch(rta->rta_type) {
			case CGW_SRC_IF:
				rule.src_ifindex = (*(unsigned int*)RTA_DATA(rta));
				break;

			case CGW_DST_IF:
				rule.dst_ifindex = (*(unsigned int*)RTA_DATA(rta));
				break;
			default:
				break;
			}
		}

		rule.options = (SOCKETCAN_GW_RULE_ECHO | SOCKETCAN_GW_RULE_FILTER);

		if ((rtc->flags & CGW_FLAGS_CAN_ECHO) == CGW_FLAGS_CAN_ECHO) {
			rule.options = 1;
		} else {
			rule.options = 0;
		}

		/* second parse for mod attributes */
		rta = (struct rtattr *) RTCAN_RTA(rtc);
		rtlen = RTCAN_PAYLOAD(nlh);
		for(;RTA_OK(rta, rtlen);rta=RTA_NEXT(rta,rtlen))
		{
			switch(rta->rta_type) {
			case CGW_FILTER:
			{
				struct can_filter *filter = (struct can_filter *)RTA_DATA(rta);
				if (filter->can_id & CAN_INV_FILTER) {
					rule.filter.can_id = (filter->can_id & ~CAN_INV_FILTER);
				} else {
					rule.filter.can_id = filter->can_id;
				}
				rule.filter.can_mask = filter->can_mask;
				break;
			}

			default:
				break;
			}
		}
		/* end of entry */

		printf("cangw: -s %s -d %s -f %03X:%X\n", 
			if_indextoname(rule.src_ifindex, src_ifname), 
			if_indextoname(rule.dst_ifindex, dst_ifname),
			rule.filter.can_id,
			rule.filter.can_mask);

		/* jump to next NLMSG in the given buffer */
		nlh = NLMSG_NEXT(nlh, len);
	}
}

static int send_cangw_get_request(struct s_request_data *req)
{
	int result = 0;
	int sock_fd = -1;
	ssize_t ret = -1;
	struct sockaddr_nl nladdr;
	unsigned char rxbuf[8192];

	// Open netlink socket interface
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock_fd < 0) {
		result = -1;
		goto do_return;
	}

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid    = 0;
	nladdr.nl_groups = 0;

	ret = sendto(sock_fd, req, req->nh.nlmsg_len, 0, (struct sockaddr*)&nladdr, sizeof(nladdr));
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	memset(rxbuf, 0, sizeof(rxbuf));
	while (1) {
		ret = recv(sock_fd, &rxbuf, sizeof(rxbuf), 0);
		if (ret < 0) {
			result = -1;
			goto do_return;
		}

		/* leave on errors or NLMSG_DONE */
		if (parse_rtlist(rxbuf, ret))
			break;
	}

do_return:
	if (sock_fd >= 0) {
		close(sock_fd);
	}

	return result;
}
/**
 * @ingroup extern
 * cangw_add_rule - add routing rule to can gateway
 * @param rule rule structure of the can gateway.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 * @return -3 if argument is invalid
 */
int cangw_add_rule(socketcan_gw_rule_t *rule)
{
	int result = 0;
	int ret = -1;
	struct s_request_data req;

	if (rule == NULL) {
		result = -3;
		goto do_return;
	}

	// Setup common message
	memset(&req, 0, sizeof(req));

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_type  = RTM_NEWROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = 0;

	if ((rule->src_ifindex == 0) || (rule->dst_ifindex == 0)) {
		// invalid ifindex
		result = -3;
		goto do_return;
	}
	addattr_l(&req.nh, sizeof(req), CGW_SRC_IF, &rule->src_ifindex, sizeof(rule->src_ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_DST_IF, &rule->dst_ifindex, sizeof(rule->dst_ifindex));

	// Echo option
	if ((rule->options | SOCKETCAN_GW_RULE_ECHO) == SOCKETCAN_GW_RULE_ECHO) {
		if (rule->echo == 1) {
			req.rtcan.flags |= CGW_FLAGS_CAN_ECHO;
		}
	}

	if ((rule->options | SOCKETCAN_GW_RULE_FILTER) == SOCKETCAN_GW_RULE_FILTER) {
		addattr_l(&req.nh, sizeof(req), CGW_FILTER, &rule->filter, sizeof(struct can_filter));
	}

	ret = send_cangw_set_request(&req);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

do_return:
	return result;
}

/**
 * @ingroup extern
 * cangw_delete_rule - delete routing rule to can gateway
 * @param rule rule structure of the can gateway.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 * @return -3 if argument is invalid
 */
int cangw_delete_rule(socketcan_gw_rule_t *rule)
{
	int result = 0;
	int ret = -1;
	struct s_request_data req;

	if (rule == NULL) {
		result = -3;
		goto do_return;
	}

	// Setup common message
	memset(&req, 0, sizeof(req));

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_type  = RTM_DELROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = 0;

	if ((rule->src_ifindex == 0) || (rule->dst_ifindex == 0)) {
		// invalid ifindex
		result = -3;
		goto do_return;
	}
	addattr_l(&req.nh, sizeof(req), CGW_SRC_IF, &rule->src_ifindex, sizeof(rule->src_ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_DST_IF, &rule->dst_ifindex, sizeof(rule->dst_ifindex));

	// Echo option
	if ((rule->options | SOCKETCAN_GW_RULE_ECHO) == SOCKETCAN_GW_RULE_ECHO) {
		if (rule->echo == 1) {
			req.rtcan.flags |= CGW_FLAGS_CAN_ECHO;
		}
	}

	if ((rule->options | SOCKETCAN_GW_RULE_FILTER) == SOCKETCAN_GW_RULE_FILTER) {
		addattr_l(&req.nh, sizeof(req), CGW_FILTER, &rule->filter, sizeof(struct can_filter));
	}

	ret = send_cangw_set_request(&req);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

do_return:
	return result;
}

/**
 * @ingroup extern
 * cangw_clean_rule - delete routing rule to can gateway
 * @param src_ifindex interface index of routing source.
 * @param dst_ifindex interface index of routing destination.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 */
int cangw_clean_rule(unsigned int src_ifindex, unsigned int dst_ifindex)
{
	int result = 0;
	int ret = -1;
	struct s_request_data req;

	// Setup common message
	memset(&req, 0, sizeof(req));

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_type  = RTM_DELROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = 0;

	addattr_l(&req.nh, sizeof(req), CGW_SRC_IF, &src_ifindex, sizeof(src_ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_DST_IF, &dst_ifindex, sizeof(dst_ifindex));

	ret = send_cangw_set_request(&req);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

do_return:
	return result;
}

/**
 * @ingroup extern
 * cangw_delete_rule - delete routing rule to can gateway
 * @param rule rule structure of the can gateway.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 * @return -3 if argument is invalid
 */
int cangw_get_rules(socketcan_gw_rule_t **rules, size_t *rule_num)
{
	int result = 0;
	int ret = -1;
	struct s_request_data req;

	if ((rules == NULL) || (rule_num == NULL)) {
		result = -3;
		goto do_return;
	}

	// Setup common message
	memset(&req, 0, sizeof(req));

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nh.nlmsg_type  = RTM_GETROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = 0;

	ret = send_cangw_get_request(&req);
	if (ret < 0) {
		result = -1;
		goto do_return;
	}




do_return:
	return result;
}