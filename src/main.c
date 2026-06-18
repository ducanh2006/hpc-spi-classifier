#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>

#include "common.h"
#include "control.h"
#include "master.h"
#include "matcher.h"
#include "stats.h"
#include "worker.h"

/* MIN_MBUFS = rxd(1024) + txd(1024) + workers*RING_SIZE(4*4096) + lcores*BURST_SIZE*2(5*64*2)
 * = 1024 + 1024 + 16384 + 640 = 19072. Use 32767 (2^15-1) for a safe 1.7x margin.
 * MBUF_CACHE_SIZE: power-of-2 <= RTE_MEMPOOL_CACHE_MAX_SIZE(512) and <= NUM_MBUFS/1.5 */
#define NUM_MBUFS       32767
#define MBUF_CACHE_SIZE 256

static const char      *rule_file = "spi_rules.conf";
static struct rte_ring  *worker_rings[MAX_WORKERS];
static struct worker_params w_params[MAX_WORKERS];

volatile bool force_quit = false;

static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n", signum);
		force_quit = true;
	}
}

static int parse_args(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "r:")) != EOF) {
		switch (opt) {
		case 'r':
			rule_file = optarg;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	if (parse_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid application arguments\n");

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Load the initial rule set into table_a and activate it */
	if (matcher_init(rule_file) < 0)
		rte_exit(EXIT_FAILURE, "Failed to initialize matcher\n");

	stats_init();

	/* Start the control plane thread before the data path so that
	 * the socket is ready as soon as the application is reachable */
	if (control_thread_start() < 0)
		rte_exit(EXIT_FAILURE,
			 "Failed to start control thread\n");

	struct rte_mempool *mbuf_pool =
		rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
					MBUF_CACHE_SIZE,
					RTE_ALIGN(sizeof(pkt_metadata_t), RTE_MBUF_PRIV_ALIGN),
					RTE_MBUF_DEFAULT_BUF_SIZE,
					rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	uint16_t portid = 0;
	if (rte_eth_dev_count_avail() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_lro_pkt_size = RTE_ETHER_MAX_LEN,
		},
	};

	ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			 "Cannot configure device: err=%d, port=%u\n",
			 ret, portid);

	ret = rte_eth_rx_queue_setup(portid, 0, 1024,
				     rte_eth_dev_socket_id(portid),
				     NULL, mbuf_pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			 "rx queue setup failed: err=%d, port=%u\n",
			 ret, portid);

	ret = rte_eth_tx_queue_setup(portid, 0, 64,
				     rte_eth_dev_socket_id(portid), NULL);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			 "tx queue setup failed: err=%d, port=%u\n",
			 ret, portid);

	ret = rte_eth_dev_start(portid);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			 "rte_eth_dev_start: err=%d, port=%u\n",
			 ret, portid);

	uint32_t num_workers = 0;
	uint32_t lcore_id;

	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (num_workers >= MAX_WORKERS) {
			printf("Warning: More lcores than MAX_WORKERS (%d),"
			       " ignoring lcore %u\n", MAX_WORKERS, lcore_id);
			continue;
		}

		char ring_name[32];
		snprintf(ring_name, sizeof(ring_name),
			 "WORKER_RING_%u", num_workers);

		worker_rings[num_workers] =
			rte_ring_create(ring_name, RING_SIZE,
					rte_socket_id(),
					RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (worker_rings[num_workers] == NULL)
			rte_exit(EXIT_FAILURE,
				 "Cannot create ring %s\n", ring_name);

		w_params[num_workers].ring      = worker_rings[num_workers];
		w_params[num_workers].worker_id = num_workers;

		rte_eal_remote_launch(worker_loop,
				      &w_params[num_workers], lcore_id);
		num_workers++;
	}

	if (num_workers == 0)
		rte_exit(EXIT_FAILURE,
			 "Need at least 1 worker core (e.g. -l 0-1)\n");

	master_loop(worker_rings, num_workers, portid);

	rte_eal_mp_wait_lcore();

	/* Graceful control thread shutdown */
	control_thread_stop();

	return 0;
}
