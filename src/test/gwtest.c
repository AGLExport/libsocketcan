#include "../libsocketcan-utils.c"
#include "../libcangw.c"


int main(int argc, char *argv[])
{
	int ret = -1;
	ret = cangw_add_rule();
	fprintf(stdout,"ret = %d\n",ret);

	return 0;
}