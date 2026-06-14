#include "master.h"
#include "common.h"
#include "stats.h"
#include "parser.h"
#include <rte_ethdev.h>
#include <rte_jhash.h>
#include <rte_lcore.h>

int master_loop(struct rte_ring *worker_rings[], uint32_t num_workers, uint16_t port_id)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	uint32_t current_worker = 0;
	
	printf("Master started on lcore %u\n", rte_lcore_id());
	
	uint16_t loop_count = 0;
	
	while (1) {
		if (unlikely((loop_count++ & 0xFFF) == 0)) {
			stats_print_periodic();
		}
		
		uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
		
		if (unlikely(nb_rx == 0)) continue;
		
		uint64_t total_rx_bytes = 0;
		for (uint16_t i = 0; i < nb_rx; i++) {
			total_rx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
		}
		
		g_master_rx_packets += nb_rx;
		g_master_rx_bytes += total_rx_bytes;
		
		// Enqueue the entire burst to the current worker ring
		uint16_t nb_tx = rte_ring_enqueue_burst(worker_rings[current_worker], 
			(void * const *)bufs, nb_rx, NULL);
		
		if (unlikely(nb_tx < nb_rx)) {
			uint16_t drop_count = nb_rx - nb_tx;
			g_master_dropped_packets += drop_count;
			
			// Free dropped packets
			rte_pktmbuf_free_bulk(&bufs[nb_tx], drop_count);
		}
		
		// Rotate to the next worker
		current_worker++;
		if (unlikely(current_worker >= num_workers)) {
			current_worker = 0;
		}
	}
	
	return 0;
}
