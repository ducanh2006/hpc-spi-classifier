#include "matcher.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_acl.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#define MAX_GROUPS 32

typedef struct {
	char name[64];
	uint32_t precedence;
	action_t action;
} group_entry_t;

/* --- Double-buffered rule tables --- */
spi_rule_t g_rule_table_a[MAX_RULES] __rte_cache_aligned;
spi_rule_t g_rule_table_b[MAX_RULES] __rte_cache_aligned;

/* Atomic active-table pointer, count, and ACL context */
_Atomic(spi_rule_t *) g_active_rules = NULL;
_Atomic uint32_t      g_active_num_rules = 0;
_Atomic(struct rte_acl_ctx *) g_active_acl_ctx = NULL;

/* DPDK ACL 5-tuple field definitions mapping to five_tuple_t layout */
static struct rte_acl_field_def ipv4_defs[5] = {
	/* Protocol */
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = 0,
		.input_index = 0,
		.offset = offsetof(five_tuple_t, protocol),
	},
	/* Source IP */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 1,
		.input_index = 1,
		.offset = offsetof(five_tuple_t, src_ip),
	},
	/* Destination IP */
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = 2,
		.input_index = 2,
		.offset = offsetof(five_tuple_t, dst_ip),
	},
	/* Source Port */
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 3,
		.input_index = 3,
		.offset = offsetof(five_tuple_t, src_port),
	},
	/* Destination Port */
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = 4,
		.input_index = 3,
		.offset = offsetof(five_tuple_t, dst_port),
	},
};

static char *
trim_whitespace(char *str)
{
	while (isspace((unsigned char)*str))
		str++;
	if (*str == 0)
		return str;
	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;
	end[1] = '\0';
	return str;
}

static uint8_t
parse_protocol(const char *proto_str)
{
	if (strcmp(proto_str, "TCP") == 0 || strcmp(proto_str, "tcp") == 0)
		return IPPROTO_TCP;
	if (strcmp(proto_str, "UDP") == 0 || strcmp(proto_str, "udp") == 0)
		return IPPROTO_UDP;
	return 0; /* wildcard: any protocol */
}

