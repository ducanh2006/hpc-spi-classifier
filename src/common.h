#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <rte_common.h>
#include <rte_mbuf.h>

#define MAX_RULES 128
#define MAX_WORKERS 4
#define RING_SIZE 4096
#define BURST_SIZE 64

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef enum {
	ACTION_DROP = 0,
	ACTION_FORWARD = 1,
} action_t;

typedef struct {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t protocol;
} five_tuple_t;

typedef struct {
	char name[64];
	five_tuple_t tuple;
	uint32_t action_mask;
	uint64_t hit_count; // Bonus: Statistics of rule match count
} spi_rule_t;

// Extern declarations for global state
extern spi_rule_t g_rules[MAX_RULES];
extern uint32_t g_num_rules;
