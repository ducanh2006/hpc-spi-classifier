#include "stats.h"

#include <stdio.h>
#include <string.h>

#include <rte_cycles.h>

#include "common.h"

worker_stats_t g_worker_stats[MAX_WORKERS];
uint64_t       g_master_rx_packets        = 0;
uint64_t       g_master_rx_bytes          = 0;
uint64_t       g_master_dropped_packets   = 0;

static uint64_t last_time       = 0;
static uint64_t last_rx_packets = 0;
static uint64_t last_rx_bytes   = 0;

void stats_init(void)
{
	memset(g_worker_stats, 0, sizeof(g_worker_stats));
	g_master_rx_packets      = 0;
	g_master_rx_bytes        = 0;
	g_master_dropped_packets = 0;
	last_time                = rte_get_timer_cycles();
}

void stats_print_periodic(void)
{
	uint64_t current_time = rte_get_timer_cycles();
	uint64_t timer_hz     = rte_get_timer_hz();

	/* Print at most once per second */
	if (current_time - last_time < timer_hz)
		return;

	/*
	 * Take a lock-free snapshot of the active rule set so that rule
	 * names and count are consistent with each other during printing.
	 */
	const spi_rule_t *rules =
		atomic_load_explicit(&g_active_rules, memory_order_acquire);
	uint32_t num_rules =
		atomic_load_explicit(&g_active_num_rules, memory_order_acquire);

	uint64_t total_rx_pkts  = g_master_rx_packets;
	uint64_t total_rx_bytes = g_master_rx_bytes;
	uint64_t total_drop     = g_master_dropped_packets;

	uint64_t worker_rx_pkts  = 0;
	uint64_t worker_drop_pkts = 0;
	uint64_t rule_hits[MAX_RULES] = {0};

	for (int i = 0; i < MAX_WORKERS; i++) {
		worker_rx_pkts  += g_worker_stats[i].rx_packets;
		worker_drop_pkts += g_worker_stats[i].dropped_packets;
		for (uint32_t j = 0; j < num_rules; j++)
			rule_hits[j] += g_worker_stats[i].rule_hits[j];
	}

	uint64_t diff_pkts  = total_rx_pkts  - last_rx_packets;
	uint64_t diff_bytes = total_rx_bytes - last_rx_bytes;

	double pps  = (double)diff_pkts;
	double mbps = (double)diff_bytes * 8.0 / 1000000.0;

	double drop_rate = 0.0;
	if (total_rx_pkts > 0) {
		drop_rate = ((double)total_drop / total_rx_pkts) * 100.0;
	}

	int64_t missing_pkts = (int64_t)total_rx_pkts - ((int64_t)total_drop + (int64_t)worker_rx_pkts);
	double missing_rate = 0.0;
	if (total_rx_pkts > 0) {
		missing_rate = ((double)missing_pkts / total_rx_pkts) * 100.0;
	}

	printf("\n====================================================\n");
	printf("Throughput: %.2f Mbps | Flow Rate: %.0f pps\n", mbps, pps);
	printf("Master Rx: %lu pkts | Master Drop: %lu pkts\n",
	       total_rx_pkts, total_drop);
	printf("Worker Rx: %lu pkts | Worker Drop: %lu pkts\n",
	       worker_rx_pkts, worker_drop_pkts);
	printf("Packet Drop Rate: %.4f%% | Missing Packet Rate: %.4f%%\n",
	       drop_rate, missing_rate);

	printf("--- Rule Hits ---\n");
	for (uint32_t i = 0; i < num_rules; i++)
		printf("Rule [%s]: %lu hits\n", rules[i].name, rule_hits[i]);

	printf("====================================================\n");

	last_rx_packets = total_rx_pkts;
	last_rx_bytes   = total_rx_bytes;
	last_time       = current_time;
}
