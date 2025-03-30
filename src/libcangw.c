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

static int push_gw_rule(socketcan_gw_rules_t *gw_rules, socketcan_gw_rule_t *rule)
{
	int result = 0;

	if (gw_rules->rules == NULL) {
		// Create rules array
		gw_rules->rule_num = 0;	// Initial size
		gw_rules->array_num = 2;	// Initial size
		gw_rules->rules = (socketcan_gw_rule_t**)malloc(sizeof(socketcan_gw_rule_t*) * gw_rules->array_num);
		if (gw_rules->rules == NULL) {
			result = -1;
			goto do_return;
		}
	}

	if (!(gw_rules->rule_num < gw_rules->array_num)) {
		// Extend array
		socketcan_gw_rule_t **pnew_rules = NULL;
		gw_rules->array_num = gw_rules->array_num * 2;
		pnew_rules = (socketcan_gw_rule_t**)realloc(gw_rules->rules, (sizeof(socketcan_gw_rule_t*) * gw_rules->array_num));
		if (pnew_rules != NULL) {
			gw_rules->rules = pnew_rules;
		} else {
			result = -1;
			goto do_return;
		}
	}

	gw_rules->rules[gw_rules->rule_num] = rule;
	gw_rules->rule_num = gw_rules->rule_num + 1;

do_return:
	return result;
}

static int free_gw_rules(socketcan_gw_rules_t *gw_rules)
{
	for(size_t i=0; i < gw_rules->rule_num; i++) {
		free(gw_rules->rules[i]);
		gw_rules->rules[i] = NULL;
	}

	free(gw_rules->rules);
	gw_rules->rules = NULL;
	gw_rules->rule_num = 0;
	gw_rules->array_num = 0;

	return 0;
}

static int print_gw_rules(socketcan_gw_rules_t *gw_rules)
{
	char src_ifname[IF_NAMESIZE]; /* interface name for if_indextoname() */
	char dst_ifname[IF_NAMESIZE]; /* interface name for if_indextoname() */

	for(size_t i=0; i < gw_rules->rule_num; i++) {
		socketcan_gw_rule_t *rule = gw_rules->rules[i];

		fprintf(stdout, "cangw: -s %s -d %s -f %03X:%X\n", 
			if_indextoname(rule->src_ifindex, src_ifname), 
			if_indextoname(rule->dst_ifindex, dst_ifname),
			rule->filter.can_id,
			rule->filter.can_mask);
	}

	return 0;
}

