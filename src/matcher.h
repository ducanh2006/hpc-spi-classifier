#pragma once
#include <stdint.h>
#include "common.h"

// Initialize matcher by reading rules from file
int matcher_init(const char *rule_file);

// Match 5-tuple against rules. Returns rule index or -1 if no match.
static inline __attribute__((always_inline)) int match_rule(const five_tuple_t * __restrict__ tuple)
{
	uint32_t num_rules = g_num_rules;
	const spi_rule_t * __restrict__ rules = g_rules;

	for (uint32_t i = 0; i < num_rules; i++) {
		if (rules[i].tuple.protocol != 0 && rules[i].tuple.protocol != tuple->protocol) continue;
		if (rules[i].tuple.src_port != 0 && rules[i].tuple.src_port != tuple->src_port) continue;
		if (rules[i].tuple.dst_port != 0 && rules[i].tuple.dst_port != tuple->dst_port) continue;
		if (rules[i].tuple.src_ip != 0 && rules[i].tuple.src_ip != tuple->src_ip) continue;
		if (rules[i].tuple.dst_ip != 0 && rules[i].tuple.dst_ip != tuple->dst_ip) continue;
		
		return (int)i;
	}
	
	return -1;
}
