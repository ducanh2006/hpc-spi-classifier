#include "worker.h"

#include <rte_lcore.h>
#include <rte_mbuf.h>

#include "common.h"
#include "matcher.h"
#include "parser.h"
#include "stats.h"

int worker_loop(void *arg)
{
	struct worker_params *p    = (struct worker_params *)arg;
	struct rte_ring      *ring = p->ring;
	uint32_t              w_id = p->worker_id;

	struct rte_mbuf *bufs[BURST_SIZE];

	printf("Worker %u started on lcore %u\n", w_id, rte_lcore_id());

	while (1) {
		uint16_t nb_rx = rte_ring_dequeue_burst(ring,
						(void **)bufs,
						BURST_SIZE, NULL);

		if (unlikely(nb_rx == 0)) continue;

		uint64_t w_rx_pkts            = 0;
		uint64_t w_rx_bytes           = 0;
		uint64_t w_drop_pkts          = 0;
		uint64_t local_rule_hits[MAX_RULES] = {0};

		/*
		 * Take a single lock-free snapshot of the active rule table
		 * for this burst.  All packets in the burst see a consistent
		 * rule set; the next burst may transparently see a new one
		 * after a hot-reload.
		 */
		const spi_rule_t *rules =
			atomic_load_explicit(&g_active_rules,
					     memory_order_acquire);

		for (uint16_t i = 0; i < nb_rx; i++) {
			if (likely(i + 4 < nb_rx)) {
				rte_prefetch0(bufs[i + 4]);
				rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 4],
							       void *));
			}

			struct rte_mbuf *m = bufs[i];
			w_rx_pkts++;
			w_rx_bytes += rte_pktmbuf_pkt_len(m);

			five_tuple_t tuple;
			if (likely(parse_five_tuple(m, &tuple))) {
				int rule_idx = match_rule(&tuple);
				if (rule_idx >= 0) {
					local_rule_hits[rule_idx]++;

					/* Use the burst-local pointer — same
					 * snapshot, avoids a second atomic load */
					if (rules[rule_idx].action_mask ==
					    ACTION_DROP)
						w_drop_pkts++;
				} else {
					/* No match — default drop */
					w_drop_pkts++;
				}
			} else {
				/* Non-IPv4/TCP/UDP — drop */
				w_drop_pkts++;
			}
		}

		/* Flush local counters to per-worker stats (non-atomic add
		 * is safe: only this lcore writes to its own slot) */
		g_worker_stats[w_id].rx_packets  += w_rx_pkts;
		g_worker_stats[w_id].rx_bytes    += w_rx_bytes;
		g_worker_stats[w_id].dropped_packets += w_drop_pkts;

		uint32_t num_rules =
			atomic_load_explicit(&g_active_num_rules,
					     memory_order_relaxed);
		for (uint32_t i = 0; i < num_rules; i++)
			g_worker_stats[w_id].rule_hits[i] +=
				local_rule_hits[i];

		/* Batch memory free — single call amortises overhead */
		rte_pktmbuf_free_bulk(bufs, nb_rx);
	}

	return 0;
}
