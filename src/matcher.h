#pragma once

#include <stdint.h>

#include <rte_acl.h>

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
 * match_rule - Inline lookup against the active ACL context.
 *
 * Reads the active ACL context via atomic_load (memory_order_acquire).
 * Always inline to avoid function call overhead.
 */
static inline __attribute__((always_inline))
int match_rule(const struct rte_acl_ctx *acl_ctx, const five_tuple_t *__restrict__ tuple)
{
	const uint8_t *data = (const uint8_t *)tuple;
	uint32_t results = 0;

	if (unlikely(acl_ctx == NULL))
		return -1;

	rte_acl_classify(acl_ctx, &data, &results, 1, 1);
	return (results > 0) ? (int)(results - 1) : -1;
}

