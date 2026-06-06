#pragma once
#include <rte_mbuf.h>
#include <stdbool.h>
#include "common.h"

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_byteorder.h>

// Parse 5-tuple from mbuf. Returns true if it's an IPv4 packet with TCP/UDP.
static inline __attribute__((always_inline)) bool parse_five_tuple(struct rte_mbuf * __restrict__ m, five_tuple_t * __restrict__ tuple)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_ipv4_hdr *ip_hdr;
	uint16_t ether_type;
	uint16_t l3_offset = 0;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	ether_type = eth_hdr->ether_type;
	l3_offset = sizeof(struct rte_ether_hdr);

	// Handle VLAN if present
	if (unlikely(ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN))) {
		struct rte_vlan_hdr *vlan_hdr = (struct rte_vlan_hdr *)((uint8_t *)eth_hdr + l3_offset);
		ether_type = vlan_hdr->eth_proto;
		l3_offset += sizeof(struct rte_vlan_hdr);
	}

	if (likely(ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))) {
		ip_hdr = (struct rte_ipv4_hdr *)((uint8_t *)eth_hdr + l3_offset);
		
		tuple->src_ip = ip_hdr->src_addr;
		tuple->dst_ip = ip_hdr->dst_addr;
		tuple->protocol = ip_hdr->next_proto_id;
		
		uint16_t ip_hlen = (ip_hdr->version_ihl & RTE_IPV4_HDR_IHL_MASK) * RTE_IPV4_IHL_MULTIPLIER;
		uint16_t l4_offset = l3_offset + ip_hlen;
		
		if (tuple->protocol == IPPROTO_TCP) {
			struct rte_tcp_hdr *tcp_hdr = (struct rte_tcp_hdr *)((uint8_t *)eth_hdr + l4_offset);
			tuple->src_port = tcp_hdr->src_port;
			tuple->dst_port = tcp_hdr->dst_port;
			return true;
		} else if (tuple->protocol == IPPROTO_UDP) {
			struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t *)eth_hdr + l4_offset);
			tuple->src_port = udp_hdr->src_port;
			tuple->dst_port = udp_hdr->dst_port;
			return true;
		}
	}
	
	return false; // Not an IPv4 TCP/UDP packet
}
