import os
import csv
import socket
import struct
import random
from scapy.all import Ether, IP, TCP, UDP, Raw, ICMP, IPv6
from scapy.utils import PcapWriter

PROTO_MAP = {"TCP": 6, "UDP": 17, "*": 0}


def ip_to_int(ip_str):
    """Convert dotted-decimal IP string to a 32-bit network-order integer."""
    return struct.unpack("!I", socket.inet_aton(ip_str))[0]


def parse_rules(conf_path):
    """
    Parse new section-based spi_rules.conf into a list of dicts.
    Resolves action and precedence via groups, and sorts by priority descending
    to match the DPDK ACL first-match logic.
    """
    groups = {}
    rules = []
    current_section = None
    with open(conf_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line == '[GROUPS_SECTION]':
                current_section = 'groups'
                continue
            elif line == '[FILTERS_SECTION]':
                current_section = 'filters'
                continue
            
            parts = [p.strip() for p in line.split(',')]
            if current_section == 'groups':
                if len(parts) >= 3:
                    # [Group_Name],[Priority],[Action]
                    groups[parts[0]] = (int(parts[1]), parts[2].upper())
            elif current_section == 'filters':
                if len(parts) >= 7:
                    # [Filter_Name],[Group],[Proto],[SrcIP/Mask],[DstIP/Mask],[SrcPort],[DstPort]
                    filter_name = parts[0]
                    group_name = parts[1]
                    proto = parts[2]
                    src_ip_mask = parts[3]
                    dst_ip_mask = parts[4]
                    src_port = parts[5]
                    dst_port = parts[6]
                    
                    prec, action = groups.get(group_name, (1000, "FORWARD"))
                    
                    proto_val = PROTO_MAP.get(proto.upper(), 0)
                    
                    def parse_ip_cidr(ip_str):
                        if ip_str in ('*', 'ANY', ''):
                            return 0, 0
                        if '/' in ip_str:
                            ip_part, mask_part = ip_str.split('/')
                            return ip_to_int(ip_part), int(mask_part)
                        else:
                            return ip_to_int(ip_str), 32
                    
                    src_ip_val, src_mask = parse_ip_cidr(src_ip_mask)
                    dst_ip_val, dst_mask = parse_ip_cidr(dst_ip_mask)
                    
                    src_port_val = 0 if src_port in ('*', '') else int(src_port)
                    dst_port_val = 0 if dst_port in ('*', '') else int(dst_port)
                    
                    rules.append({
                        "name": filter_name,
                        "group": group_name,
                        "proto_str": proto.upper(),
                        "protocol": proto_val,
                        "src_ip": src_ip_val,
                        "src_mask": src_mask,
                        "dst_ip": dst_ip_val,
                        "dst_mask": dst_mask,
                        "src_port": src_port_val,
                        "dst_port": dst_port_val,
                        "precedence": prec,
                        "priority": 1000 - prec,
                        "action": action,
                    })
    # Sort rules by priority descending (highest priority matches first)
    rules.sort(key=lambda r: r["priority"], reverse=True)
    return rules


def simulate_first_match(rules, pkt_proto, pkt_src_ip, pkt_dst_ip,
                          pkt_src_port, pkt_dst_port):
    """
    Pure-Python replica of the DPDK ACL matching algorithm.
    Rules are pre-sorted by priority descending.
    """
    for rule in rules:
        if rule["protocol"] != 0 and rule["protocol"] != pkt_proto:
            continue
        if rule["src_port"] != 0 and rule["src_port"] != pkt_src_port:
            continue
        if rule["dst_port"] != 0 and rule["dst_port"] != pkt_dst_port:
            continue
        
        # Check source IP subnet mask
        if rule["src_mask"] > 0:
            shift = 32 - rule["src_mask"]
            if (rule["src_ip"] >> shift) != (pkt_src_ip >> shift):
                continue
        elif rule["src_ip"] != 0:
            if rule["src_ip"] != pkt_src_ip:
                continue

        # Check destination IP subnet mask
        if rule["dst_mask"] > 0:
            shift = 32 - rule["dst_mask"]
            if (rule["dst_ip"] >> shift) != (pkt_dst_ip >> shift):
                continue
        elif rule["dst_ip"] != 0:
            if rule["dst_ip"] != pkt_dst_ip:
                continue

        return rule["name"], rule["action"]
    
    return "DEFAULT", "DROP"


def generate_func_test(output_dir, conf_path):
    pcap_filename = os.path.join(output_dir, "pcap", "func_test.pcap")
    csv_filename = os.path.join(output_dir, "csv", "func_test_map.csv")

    os.makedirs(os.path.dirname(pcap_filename), exist_ok=True)
    os.makedirs(os.path.dirname(csv_filename), exist_ok=True)

    rules = parse_rules(conf_path)

    with PcapWriter(pcap_filename, append=False, sync=False) as pcap_writer, \
         open(csv_filename, 'w', newline='') as csv_file:

        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(["Packet_Index", "Expected_Rule", "Expected_Action"])

        packet_index = 0

        # ============================================================
        # 0. Balanced Valid Traffic Generation
        # ============================================================
        def int_to_ip(ip_int):
            return socket.inet_ntoa(struct.pack('!I', ip_int))

        for rule in rules:
            for _ in range(15):
                src_ip_int = rule["src_ip"] if rule["src_ip"] != 0 else random.randint(16777216, 3758096383)
                # Generate destination IP matching mask if applicable
                if rule["dst_ip"] != 0:
                    if rule["dst_mask"] > 0:
                        shift = 32 - rule["dst_mask"]
                        mask_base = (rule["dst_ip"] >> shift) << shift
                        host_bits = random.randint(0, (1 << shift) - 1)
                        dst_ip_int = mask_base | host_bits
                    else:
                        dst_ip_int = rule["dst_ip"]
                else:
                    dst_ip_int = random.randint(16777216, 3758096383)
                
                src_ip = int_to_ip(src_ip_int)
                dst_ip = int_to_ip(dst_ip_int)
                
                src_port = rule["src_port"] if rule["src_port"] != 0 else random.randint(1024, 65535)
                dst_port = rule["dst_port"] if rule["dst_port"] != 0 else random.randint(1024, 65535)
                
                proto_str = rule["proto_str"]
                if proto_str == "TCP":
                    l4 = TCP(sport=src_port, dport=dst_port, flags="S")
                    pkt_proto = 6
                elif proto_str == "UDP":
                    l4 = UDP(sport=src_port, dport=dst_port)
                    pkt_proto = 17
                else:
                    l4 = TCP(sport=src_port, dport=dst_port, flags="S")
                    pkt_proto = 6
                
                eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
                ip = IP(src=src_ip, dst=dst_ip)
                pkt = eth / ip / l4 / Raw(load=b"Valid Balanced Traffic")
                
                pcap_writer.write(pkt)
                
                expected_rule, expected_action = simulate_first_match(
                    rules, pkt_proto, src_ip_int, dst_ip_int, src_port, dst_port)
                
                csv_writer.writerow([packet_index, expected_rule, expected_action])
                packet_index += 1

        # ============================================================
        # 1. Bitmask Exhaustive Edge Testing: 2 * (2^6 - 1) = 126
        # ============================================================
        # Pick one TCP rule and one UDP rule as targets from the new configuration
        target_rules = [r for r in rules
                        if r["name"] in ("f_l34_http_all", "f_l34_dns_udp")]

        CORRECT_SRC_IP  = "192.168.1.1"
        CORRECT_DST_IP  = "10.0.0.1"
        CORRECT_SPORT   = 12345

        WRONG_SRC_IP    = "10.99.99.99"
        WRONG_DST_IP    = "10.88.88.88"
        WRONG_SPORT     = 54321
        WRONG_DPORT     = 9999

        for rule in target_rules:
            rule_port = int(rule["dst_port"]) if rule["dst_port"] != 0 else 80

            for mask in range(1, 64):  # 1 .. 63

                is_valid     = (mask & (1 << 5)) != 0
                match_proto  = (mask & (1 << 0)) != 0
                match_srcip  = (mask & (1 << 1)) != 0
                match_dstip  = (mask & (1 << 2)) != 0
                match_srcport = (mask & (1 << 3)) != 0
                match_dstport = (mask & (1 << 4)) != 0

                eth = Ether(src="00:11:22:33:44:55",
                            dst="66:77:88:99:AA:BB")

                if not is_valid:
                    if match_proto:
                        pkt = (eth / IPv6(src="2001:db8::1",
                                          dst="2001:db8::2")
                               / UDP(sport=123, dport=456))
                    else:
                        pkt = eth / IP(src="1.1.1.1",
                                       dst="2.2.2.2") / ICMP()
                    pcap_writer.write(pkt)
                    csv_writer.writerow([packet_index, "INVALID", "DROP"])
                    packet_index += 1
                    continue

                src_ip = CORRECT_SRC_IP if match_srcip else WRONG_SRC_IP
                dst_ip = CORRECT_DST_IP if match_dstip else WRONG_DST_IP
                ip = IP(src=src_ip, dst=dst_ip)

                sport = CORRECT_SPORT if match_srcport else WRONG_SPORT
                dport = rule_port if match_dstport else WRONG_DPORT

                if match_proto:
                    if rule["proto_str"] == "TCP":
                        l4 = TCP(sport=sport, dport=dport, flags="S")
                    else:
                        l4 = UDP(sport=sport, dport=dport)
                    pkt_proto = rule["protocol"]
                else:
                    if rule["proto_str"] == "TCP":
                        l4 = UDP(sport=sport, dport=dport)
                        pkt_proto = 17
                    else:
                        l4 = TCP(sport=sport, dport=dport, flags="S")
                        pkt_proto = 6

                pkt = eth / ip / l4 / Raw(load=b"Bitmask Test")
                pcap_writer.write(pkt)

                expected_rule, expected_action = simulate_first_match(
                    rules, pkt_proto,
                    ip_to_int(src_ip), ip_to_int(dst_ip),
                    sport, dport)

                csv_writer.writerow([packet_index,
                                     expected_rule, expected_action])
                packet_index += 1

        # ============================================================
        # 2. Boundary Value Testing
        # ============================================================

        # Boundary: Port 0
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="10.0.0.1")
        l4 = UDP(sport=12345, dport=0)
        pkt = eth / ip / l4 / Raw(load=b"Boundary Port 0")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 17, ip_to_int("192.168.1.1"),
            ip_to_int("10.0.0.1"), 12345, 0)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Boundary: Port 65535
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="10.0.0.1")
        l4 = TCP(sport=12345, dport=65535, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Boundary Port 65535")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("192.168.1.1"),
            ip_to_int("10.0.0.1"), 12345, 65535)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Boundary: IP 0.0.0.0
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="0.0.0.0", dst="10.0.0.1")
        l4 = TCP(sport=12345, dport=80, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Boundary IP 0.0.0.0")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("0.0.0.0"),
            ip_to_int("10.0.0.1"), 12345, 80)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Boundary: IP 255.255.255.255
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="255.255.255.255", dst="10.0.0.1")
        l4 = TCP(sport=12345, dport=80, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Boundary IP 255.255.255.255")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("255.255.255.255"),
            ip_to_int("10.0.0.1"), 12345, 80)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Boundary: Protocol 0 (HOPOPT)
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="10.0.0.1", proto=0)
        pkt = eth / ip / Raw(load=b"Boundary Proto 0")
        pcap_writer.write(pkt)
        csv_writer.writerow([packet_index, "INVALID", "DROP"])
        packet_index += 1

        # Boundary: Protocol 255 (Reserved)
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="10.0.0.1", proto=255)
        pkt = eth / ip / Raw(load=b"Boundary Proto 255")
        pcap_writer.write(pkt)
        csv_writer.writerow([packet_index, "INVALID", "DROP"])
        packet_index += 1

        # ============================================================
        # 3. Overlap Rules Testing (First-Match Priority)
        # ============================================================
        # f_l34_youtube_1 (Precedence 101) has higher priority than f_l34_https_all (Precedence 103)
        # - Packet A (destination IP 142.250.1.1, dport 443) should match f_l34_youtube_1
        # - Packet B (destination IP 192.168.1.99, dport 443) should match f_l34_https_all

        # Packet A: Matches f_l34_youtube_1
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="142.250.1.1")
        l4 = TCP(sport=12345, dport=443, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Overlap: Youtube IP (should match youtube)")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("192.168.1.1"),
            ip_to_int("142.250.1.1"), 12345, 443)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Packet B: Matches f_l34_https_all
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.1", dst="192.168.1.99")
        l4 = TCP(sport=12345, dport=443, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Overlap: Other IP (should match general HTTPS)")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("192.168.1.1"),
            ip_to_int("192.168.1.99"), 12345, 443)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

    print(f"Generated {packet_index} test packets.")


if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_directory = os.path.join(base_dir, "data")
    conf_path = os.path.join(base_dir, "..", "spi_rules.conf")
    generate_func_test(output_directory, conf_path)