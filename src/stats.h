#pragma once

#include <stdint.h>
#include <rte_common.h>
#include "common.h"

// Statistics for each worker thread, cache-line aligned to prevent false sharing
typedef struct {
	uint64_t rx_packets;
	uint64_t rx_bytes;
	uint64_t tx_packets;
	uint64_t dropped_packets;
	uint64_t rule_hits[MAX_RULES];
} __rte_cache_aligned worker_stats_t;

// Global array for per-worker stats
extern worker_stats_t g_worker_stats[MAX_WORKERS];

// Master core statistics
extern uint64_t g_master_rx_packets;
extern uint64_t g_master_rx_bytes;
extern uint64_t g_master_dropped_packets;

void stats_init(void);
void stats_print_periodic(void);
