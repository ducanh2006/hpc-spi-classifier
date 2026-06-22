#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_mbuf.h>

#define MAX_RULES    128
#define MAX_WORKERS  4
#define RING_SIZE    4096
#define BURST_SIZE   64

/* Unix Domain Socket path for the control plane */
#define CTRL_SOCKET_PATH "/tmp/spifast_ctrl.sock"

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

extern volatile bool force_quit;

typedef enum {
	ACTION_DROP    = 0,
	ACTION_FORWARD = 1,
} action_t;

typedef struct {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t  protocol;
} five_tuple_t;

typedef struct {
	five_tuple_t tuple;
	uint8_t is_valid;
#ifdef DEBUG_MODE
	uint64_t packet_index;
#endif
} pkt_metadata_t;


typedef struct {
	char         name[64];
	five_tuple_t tuple;
	uint32_t     action_mask;
	uint64_t     hit_count; /* Bonus: Statistics of rule match count */
} __rte_cache_aligned spi_rule_t;

/*
 * Double-buffered rule tables — defined in matcher.c.
 * The active table is selected via the atomic pointer below.
 * Lock-free readers load g_active_rules with memory_order_acquire;
 * the control thread stores with memory_order_release after fully
 * populating the shadow table, guaranteeing a consistent view.
 */
struct rte_acl_ctx;

extern spi_rule_t g_rule_table_a[MAX_RULES];
extern spi_rule_t g_rule_table_b[MAX_RULES];

extern _Atomic(spi_rule_t *) g_active_rules;
extern _Atomic uint32_t      g_active_num_rules;
extern _Atomic(struct rte_acl_ctx *) g_active_acl_ctx;

