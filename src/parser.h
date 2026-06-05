#pragma once
#include <rte_mbuf.h>
#include <stdbool.h>
#include "common.h"

// Parse 5-tuple from mbuf. Returns true if it's an IPv4 packet with TCP/UDP.
bool parse_five_tuple(struct rte_mbuf *m, five_tuple_t *tuple);
