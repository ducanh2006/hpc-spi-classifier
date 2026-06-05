#include "stats.h"
#include <stdio.h>
#include <string.h>
#include <rte_cycles.h>

worker_stats_t g_worker_stats[MAX_WORKERS];
uint64_t g_master_rx_packets = 0;
uint64_t g_master_rx_bytes = 0;
uint64_t g_master_dropped_packets = 0;

static uint64_t last_time = 0;
static uint64_t last_rx_packets = 0;
static uint64_t last_rx_bytes = 0;

void stats_init(void)
{
	memset(g_worker_stats, 0, sizeof(g_worker_stats));
	g_master_rx_packets = 0;
	g_master_rx_bytes = 0;
	g_master_dropped_packets = 0;
	last_time = rte_get_timer_cycles();
}

void stats_print_periodic(void)
{
	uint64_t current_time = rte_get_timer_cycles();
	uint64_t timer_hz = rte_get_timer_hz();
	
	// Print every 1 second
	if (current_time - last_time >= timer_hz) {
		uint64_t total_rx_pkts = g_master_rx_packets;
		uint64_t total_rx_bytes = g_master_rx_bytes;
		uint64_t total_drop = g_master_dropped_packets;
		
		uint64_t worker_rx_pkts = 0;
		uint64_t worker_drop_pkts = 0;
		uint64_t rule_hits[MAX_RULES] = {0};
		
		for (int i = 0; i < MAX_WORKERS; i++) {
			worker_rx_pkts += g_worker_stats[i].rx_packets;
			worker_drop_pkts += g_worker_stats[i].dropped_packets;
			for (uint32_t j = 0; j < g_num_rules; j++) {
				rule_hits[j] += g_worker_stats[i].rule_hits[j];
			}
		}
		
		uint64_t diff_pkts = total_rx_pkts - last_rx_packets;
		uint64_t diff_bytes = total_rx_bytes - last_rx_bytes;
		
		double pps = (double)diff_pkts;
		double mbps = (double)diff_bytes * 8 / 1000000.0;
		
		printf("\n====================================================\n");
		printf("Throughput: %.2f Mbps | Flow Rate: %.0f pps\n", mbps, pps);
		printf("Master Rx: %lu pkts | Master Drop: %lu pkts\n", total_rx_pkts, total_drop);
		printf("Worker Rx: %lu pkts | Worker Drop: %lu pkts\n", worker_rx_pkts, worker_drop_pkts);
		
		printf("--- Rule Hits ---\n");
		for (uint32_t i = 0; i < g_num_rules; i++) {
			printf("Rule [%s]: %lu hits\n", g_rules[i].name, rule_hits[i]);
		}
		printf("====================================================\n");
		
		last_rx_packets = total_rx_pkts;
		last_rx_bytes = total_rx_bytes;
		last_time = current_time;
	}
}
