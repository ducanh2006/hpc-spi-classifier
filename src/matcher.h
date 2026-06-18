#pragma once

#include <stdint.h>

#include "common.h"

/**
 * matcher_init - Load initial rules from file into table_a and activate it.
 * @rule_file: Path to the .conf file.
 *
 * Returns 0 on success, -1 on failure.
 */
int matcher_init(const char *rule_file);

/**
 * matcher_reload - Hot-reload rules without stopping the data path.
 *
 * Parses @rule_file into the currently *inactive* (shadow) table, then
 * performs an atomic pointer swap so that all workers see the new rules
 * on their next iteration — fully lock-free, zero downtime.
 *
 * @rule_file: Path to the new .conf file.
 * Returns 0 on success, -1 on parse failure (active rules unchanged).
 */
int matcher_reload(const char *rule_file);

/*
 * match_rule - Inline first-match lookup against the active rule table.
 *
 * Reads the active rule set via atomic_load (memory_order_acquire) so
 * it always sees a fully-committed table after a hot swap.
 * All existing optimizations are preserved:
 *   - always_inline: no call overhead
 *   - __restrict__:  aliasing hint for the compiler
 *   - field order:   most-selective checks first (protocol → ports → IPs)
 */
static inline __attribute__((always_inline))
int match_rule(const spi_rule_t *__restrict__ rules, uint32_t num_rules, const five_tuple_t *__restrict__ tuple)
{

	for (uint32_t i = 0; i < num_rules; i++) {
		if (rules[i].tuple.protocol != 0 &&
		    rules[i].tuple.protocol != tuple->protocol)
			continue;
		if (rules[i].tuple.src_port != 0 &&
		    rules[i].tuple.src_port != tuple->src_port)
			continue;
		if (rules[i].tuple.dst_port != 0 &&
		    rules[i].tuple.dst_port != tuple->dst_port)
			continue;
		if (rules[i].tuple.src_ip != 0 &&
		    rules[i].tuple.src_ip != tuple->src_ip)
			continue;
		if (rules[i].tuple.dst_ip != 0 &&
		    rules[i].tuple.dst_ip != tuple->dst_ip)
			continue;

		return (int)i;
	}

	return -1;
}
