#pragma once
#include <rte_ring.h>
#include <stdint.h>

struct worker_params {
	struct rte_ring *ring;
	uint32_t worker_id;
};

int worker_loop(void *arg);
