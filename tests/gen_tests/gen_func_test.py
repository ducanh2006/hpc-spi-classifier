import os
import csv
import socket
import struct
from scapy.all import Ether, IP, TCP, UDP, Raw, ICMP, IPv6
from scapy.utils import PcapWriter

PROTO_MAP = {"TCP": 6, "UDP": 17, "*": 0}


def ip_to_int(ip_str):
    """Convert dotted-decimal IP string to a 32-bit network-order integer."""
    return struct.unpack("!I", socket.inet_aton(ip_str))[0]


def parse_rules(conf_path):
    """
    Parse spi_rules.conf into a list of dicts, mirroring exactly how the
    C matcher (matcher.c) interprets each field:
      - protocol 0 = wildcard
      - ip 0       = wildcard  (inet_pton fails on CIDR -> stays 0)
      - port 0     = wildcard
    """
    rules = []
    with open(conf_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split(',')
            if len(parts) != 7:
                continue

            # Protocol
            proto_val = PROTO_MAP.get(parts[1], 0)

            # Source IP — mirror C inet_pton: fails on CIDR or '*'
            src_ip_val = 0
            if parts[2] not in ('*', 'ANY'):
                try:
                    src_ip_val = ip_to_int(parts[2])
                except OSError:
                    src_ip_val = 0  # CIDR like "192.168.1.0/24" fails

            # Destination IP
            dst_ip_val = 0
            if parts[3] not in ('*', 'ANY'):
                try:
                    dst_ip_val = ip_to_int(parts[3])
                except OSError:
                    dst_ip_val = 0

            # Ports
            src_port_val = 0 if parts[4] == '*' else int(parts[4])
            dst_port_val = 0 if parts[5] == '*' else int(parts[5])

            rules.append({
                "name":     parts[0],
                "proto_str": parts[1],
                "protocol": proto_val,
                "src_ip":   src_ip_val,
                "dst_ip":   dst_ip_val,
                "src_port": src_port_val,
                "dst_port": dst_port_val,
                "action":   parts[6],
            })
    return rules


def simulate_first_match(rules, pkt_proto, pkt_src_ip, pkt_dst_ip,
                         pkt_src_port, pkt_dst_port):
    """
    Pure-Python replica of the C match_rule() first-match algorithm.
    Field value 0 in a rule means wildcard (matches anything).
    Returns (rule_name, action).
    """
    for rule in rules:
        if rule["protocol"] != 0 and rule["protocol"] != pkt_proto:
            continue
        if rule["src_port"] != 0 and rule["src_port"] != pkt_src_port:
            continue
        if rule["dst_port"] != 0 and rule["dst_port"] != pkt_dst_port:
            continue
        if rule["src_ip"] != 0 and rule["src_ip"] != pkt_src_ip:
            continue
        if rule["dst_ip"] != 0 and rule["dst_ip"] != pkt_dst_ip:
            continue
        return rule["name"], rule["action"]
    # No rule matched at all (should not happen if DEFAULT exists)
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
        import random
        def int_to_ip(ip_int):
            return socket.inet_ntoa(struct.pack('!I', ip_int))

        for rule in rules:
            if rule["name"] == "DEFAULT": continue
            for _ in range(15):
                src_ip_int = rule["src_ip"] if rule["src_ip"] != 0 else random.randint(16777216, 3758096383)
                dst_ip_int = rule["dst_ip"] if rule["dst_ip"] != 0 else random.randint(16777216, 3758096383)
                
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
        # Pick one TCP rule and one UDP rule as targets
        target_rules = [r for r in rules
                        if r["name"] in ("HTTP_TRAFFIC", "DNS_TRAFFIC")]

        # "Correct" values used when a bit is 1
        CORRECT_SRC_IP  = "192.168.1.1"
        CORRECT_DST_IP  = "10.0.0.1"
        CORRECT_SPORT   = 12345

        # "Incorrect" values used when a bit is 0
        WRONG_SRC_IP    = "10.99.99.99"
        WRONG_DST_IP    = "10.88.88.88"
        WRONG_SPORT     = 54321
        WRONG_DPORT     = 9999

        for rule in target_rules:
            rule_port = int(rule["dst_port"]) if rule["dst_port"] != 0 else 80

            for mask in range(1, 64):  # 1 .. 63

                # Bit 5: Packet Validity (0 = Invalid, 1 = Valid IPv4)
                is_valid     = (mask & (1 << 5)) != 0
                # Bit 0: Protocol
                match_proto  = (mask & (1 << 0)) != 0
                # Bit 1: Source IP
                match_srcip  = (mask & (1 << 1)) != 0
                # Bit 2: Destination IP
                match_dstip  = (mask & (1 << 2)) != 0
                # Bit 3: Source Port
                match_srcport = (mask & (1 << 3)) != 0
                # Bit 4: Destination Port
                match_dstport = (mask & (1 << 4)) != 0

                eth = Ether(src="00:11:22:33:44:55",
                            dst="66:77:88:99:AA:BB")

                if not is_valid:
                    # Invalid L3/L4 format -> C parser marks INVALID
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

                # --- Valid IPv4 packet construction ---
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
                    # Mismatch: swap TCP<->UDP
                    if rule["proto_str"] == "TCP":
                        l4 = UDP(sport=sport, dport=dport)
                        pkt_proto = 17
                    else:
                        l4 = TCP(sport=sport, dport=dport, flags="S")
                        pkt_proto = 6

                pkt = eth / ip / l4 / Raw(load=b"Bitmask Test")
                pcap_writer.write(pkt)

                # Use first-match simulation to determine expected result
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
        # In the config, OVERLAP_EXACT (.5 -> DROP) is placed BEFORE 
        # OVERLAP_BROAD (* -> FORWARD).
        # - Packet A (192.168.1.5) should match OVERLAP_EXACT.
        # - Packet B (192.168.1.99) should bypass EXACT and match OVERLAP_BROAD.

        # Packet A: Matches OVERLAP_EXACT
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.5", dst="10.0.0.1")
        l4 = TCP(sport=12345, dport=8080, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Overlap from .5 (Exact)")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("192.168.1.5"),
            ip_to_int("10.0.0.1"), 12345, 8080)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

        # Packet B: Matches OVERLAP_BROAD
        eth = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:AA:BB")
        ip = IP(src="192.168.1.99", dst="10.0.0.1")
        l4 = TCP(sport=12345, dport=8080, flags="S")
        pkt = eth / ip / l4 / Raw(load=b"Overlap from .99 (Broad)")
        pcap_writer.write(pkt)
        expected = simulate_first_match(
            rules, 6, ip_to_int("192.168.1.99"),
            ip_to_int("10.0.0.1"), 12345, 8080)
        csv_writer.writerow([packet_index, *expected])
        packet_index += 1

    print(f"Generated {packet_index} test packets.")


if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_directory = os.path.join(base_dir, "data")
    conf_path = os.path.join(base_dir, "..", "spi_rules.conf")
    generate_func_test(output_directory, conf_path)