#pragma once
#include <rte_ring.h>
#include <stdint.h>

int master_loop(struct rte_ring *worker_rings[], uint32_t num_workers, uint16_t port_id);
