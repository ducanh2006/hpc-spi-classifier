#include "worker.h"
#include "common.h"
#include "parser.h"
#include "matcher.h"
#include "stats.h"
#include <rte_lcore.h>
#include <rte_mbuf.h>

int worker_loop(void *arg)
{
	struct worker_params *p = (struct worker_params *)arg;
	struct rte_ring *ring = p->ring;
	uint32_t w_id = p->worker_id;
	
	struct rte_mbuf *bufs[BURST_SIZE];
	struct rte_mbuf *free_bufs[BURST_SIZE];
	
	printf("Worker %u started on lcore %u\n", w_id, rte_lcore_id());
	
	while (1) {
		uint16_t nb_rx = rte_ring_dequeue_burst(ring, (void **)bufs, BURST_SIZE, NULL);
		
		if (unlikely(nb_rx == 0)) continue;
		
		uint16_t free_idx = 0;
		uint64_t w_rx_pkts = 0;
		uint64_t w_rx_bytes = 0;
		uint64_t w_drop_pkts = 0;
		uint64_t local_rule_hits[MAX_RULES] = {0};
		
		for (uint16_t i = 0; i < nb_rx; i++) {
			struct rte_mbuf *m = bufs[i];
			w_rx_pkts++;
			w_rx_bytes += rte_pktmbuf_pkt_len(m);
			
			five_tuple_t tuple;
			if (likely(parse_five_tuple(m, &tuple))) {
				int rule_idx = match_rule(&tuple);
				if (rule_idx >= 0) {
					local_rule_hits[rule_idx]++;
					
					if (g_rules[rule_idx].action_mask == ACTION_DROP) {
						w_drop_pkts++;
					}
				} else {
					// No match, assume drop
					w_drop_pkts++;
				}
			} else {
				// Not IPv4/TCP/UDP
				w_drop_pkts++;
			}
			
			// Accumulate for batch freeing
			free_bufs[free_idx++] = m;
		}
		
		// Update stats
		g_worker_stats[w_id].rx_packets += w_rx_pkts;
		g_worker_stats[w_id].rx_bytes += w_rx_bytes;
		g_worker_stats[w_id].dropped_packets += w_drop_pkts;
		for (uint32_t i = 0; i < g_num_rules; i++) {
			g_worker_stats[w_id].rule_hits[i] += local_rule_hits[i];
		}
		
		// Batching Memory Free
		if (likely(free_idx > 0)) {
			rte_pktmbuf_free_bulk(free_bufs, free_idx);
		}
	}
	
	return 0;
}
