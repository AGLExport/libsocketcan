#include <linux/can/gw.h>
#include <sys/socket.h>

#include "libsocketcan-utils.h"

enum {
	UNSPEC,
	ADD,
	DEL,
	FLUSH,
	LIST
};

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

int cangw_add_rule(void)
{
	struct {
		struct nlmsghdr nh;
		struct rtcanmsg rtcan;
		char buf[1500];
	} req;
	int err = 0;
	int s;
	unsigned int src_ifindex = 0;
	unsigned int dst_ifindex = 0;
	__u16 flags = 0;
	struct can_filter filter;
	struct sockaddr_nl nladdr;
	struct nlmsghdr *nlh;
	struct nlmsgerr *rte;
	unsigned char rxbuf[8192]; /* netlink receive buffer */

	memset(&req, 0, sizeof(req));

	flags |= CGW_FLAGS_CAN_ECHO;
	filter.can_id = 0x3C0;
	filter.can_mask = 0xff0;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.nh.nlmsg_type  = RTM_NEWROUTE;
	req.nh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtcanmsg));
	req.nh.nlmsg_seq   = 0;

	req.rtcan.can_family  = AF_CAN;
	req.rtcan.gwtype = CGW_TYPE_CAN_CAN;
	req.rtcan.flags = flags;

	src_ifindex = if_nametoindex("vcan0");
	dst_ifindex = if_nametoindex("vcan1");
	addattr_l(&req.nh, sizeof(req), CGW_SRC_IF, &src_ifindex, sizeof(src_ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_DST_IF, &dst_ifindex, sizeof(dst_ifindex));
	addattr_l(&req.nh, sizeof(req), CGW_FILTER, &filter, sizeof(filter));

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid    = 0;
	nladdr.nl_groups = 0;

	err = sendto(s, &req, req.nh.nlmsg_len, 0,
		(struct sockaddr*)&nladdr, sizeof(nladdr));
	if (err < 0) {
	perror("netlink sendto");
	return -1;
	}

	memset(rxbuf, 0x0, sizeof(rxbuf));
	err = recv(s, &rxbuf, sizeof(rxbuf), 0);
	if (err < 0) {
		perror("netlink recv");
		return err;
	}
	nlh = (struct nlmsghdr *)rxbuf;
	if (nlh->nlmsg_type != NLMSG_ERROR) {
		fprintf(stderr, "unexpected netlink answer of type %d\n", nlh->nlmsg_type);
		return -EINVAL;
	}
	rte = (struct nlmsgerr *)NLMSG_DATA(nlh);
	err = rte->error;
	if (err < 0)
		fprintf(stderr, "netlink error %d (%s)\n", err, strerror(abs(err)));

	close(s);

	return 0;
}
