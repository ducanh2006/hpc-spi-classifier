#include <stdio.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>

static int
lcore_hello(__rte_unused void *arg)
{
	unsigned int lcore_id;

	lcore_id = rte_lcore_id();
	printf("hello from core %u\n", lcore_id);
	return 0;
}

int
main(int argc, char *argv[])
{
	unsigned int lcore_id;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Cannot init EAL\n");
	}

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}

	lcore_hello(NULL);

	rte_eal_mp_wait_lcore();
	rte_eal_cleanup();

	return 0;
}
/*
from /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier run:

1)
g++ -O3 src/helloworld.cpp -o build_helloworld $(PKG_CONFIG_PATH=/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/dpdk-24.11/build/meson-uninstalled pkg-config --cflags --libs libdpdk-uninstalled)

2)
LD_LIBRARY_PATH=/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/dpdk-24.11/build/lib ./build_helloworld --no-huge -m 128
*/