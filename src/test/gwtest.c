#include "../libsocketcan-utils.c"
#include "../libcangw.c"


int cangw_add_rule_test(void)
{
	int ret = -1;
	socketcan_gw_rule_t gw_rule;
	socketcan_gw_rules_t *gw_rules = NULL;	

	memset(&gw_rule, 0, sizeof(gw_rule));

	gw_rule.src_ifindex = if_nametoindex("vcan0");
	gw_rule.dst_ifindex = if_nametoindex("vxcan0");

	gw_rule.options |= SOCKETCAN_GW_RULE_ECHO;
	gw_rule.echo = 0;

	gw_rule.options |= SOCKETCAN_GW_RULE_FILTER;
	gw_rule.filter.can_id = 0x3C0;
	gw_rule.filter.can_mask = 0xff0;

	ret = cangw_add_rule(&gw_rule);
	if (ret < 0) {
		fprintf(stdout,"cangw_add_rule is failed ret = %d\n",ret);
		return -1;
	}

	ret = cangw_get_rules(&gw_rules);
	if (ret < 0) {
		fprintf(stdout,"cangw_get_rules is failed ret = %d\n",ret);
		return -4;
	}
	cangw_release_rules(gw_rules);
	fprintf(stdout,"\n");

	ret = cangw_delete_rule(&gw_rule);
	if (ret < 0) {
		fprintf(stdout,"cangw_delete_rule is failed ret = %d\n",ret);
		return -2;
	}

	ret = cangw_get_rules(&gw_rules);
	if (ret < 0) {
		fprintf(stdout,"cangw_get_rules is failed ret = %d\n",ret);
		return -4;
	}
	cangw_release_rules(gw_rules);
	fprintf(stdout,"\n");

	ret = cangw_add_rule(&gw_rule);
	if (ret < 0) {
		fprintf(stdout,"cangw_add_rule is failed ret = %d\n",ret);
		return -3;
	}

	ret = cangw_clean_rule();
	if (ret < 0) {
		fprintf(stdout,"cangw_clean_rule is failed ret = %d\n",ret);
		return -4;
	}

	ret = cangw_get_rules(&gw_rules);
	if (ret < 0) {
		fprintf(stdout,"cangw_get_rules is failed ret = %d\n",ret);
		return -4;
	}
	cangw_release_rules(gw_rules);
	fprintf(stdout,"\n");

	ret = cangw_add_rule(&gw_rule);
	if (ret < 0) {
		fprintf(stdout,"cangw_add_rule is failed ret = %d\n",ret);
		return -3;
	}

	ret = cangw_get_rules(&gw_rules);
	if (ret < 0) {
		fprintf(stdout,"cangw_get_rules is failed ret = %d\n",ret);
		return -4;
	}
	cangw_release_rules(gw_rules);
	fprintf(stdout,"\n");

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	ret = cangw_add_rule_test();
	fprintf(stdout,"ret = %d\n",ret);

	return 0;
}