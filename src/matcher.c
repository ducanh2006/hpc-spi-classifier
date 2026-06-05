#include "matcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

spi_rule_t g_rules[MAX_RULES];
uint32_t g_num_rules = 0;

static uint8_t parse_protocol(const char *proto_str)
{
	if (strcmp(proto_str, "TCP") == 0) return IPPROTO_TCP;
	if (strcmp(proto_str, "UDP") == 0) return IPPROTO_UDP;
	return 0; // Any
}

int matcher_init(const char *rule_file)
{
	FILE *fp = fopen(rule_file, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open rule file: %s\n", rule_file);
		return -1;
	}

	char line[256];
	g_num_rules = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (g_num_rules >= MAX_RULES) break;

		// Remove newline
		line[strcspn(line, "\r\n")] = 0;
		if (strlen(line) == 0) continue;

		spi_rule_t *rule = &g_rules[g_num_rules];
		memset(rule, 0, sizeof(spi_rule_t));

		char proto_str[16];
		char src_ip_str[32], dst_ip_str[32];
		char src_port_str[16], dst_port_str[16];
		char action_str[32];

		int parsed = sscanf(line, "%63[^,],%15[^,],%31[^,],%31[^,],%15[^,],%15[^,],%31s",
					rule->name, proto_str, src_ip_str, dst_ip_str,
					src_port_str, dst_port_str, action_str);

		if (parsed != 7) {
			fprintf(stderr, "Failed to parse rule: %s\n", line);
			continue;
		}

		// Parse protocol
		rule->tuple.protocol = parse_protocol(proto_str);

		// IP parsing
		if (strcmp(src_ip_str, "ANY") != 0 && strcmp(src_ip_str, "*") != 0) {
			struct in_addr addr;
			if (inet_pton(AF_INET, src_ip_str, &addr) == 1) {
				rule->tuple.src_ip = rte_be_to_cpu_32(addr.s_addr);
			} else {
				rule->tuple.src_ip = 0;
			}
		} else {
			rule->tuple.src_ip = 0; 
		}

		if (strcmp(dst_ip_str, "ANY") != 0 && strcmp(dst_ip_str, "*") != 0) {
			struct in_addr addr;
			if (inet_pton(AF_INET, dst_ip_str, &addr) == 1) {
				rule->tuple.dst_ip = rte_be_to_cpu_32(addr.s_addr);
			} else {
				rule->tuple.dst_ip = 0;
			}
		} else {
			rule->tuple.dst_ip = 0;
		}

		// Port parsing
		if (strcmp(src_port_str, "*") != 0) {
			rule->tuple.src_port = atoi(src_port_str);
		}
		if (strcmp(dst_port_str, "*") != 0) {
			rule->tuple.dst_port = atoi(dst_port_str);
		}

		// Action parsing
		if (strcmp(action_str, "DROP") == 0) {
			rule->action_mask = ACTION_DROP;
		} else {
			rule->action_mask = ACTION_FORWARD;
		}

		g_num_rules++;
	}

	fclose(fp);
	printf("Loaded %u rules from %s\n", g_num_rules, rule_file);
	return 0;
}
