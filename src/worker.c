#include "worker.h"

#include <rte_acl.h>
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

#ifdef DEBUG_MODE
	FILE *debug_fp = NULL;
	if (w_id == 0) {
		debug_fp = fopen("tests/results/actual.csv", "w");
		if (debug_fp) {
			fprintf(debug_fp, "Packet_Index,Rule,Action\n");
		}
	}
#endif

	printf("Worker %u started on lcore %u\n", w_id, rte_lcore_id());

	while (!force_quit) {
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
		 * and active ACL context for this burst.
		 */
		const spi_rule_t *rules =
			atomic_load_explicit(&g_active_rules,
					     memory_order_acquire);
		uint32_t num_rules =
			atomic_load_explicit(&g_active_num_rules,
					     memory_order_acquire);
		struct rte_acl_ctx *acl_ctx =
			atomic_load_explicit(&g_active_acl_ctx,
					     memory_order_acquire);

		const uint8_t *data_ptrs[BURST_SIZE];
		uint32_t results[BURST_SIZE];
#ifdef DEBUG_MODE
		uint16_t valid_indices[BURST_SIZE];
#endif
		uint32_t num_valid = 0;

		for (uint16_t i = 0; i < nb_rx; i++) {
			if (likely(i + 4 < nb_rx)) {
				rte_prefetch0(bufs[i + 4]);
				rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 4],
							       void *));
			}

			struct rte_mbuf *m = bufs[i];
			w_rx_pkts++;
			w_rx_bytes += rte_pktmbuf_pkt_len(m);

			pkt_metadata_t *meta = (pkt_metadata_t *)rte_mbuf_to_priv(m);
			if (likely(meta->is_valid)) {
				data_ptrs[num_valid] = (const uint8_t *)&meta->tuple;
#ifdef DEBUG_MODE
				valid_indices[num_valid] = i;
#endif
				num_valid++;
			} else {
				w_drop_pkts++;
#ifdef DEBUG_MODE
				if (debug_fp) {
					fprintf(debug_fp, "%lu,INVALID,DROP\n", meta->packet_index);
				}
#endif
			}
		}

		if (num_valid > 0 && likely(acl_ctx != NULL)) {
			rte_acl_classify(acl_ctx, data_ptrs, results, num_valid, 1);

			for (uint32_t i = 0; i < num_valid; i++) {
#ifdef DEBUG_MODE
				uint16_t idx = valid_indices[i];
				struct rte_mbuf *m = bufs[idx];
				pkt_metadata_t *meta = (pkt_metadata_t *)rte_mbuf_to_priv(m);
#endif
				uint32_t res = results[i];

				if (res > 0) {
					uint32_t rule_idx = res - 1;
					if (likely(rule_idx < num_rules)) {
						local_rule_hits[rule_idx]++;
						if (rules[rule_idx].action_mask == ACTION_DROP)
							w_drop_pkts++;
#ifdef DEBUG_MODE
						if (debug_fp) {
							fprintf(debug_fp, "%lu,%s,%s\n",
								meta->packet_index,
								rules[rule_idx].name,
								rules[rule_idx].action_mask == ACTION_FORWARD ? "FORWARD" : "DROP");
						}
#endif
					} else {
						w_drop_pkts++;
#ifdef DEBUG_MODE
						if (debug_fp) {
							fprintf(debug_fp, "%lu,DEFAULT,DROP\n", meta->packet_index);
						}
#endif
					}
				} else {
					w_drop_pkts++;
#ifdef DEBUG_MODE
					if (debug_fp) {
						fprintf(debug_fp, "%lu,DEFAULT,DROP\n", meta->packet_index);
					}
#endif
				}
			}
		} else if (num_valid > 0) {
			/* No ACL context available yet — drop all valid packets as fallback */
			for (uint32_t i = 0; i < num_valid; i++) {
				w_drop_pkts++;
#ifdef DEBUG_MODE
				uint16_t idx = valid_indices[i];
				struct rte_mbuf *m = bufs[idx];
				pkt_metadata_t *meta = (pkt_metadata_t *)rte_mbuf_to_priv(m);
				if (debug_fp) {
					fprintf(debug_fp, "%lu,DEFAULT,DROP\n", meta->packet_index);
				}
#endif
			}
		}

		/* Flush local counters to per-worker stats */
		g_worker_stats[w_id].rx_packets  += w_rx_pkts;
		g_worker_stats[w_id].rx_bytes    += w_rx_bytes;
		g_worker_stats[w_id].dropped_packets += w_drop_pkts;

		for (uint32_t i = 0; i < num_rules; i++)
			g_worker_stats[w_id].rule_hits[i] +=
				local_rule_hits[i];

		/* Batch memory free — single call amortises overhead */
		rte_pktmbuf_free_bulk(bufs, nb_rx);
	}

#ifdef DEBUG_MODE
	if (debug_fp) {
		fclose(debug_fp);
	}
#endif

	return 0;
}