static int
parse_rules_into(const char *rule_file, spi_rule_t *table,
		 uint32_t *rule_precedences, uint8_t *rule_src_masks,
		 uint8_t *rule_dst_masks, uint32_t *out_count)
{
	FILE *fp = fopen(rule_file, "r");
	if (!fp) {
		fprintf(stderr, "[matcher] Failed to open rule file: %s\n",
			rule_file);
		return -1;
	}

	char line[256];
	uint32_t count = 0;

	group_entry_t groups[MAX_GROUPS];
	uint32_t num_groups = 0;

	enum {
		STATE_NONE,
		STATE_GROUPS,
		STATE_FILTERS
	} state = STATE_NONE;

	while (fgets(line, sizeof(line), fp)) {
		/* Strip trailing newline/CR */
		line[strcspn(line, "\r\n")] = '\0';

		char *trimmed = trim_whitespace(line);
		if (strlen(trimmed) == 0 || trimmed[0] == '#')
			continue;

		if (strcmp(trimmed, "[GROUPS_SECTION]") == 0) {
			state = STATE_GROUPS;
			continue;
		} else if (strcmp(trimmed, "[FILTERS_SECTION]") == 0) {
			state = STATE_FILTERS;
			continue;
		}

		if (state == STATE_GROUPS) {
			if (num_groups >= MAX_GROUPS)
				continue;

			char *tokens[3];
			int tok_idx = 0;
			char *token = trimmed;
			while (tok_idx < 3) {
				char *comma = strchr(token, ',');
				if (comma) {
					*comma = '\0';
					tokens[tok_idx++] = trim_whitespace(token);
					token = comma + 1;
				} else {
					tokens[tok_idx++] = trim_whitespace(token);
					break;
				}
			}

			if (tok_idx < 3) {
				fprintf(stderr, "[matcher] Malformed group line: %s\n", line);
				continue;
			}

			group_entry_t *grp = &groups[num_groups];
			snprintf(grp->name, sizeof(grp->name), "%s", tokens[0]);
			grp->precedence = (uint32_t)atoi(tokens[1]);
			grp->action = (strcmp(tokens[2], "DROP") == 0) ? ACTION_DROP : ACTION_FORWARD;
			num_groups++;
		} else if (state == STATE_FILTERS) {
			if (count >= MAX_RULES)
				break;

			char *tokens[7];
			int tok_idx = 0;
			char *token = trimmed;
			while (tok_idx < 7) {
				char *comma = strchr(token, ',');
				if (comma) {
					*comma = '\0';
					tokens[tok_idx++] = trim_whitespace(token);
					token = comma + 1;
				} else {
					tokens[tok_idx++] = trim_whitespace(token);
					break;
				}
			}

			if (tok_idx < 7) {
				fprintf(stderr, "[matcher] Malformed filter line: %s\n", line);
				continue;
			}

			uint32_t precedence = 1000;
			action_t action = ACTION_FORWARD;
			bool group_found = false;
			for (uint32_t g = 0; g < num_groups; g++) {
				if (strcmp(tokens[1], groups[g].name) == 0) {
					precedence = groups[g].precedence;
					action = groups[g].action;
					group_found = true;
					break;
				}
			}

			if (!group_found) {
				fprintf(stderr, "[matcher] Warning: Group %s not found for filter %s\n",
					tokens[1], tokens[0]);
			}

			spi_rule_t *rule = &table[count];
			memset(rule, 0, sizeof(spi_rule_t));
			snprintf(rule->name, sizeof(rule->name), "%s", tokens[0]);
			rule->action_mask = action;
			rule_precedences[count] = precedence;

			/* Protocol */
			rule->tuple.protocol = parse_protocol(tokens[2]);

			/* Src IP / Mask */
			char *src_ip_str = tokens[3];
			if (strcmp(src_ip_str, "*") != 0 && strcmp(src_ip_str, "ANY") != 0 && strlen(src_ip_str) > 0) {
				char *slash = strchr(src_ip_str, '/');
				if (slash) {
					*slash = '\0';
					struct in_addr addr;
					if (inet_pton(AF_INET, src_ip_str, &addr) == 1) {
						rule->tuple.src_ip = addr.s_addr;
					}
					rule_src_masks[count] = (uint8_t)atoi(slash + 1);
					*slash = '/';
				} else {
					struct in_addr addr;
					if (inet_pton(AF_INET, src_ip_str, &addr) == 1) {
						rule->tuple.src_ip = addr.s_addr;
					}
					rule_src_masks[count] = 32;
				}
			} else {
				rule_src_masks[count] = 0;
			}

			/* Dst IP / Mask */
			char *dst_ip_str = tokens[4];
			if (strcmp(dst_ip_str, "*") != 0 && strcmp(dst_ip_str, "ANY") != 0 && strlen(dst_ip_str) > 0) {
				char *slash = strchr(dst_ip_str, '/');
				if (slash) {
					*slash = '\0';
					struct in_addr addr;
					if (inet_pton(AF_INET, dst_ip_str, &addr) == 1) {
						rule->tuple.dst_ip = addr.s_addr;
					}
					rule_dst_masks[count] = (uint8_t)atoi(slash + 1);
					*slash = '/';
				} else {
					struct in_addr addr;
					if (inet_pton(AF_INET, dst_ip_str, &addr) == 1) {
						rule->tuple.dst_ip = addr.s_addr;
					}
					rule_dst_masks[count] = 32;
				}
			} else {
				rule_dst_masks[count] = 0;
			}

			/* Ports */
			if (strcmp(tokens[5], "*") != 0 && strlen(tokens[5]) > 0) {
				rule->tuple.src_port = (uint16_t)atoi(tokens[5]);
			} else {
				rule->tuple.src_port = 0;
			}

			if (strcmp(tokens[6], "*") != 0 && strlen(tokens[6]) > 0) {
				rule->tuple.dst_port = (uint16_t)atoi(tokens[6]);
			} else {
				rule->tuple.dst_port = 0;
			}

			count++;
		}
	}

	fclose(fp);
	*out_count = count;
	return 0;
}

