#include "matcher.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_byteorder.h>

/* --- Double-buffered rule tables --- */
spi_rule_t g_rule_table_a[MAX_RULES] __rte_cache_aligned;
spi_rule_t g_rule_table_b[MAX_RULES] __rte_cache_aligned;

/* Atomic active-table pointer and count — read lock-free by all lcores */
_Atomic(spi_rule_t *) g_active_rules;
_Atomic uint32_t      g_active_num_rules = 0;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static uint8_t parse_protocol(const char *proto_str)
{
	if (strcmp(proto_str, "TCP") == 0) return IPPROTO_TCP;
	if (strcmp(proto_str, "UDP") == 0) return IPPROTO_UDP;
	return 0; /* wildcard: any protocol */
}

/*
 * parse_rules_into - Parse a .conf file into a caller-supplied rule table.
 *
 * @rule_file: Path to the configuration file.
 * @table:     Destination rule array (must hold at least MAX_RULES entries).
 * @out_count: Set to the number of successfully parsed rules on return.
 *
 * Returns 0 on success, -1 if the file could not be opened.
 * Individual malformed lines are skipped with a warning; they do not
 * cause the function to fail.
 *
 * Only two actions are accepted per the v2 spec: FORWARD and DROP.
 * Any unrecognized action string is treated as FORWARD.
 */
static int parse_rules_into(const char *rule_file,
			     spi_rule_t *table,
			     uint32_t   *out_count)
{
	FILE *fp = fopen(rule_file, "r");
	if (!fp) {
		fprintf(stderr, "[matcher] Failed to open rule file: %s\n",
			rule_file);
		return -1;
	}

	char     line[256];
	uint32_t count = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (count >= MAX_RULES) break;

		/* Strip trailing newline/CR */
		line[strcspn(line, "\r\n")] = '\0';
		if (strlen(line) == 0) continue;

		spi_rule_t *rule = &table[count];
		memset(rule, 0, sizeof(spi_rule_t));

		char proto_str[16];
		char src_ip_str[32], dst_ip_str[32];
		char src_port_str[16], dst_port_str[16];
		char action_str[32];

		int parsed = sscanf(line,
			"%63[^,],%15[^,],%31[^,],%31[^,],%15[^,],%15[^,],%31s",
			rule->name, proto_str, src_ip_str, dst_ip_str,
			src_port_str, dst_port_str, action_str);

		if (parsed != 7) {
			fprintf(stderr,
				"[matcher] Skipping malformed rule: %s\n",
				line);
			continue;
		}

		/* Protocol */
		rule->tuple.protocol = parse_protocol(proto_str);

		/* Source IP */
		if (strcmp(src_ip_str, "*") != 0 &&
		    strcmp(src_ip_str, "ANY") != 0) {
			struct in_addr addr;
			if (inet_pton(AF_INET, src_ip_str, &addr) == 1)
				rule->tuple.src_ip = addr.s_addr;
		}

		/* Destination IP */
		if (strcmp(dst_ip_str, "*") != 0 &&
		    strcmp(dst_ip_str, "ANY") != 0) {
			struct in_addr addr;
			if (inet_pton(AF_INET, dst_ip_str, &addr) == 1)
				rule->tuple.dst_ip = addr.s_addr;
		}

		/* Ports — stored in network byte order to match wire format */
		if (strcmp(src_port_str, "*") != 0)
			rule->tuple.src_port =
				rte_cpu_to_be_16((uint16_t)atoi(src_port_str));
		if (strcmp(dst_port_str, "*") != 0)
			rule->tuple.dst_port =
				rte_cpu_to_be_16((uint16_t)atoi(dst_port_str));

		/* Action — v2 spec: only FORWARD or DROP */
		rule->action_mask = (strcmp(action_str, "DROP") == 0)
			? ACTION_DROP : ACTION_FORWARD;

		count++;
	}

	fclose(fp);
	*out_count = count;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int matcher_init(const char *rule_file)
{
	uint32_t count = 0;

	if (parse_rules_into(rule_file, g_rule_table_a, &count) < 0)
		return -1;

	/*
	 * Publish the initial rule set.  Use memory_order_release so
	 * all stores to g_rule_table_a are visible before any lcore
	 * that loads g_active_rules with memory_order_acquire.
	 */
	atomic_store_explicit(&g_active_num_rules, count,
			      memory_order_release);
	atomic_store_explicit(&g_active_rules, g_rule_table_a,
			      memory_order_release);

	printf("[matcher] Loaded %u rules from %s\n", count, rule_file);
	return 0;
}

int matcher_reload(const char *rule_file)
{
	/*
	 * Determine the shadow (inactive) table.
	 * relaxed load is fine here — we only need to know *which*
	 * table is currently active; correctness is guaranteed by the
	 * release store at the end.
	 */
	spi_rule_t *current =
		atomic_load_explicit(&g_active_rules, memory_order_relaxed);
	spi_rule_t *shadow  =
		(current == g_rule_table_a) ? g_rule_table_b : g_rule_table_a;

	uint32_t new_count = 0;

	if (parse_rules_into(rule_file, shadow, &new_count) < 0)
		return -1;

	/*
	 * Atomic swap — update count first, then flip the pointer.
	 * Workers that have already loaded the old pointer finish
	 * cleanly; the next iteration picks up the new table.
	 * Both stores use memory_order_release to guarantee the shadow
	 * table is fully visible before any acquire-load sees the new
	 * pointer value.
	 */
	atomic_store_explicit(&g_active_num_rules, new_count,
			      memory_order_release);
	atomic_store_explicit(&g_active_rules, shadow,
			      memory_order_release);

	printf("[matcher] Hot-reloaded %u rules from %s\n",
	       new_count, rule_file);
	return 0;
}
