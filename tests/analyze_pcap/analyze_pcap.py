"""
source venv/bin/activate
python tests/analyze_pcap/analyze_pcap.py
"""

import sys
import math
import os
import time
import socket
import struct
import glob
from scapy.all import PcapReader, IP, IPv6, TCP, UDP


def load_rules(rule_file):
    groups = {}
    rules = []
    current_section = None
    with open(rule_file, 'r') as f:
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
                    groups[parts[0]] = (int(parts[1]), parts[2].upper())
            elif current_section == 'filters':
                if len(parts) >= 7:
                    filter_name = parts[0]
                    group_name = parts[1]
                    proto = parts[2]
                    src_ip = parts[3]
                    dst_ip = parts[4]
                    src_port = parts[5]
                    dst_port = parts[6]
                    
                    prec, action = groups.get(group_name, (1000, "FORWARD"))
                    
                    rules.append({
                        'name': filter_name,
                        'proto': proto.upper(),
                        'src_ip': src_ip,
                        'dst_ip': dst_ip,
                        'src_port': src_port,
                        'dst_port': dst_port,
                        'priority': 1000 - prec,
                        'action': action
                    })
    # Sort rules by priority descending (highest priority matches first)
    rules.sort(key=lambda r: r['priority'], reverse=True)
    return rules


def ip_in_cidr(ip_str, cidr_str):
    if cidr_str == '*' or cidr_str == 'ANY' or not cidr_str:
        return True
    
    if '/' in cidr_str:
        ip_part, mask_part = cidr_str.split('/')
        mask = int(mask_part)
    else:
        ip_part = cidr_str
        mask = 32
    
    try:
        ip_int = struct.unpack("!I", socket.inet_aton(ip_str))[0]
        cidr_int = struct.unpack("!I", socket.inet_aton(ip_part))[0]
        if mask == 0:
            return True
        shift = 32 - mask
        return (ip_int >> shift) == (cidr_int >> shift)
    except OSError:
        return False


def match_packet(pkt, rules):
    proto_map = {6: 'TCP', 17: 'UDP', 1: 'ICMP'}
    pkt_proto = '*'
    src_ip = '*'
    dst_ip = '*'
    src_port = '*'
    dst_port = '*'

    if IP in pkt:
        src_ip = pkt[IP].src
        dst_ip = pkt[IP].dst
        if pkt[IP].proto in proto_map:
            pkt_proto = proto_map[pkt[IP].proto]
    elif IPv6 in pkt:
        src_ip = pkt[IPv6].src
        dst_ip = pkt[IPv6].dst
        if pkt[IPv6].nh in proto_map:
            pkt_proto = proto_map[pkt[IPv6].nh]

    if TCP in pkt:
        src_port = str(pkt[TCP].sport)
        dst_port = str(pkt[TCP].dport)
    elif UDP in pkt:
        src_port = str(pkt[UDP].sport)
        dst_port = str(pkt[UDP].dport)

    for rule in rules:
        if rule['proto'] != '*' and rule['proto'] != pkt_proto:
            continue
        if not ip_in_cidr(src_ip, rule['src_ip']):
            continue
        if not ip_in_cidr(dst_ip, rule['dst_ip']):
            continue
        if rule['src_port'] != '*' and rule['src_port'] != src_port:
            continue
        if rule['dst_port'] != '*' and rule['dst_port'] != dst_port:
            continue
        return rule['name']

    return "UNCLASSIFIED"


def analyze_pcap(pcap_path, rule_path, output_path):
    rules = load_rules(rule_path)
    
    total_packets = 0
    total_size = 0
    size_squared_sum = 0
    rule_counts = {rule['name']: 0 for rule in rules}
    rule_counts["UNCLASSIFIED"] = 0
    
    print(f"Reading rules from {rule_path}...")
    print(f"Analyzing {pcap_path}... This may take a moment for large files.")
    
    start_time = time.time()
    
    try:
        with PcapReader(pcap_path) as pcap:
            for pkt in pcap:
                total_packets += 1
                size = len(pkt)
                total_size += size
                size_squared_sum += size * size
                
                rule_name = match_packet(pkt, rules)
                rule_counts[rule_name] += 1
                
                if total_packets % 100000 == 0:
                    print(f"  ... processed {total_packets} packets")
                    
    except Exception as e:
        print(f"Error reading pcap: {e}")
        return

    elapsed = time.time() - start_time
    
    if total_packets == 0:
        with open(output_path, 'w') as f:
            f.write("No packets found in PCAP file.\n")
        return

    avg_size = total_size / total_packets
    variance = (size_squared_sum / total_packets) - (avg_size * avg_size)
    variance = max(0, variance) 
    stddev = math.sqrt(variance)

    with open(output_path, "w") as f:
        f.write(f"PCAP Analysis Report: {os.path.basename(pcap_path)}\n")
        f.write("=" * 60 + "\n")
        f.write(f"Total Packet Count  : {total_packets:,}\n")
        f.write(f"Average Packet Size : {avg_size:.2f} bytes\n")
        f.write(f"Packet Size StdDev  : {stddev:.2f} bytes\n")
        f.write("-" * 60 + "\n")
        f.write("Rule Classification Distribution:\n")
        
        sorted_rules = sorted(rule_counts.items(), key=lambda x: x[1], reverse=True)
        
        for rule_name, count in sorted_rules:
            if count > 0 or rule_name != "UNCLASSIFIED":
                percentage = (count / total_packets) * 100
                f.write(f"  - {rule_name:<16}: {count:>10,} packets ({percentage:>6.2f}%)\n")
        f.write("=" * 60 + "\n")
        
    print(f"\nAnalysis complete in {elapsed:.2f} seconds.")
    print(f"Results successfully saved to: {output_path}")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, "..", ".."))
    pcap_dir = os.path.join(project_root, "tests", "data", "pcap")
    rule_path = os.path.join(project_root, "spi_rules.conf")
    
    if not os.path.exists(rule_path):
        print(f"Error: Rule file not found at {rule_path}")
        sys.exit(1)
        
    pcap_files = glob.glob(os.path.join(pcap_dir, "*.pcap"))
    if not pcap_files:
        print(f"No PCAP files found in {pcap_dir}")
        sys.exit(0)
        
    print(f"Found {len(pcap_files)} PCAP files to analyze in {pcap_dir}")
    
    for pcap_path in pcap_files:
        pcap_filename = os.path.basename(pcap_path)
        output_filename = pcap_filename.replace(".pcap", "_analysis.txt")
        output_path = os.path.join(script_dir, output_filename)
        
        print(f"\n[{pcap_filename}] -> [{output_filename}]")
        analyze_pcap(pcap_path, rule_path, output_path)


if __name__ == "__main__":
    main()