static struct rte_acl_ctx *
build_acl_context(const spi_rule_t *rules, const uint32_t *precedences,
		  const uint8_t *src_masks, const uint8_t *dst_masks,
		  uint32_t count)
{
	static uint32_t acl_ctx_counter = 0;
	char name[32];
	snprintf(name, sizeof(name), "spifast_acl_%u", acl_ctx_counter++);

	struct rte_acl_param param = {
		.name = name,
		.socket_id = SOCKET_ID_ANY,
		.rule_size = RTE_ACL_RULE_SZ(5),
		.max_rule_num = MAX_RULES,
	};

	struct rte_acl_ctx *ctx = rte_acl_create(&param);
	if (!ctx) {
		fprintf(stderr, "[matcher] Failed to create ACL context\n");
		return NULL;
	}

	RTE_ACL_RULE_DEF(spi_acl_rule, 5);
	struct spi_acl_rule *acl_rules = calloc(count, sizeof(struct spi_acl_rule));
	if (!acl_rules) {
		fprintf(stderr, "[matcher] Failed to allocate memory for ACL rules\n");
		rte_acl_free(ctx);
		return NULL;
	}

	for (uint32_t i = 0; i < count; i++) {
		const spi_rule_t *r = &rules[i];
		struct spi_acl_rule *ar = &acl_rules[i];

		ar->data.category_mask = 1;
		ar->data.priority = 1000 - precedences[i];
		ar->data.userdata = i + 1;

		/* Field 0: Protocol */
		if (r->tuple.protocol != 0) {
			ar->field[0].value.u8 = r->tuple.protocol;
			ar->field[0].mask_range.u8 = 0xff;
		} else {
			ar->field[0].value.u8 = 0;
			ar->field[0].mask_range.u8 = 0;
		}

		/* Field 1: Source IP */
		ar->field[1].value.u32 = rte_be_to_cpu_32(r->tuple.src_ip);
		ar->field[1].mask_range.u32 = src_masks[i];

		/* Field 2: Destination IP */
		ar->field[2].value.u32 = rte_be_to_cpu_32(r->tuple.dst_ip);
		ar->field[2].mask_range.u32 = dst_masks[i];

		/* Field 3: Source Port */
		if (r->tuple.src_port != 0) {
			ar->field[3].value.u16 = r->tuple.src_port;
			ar->field[3].mask_range.u16 = r->tuple.src_port;
		} else {
			ar->field[3].value.u16 = 0;
			ar->field[3].mask_range.u16 = 65535;
		}

		/* Field 4: Destination Port */
		if (r->tuple.dst_port != 0) {
			ar->field[4].value.u16 = r->tuple.dst_port;
			ar->field[4].mask_range.u16 = r->tuple.dst_port;
		} else {
			ar->field[4].value.u16 = 0;
			ar->field[4].mask_range.u16 = 65535;
		}
	}

	int ret = rte_acl_add_rules(ctx, (struct rte_acl_rule *)acl_rules, count);
	free(acl_rules);

	if (ret < 0) {
		fprintf(stderr, "[matcher] Failed to add rules to ACL context\n");
		rte_acl_free(ctx);
		return NULL;
	}

	struct rte_acl_config config = {
		.num_categories = 1,
		.num_fields = 5,
	};
	memcpy(&config.defs, ipv4_defs, sizeof(ipv4_defs));

	ret = rte_acl_build(ctx, &config);
	if (ret < 0) {
		fprintf(stderr, "[matcher] Failed to build ACL trie\n");
		rte_acl_free(ctx);
		return NULL;
	}

	return ctx;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int
matcher_init(const char *rule_file)
{
	uint32_t count = 0;
	static uint32_t precedences[MAX_RULES];
	static uint8_t src_masks[MAX_RULES];
	static uint8_t dst_masks[MAX_RULES];

	if (parse_rules_into(rule_file, g_rule_table_a, precedences,
			     src_masks, dst_masks, &count) < 0)
		return -1;

	struct rte_acl_ctx *ctx = build_acl_context(g_rule_table_a, precedences,
						    src_masks, dst_masks, count);
	if (!ctx)
		return -1;

	atomic_store_explicit(&g_active_num_rules, count,
			      memory_order_release);
	atomic_store_explicit(&g_active_rules, g_rule_table_a,
			      memory_order_release);
	atomic_store_explicit(&g_active_acl_ctx, ctx,
			      memory_order_release);

	printf("[matcher] Loaded %u rules and built ACL context from %s\n", count, rule_file);
	return 0;
}

int
matcher_reload(const char *rule_file)
{
	spi_rule_t *current =
		atomic_load_explicit(&g_active_rules, memory_order_relaxed);
	spi_rule_t *shadow =
		(current == g_rule_table_a) ? g_rule_table_b : g_rule_table_a;

	uint32_t new_count = 0;
	static uint32_t precedences[MAX_RULES];
	static uint8_t src_masks[MAX_RULES];
	static uint8_t dst_masks[MAX_RULES];

	if (parse_rules_into(rule_file, shadow, precedences,
			     src_masks, dst_masks, &new_count) < 0)
		return -1;

	struct rte_acl_ctx *new_ctx = build_acl_context(shadow, precedences,
							src_masks, dst_masks, new_count);
	if (!new_ctx)
		return -1;

	struct rte_acl_ctx *old_ctx =
		atomic_load_explicit(&g_active_acl_ctx, memory_order_relaxed);

	/* Swap pointers atomically */
	atomic_store_explicit(&g_active_num_rules, new_count,
			      memory_order_release);
	atomic_store_explicit(&g_active_rules, shadow,
			      memory_order_release);
	atomic_store_explicit(&g_active_acl_ctx, new_ctx,
			      memory_order_release);

	/*
	 * Grace period: wait 50ms to ensure all worker threads have finished
	 * their current bursts and switched to the new ACL context.
	 */
	usleep(50000);

	if (old_ctx)
		rte_acl_free(old_ctx);

	printf("[matcher] Hot-reloaded %u rules from %s\n", new_count, rule_file);
	return 0;
}