static int parse_listing_data(socketcan_gw_rules_t *gw_rules, unsigned char *rxbuf, int len)
{
	struct rtcanmsg *rtc;
	struct rtattr *rta;
	struct nlmsghdr *nlh;
	int rtlen;
	socketcan_gw_rule_t *rule = NULL;
	int result = 0;

	nlh = (struct nlmsghdr *)rxbuf;

	while (1) {
		if (!NLMSG_OK(nlh, len)){
			result = 0;
			break;
		}

		if (nlh->nlmsg_type == NLMSG_ERROR) {
			result = -1;
			break;
		}

		if (nlh->nlmsg_type == NLMSG_DONE) {
			result = 1;
			break;
		}

		rtc = (struct rtcanmsg *)NLMSG_DATA(nlh);
		if (rtc->can_family != AF_CAN) {
			result = -1;
			break;
		}

		if (rtc->gwtype != CGW_TYPE_CAN_CAN) {
			result = -1;
			break;
		}

		rule = (socketcan_gw_rule_t*)malloc(sizeof(socketcan_gw_rule_t));
		if (rule == NULL) {
			result = -2;
			goto error_return;
		}
		memset(rule, 0 ,sizeof(socketcan_gw_rule_t));

		rta = (struct rtattr *) RTCAN_RTA(rtc);
		rtlen = RTCAN_PAYLOAD(nlh);
		for(; RTA_OK(rta, rtlen); rta=RTA_NEXT(rta,rtlen)) {
			switch(rta->rta_type) {
			case CGW_SRC_IF:
				rule->src_ifindex = (*(unsigned int*)RTA_DATA(rta));
				break;
			case CGW_DST_IF:
				rule->dst_ifindex = (*(unsigned int*)RTA_DATA(rta));
				break;
			default:
				break;
			}
		}

		rule->options = (SOCKETCAN_GW_RULE_ECHO | SOCKETCAN_GW_RULE_FILTER);

		if ((rtc->flags & CGW_FLAGS_CAN_ECHO) == CGW_FLAGS_CAN_ECHO) {
			rule->options = 1;
		} else {
			rule->options = 0;
		}

		rta = (struct rtattr *) RTCAN_RTA(rtc);
		rtlen = RTCAN_PAYLOAD(nlh);
		for(; RTA_OK(rta, rtlen); rta=RTA_NEXT(rta,rtlen)) {
			switch(rta->rta_type) {
			case CGW_FILTER:
			{
				struct can_filter *filter = (struct can_filter *)RTA_DATA(rta);
				if (filter->can_id & CAN_INV_FILTER) {
					rule->filter.can_id = (filter->can_id & ~CAN_INV_FILTER);
				} else {
					rule->filter.can_id = filter->can_id;
				}
				rule->filter.can_mask = filter->can_mask;
				break;
			}

			default:
				break;
			}
		}
		/* end of entry */

		push_gw_rule(gw_rules, rule);

		/* jump to next NLMSG in the given buffer */
		nlh = NLMSG_NEXT(nlh, len);
	}

	return result;

error_return:
	free(rule);
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
	if ((rule->options & SOCKETCAN_GW_RULE_ECHO) == SOCKETCAN_GW_RULE_ECHO) {
		if (rule->echo == 1) {
			req.rtcan.flags |= CGW_FLAGS_CAN_ECHO;
		}
	}

	if ((rule->options & SOCKETCAN_GW_RULE_FILTER) == SOCKETCAN_GW_RULE_FILTER) {
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
	if ((rule->options & SOCKETCAN_GW_RULE_ECHO) == SOCKETCAN_GW_RULE_ECHO) {
		if (rule->echo == 1) {
			req.rtcan.flags |= CGW_FLAGS_CAN_ECHO;
		}
	}

	if ((rule->options & SOCKETCAN_GW_RULE_FILTER) == SOCKETCAN_GW_RULE_FILTER) {
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
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 */
int cangw_clean_rule(void)
{
	int result = 0;
	int ret = -1;
	unsigned int ifindex = 0;
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

	// If src and dst ifindex set to 0, the all rule are deleted.
	addattr_l(&req.nh, sizeof(req), CGW_SRC_IF, &ifindex, sizeof(ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_DST_IF, &ifindex, sizeof(ifindex));

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
 * @param gw_rules rule structure of the can gateway.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 * @return -3 if argument is invalid
 */
int cangw_get_rules(socketcan_gw_rules_t **gw_rules)
{
	int result = 0;
	int sock_fd = -1;
	ssize_t ret = -1;
	struct s_request_data req;
	struct sockaddr_nl nladdr;
	socketcan_gw_rules_t *pgw_rules = NULL;

	if (gw_rules == NULL) {
		result = -3;
		goto do_return;
	}

	memset(&req, 0, sizeof(req));

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nh.nlmsg_type  = RTM_GETROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = 0;

	// Open netlink socket interface
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock_fd < 0) {
		result = -2;
		goto do_return;
	}

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid    = 0;
	nladdr.nl_groups = 0;

	ret = sendto(sock_fd, &req, req.nh.nlmsg_len, 0, (struct sockaddr*)&nladdr, sizeof(nladdr));
	if (ret < 0) {
		result = -1;
		goto do_return;
	}

	pgw_rules = malloc(sizeof(socketcan_gw_rules_t));
	if (pgw_rules == NULL) {
		result = -1;
		goto do_return;
	}

	pgw_rules->rule_num = 0;
	pgw_rules->array_num = 0;
	pgw_rules->rules = NULL;

	while (1) {
		unsigned char rxbuf[8192];
		memset(rxbuf, 0, sizeof(rxbuf));

		ret = recv(sock_fd, &rxbuf, sizeof(rxbuf), 0);
		if (ret < 0) {
			result = -1;
			goto do_return;
		}

		/* leave on errors or NLMSG_DONE */
		if (parse_listing_data(pgw_rules, rxbuf, ret))
			break;
	}
	print_gw_rules(pgw_rules);
	(*gw_rules) = pgw_rules;

do_return:
	if (sock_fd >= 0) {
		close(sock_fd);
	}

	return result;
}
/**
 * @ingroup extern
 * cangw_delete_rule - delete routing rule to can gateway
 * @param gw_rules rule structure of the can gateway.
 *
 * @return 0 if success
 * @return -1 if operation is failed
 * @return -2 if linux does not support can gateway
 * @return -3 if argument is invalid
 */
int cangw_release_rules(socketcan_gw_rules_t *gw_rules)
{
	int result = 0;

	if (gw_rules == NULL) {
		result = -3;
		goto do_return;
	}

	(void) free_gw_rules(gw_rules);
	(void) free(gw_rules);

do_return:
	return result;
}