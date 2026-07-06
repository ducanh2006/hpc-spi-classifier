import os
import random
import csv
import struct
import socket
from scapy.all import Ether, IP, TCP, UDP, Raw
from scapy.utils import PcapWriter

# Number of packets per file
PACKET_COUNT = 1000000
PAYLOAD_SIZE = 512

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

def random_mac():
    return "%02x:%02x:%02x:%02x:%02x:%02x" % (
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255),
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255)
    )

def get_unbalanced_weights(num_categories):
    """
    Generate weights according to the geometric distribution:
    - Level 0 (1 rule): occupies 50% total (50% each)
    - Level 1 (2 rules): occupies 25% total (12.5% each)
    - Level 2 (4 rules): occupies 12.5% total (3.125% each)
    - Level 3 (8 rules): occupies 6.25% total (0.78125% each)
    etc.
    """
    weights = []
    current_idx = 0
    k = 0
    while current_idx < num_categories:
        num_rules_at_level = 2**k
        weight_per_rule = 1.0 / (2.0 * (4.0**k))
        for _ in range(num_rules_at_level):
            if current_idx < num_categories:
                weights.append(weight_per_rule)
                current_idx += 1
            else:
                break
        k += 1
    
    # Normalize weights so they sum to 1.0
    total_weight = sum(weights)
    normalized_weights = [w / total_weight for w in weights]
    return normalized_weights

def generate_unbalanced_traffic(output_dir="."):
    scenario_name = "unbalanced_traffic"
    pcap_filename = os.path.join(output_dir, "pcap", f"{scenario_name}.pcap")
    csv_filename = os.path.join(output_dir, "csv", f"{scenario_name}_map.csv")
    conf_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "spi_rules.conf")
    
    print(f"Loading rules from: {conf_path}")
    rules = parse_rules(conf_path)
    print(f"Loaded {len(rules)} active rules for traffic generation.")
    
    # Pseudo-rules representing different types of default drop traffic
    unmatched_pseudo_rules = [
        {"name": "DEFAULT_DROP", "protocol": 17, "src_ip": 0, "src_mask": 0, "dst_ip": 0, "dst_mask": 0, "src_port": 0, "dst_port": 2152, "action": "DROP", "proto_str": "UDP"}, # GTPU
        {"name": "DEFAULT_DROP", "protocol": 6, "src_ip": 0, "src_mask": 0, "dst_ip": 0, "dst_mask": 0, "src_port": 0, "dst_port": 22, "action": "DROP", "proto_str": "TCP"}, # SSH
        {"name": "DEFAULT_DROP", "protocol": 6, "src_ip": 0, "src_mask": 0, "dst_ip": 0, "dst_mask": 0, "src_port": 0, "dst_port": 9999, "action": "DROP", "proto_str": "TCP"}, # DEFAULT
    ]
    
    # Total categories: len(rules) active rules, plus 1 category for unmatched/drop traffic
    num_categories = len(rules) + 1
    normalized_weights = get_unbalanced_weights(num_categories)
    
    print("Traffic distribution weights:")
    for idx in range(len(rules)):
        print(f"  Rule [{rules[idx]['name']}]: {normalized_weights[idx] * 100:.4f}%")
    print(f"  Unmatched Category (DEFAULT_DROP): {normalized_weights[-1] * 100:.4f}%")
    
    print(f"Generating dataset: {pcap_filename}")
    
    with PcapWriter(pcap_filename, append=False, sync=False) as pcap_writer, \
         open(csv_filename, 'w', newline='') as csv_file:
        
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(["Packet_Index", "Expected_Rule", "Expected_Action"])
        
        # Pre-select all categories using the calculated weights
        categories = list(range(num_categories))
        
        for i in range(PACKET_COUNT):
            if i % 100000 == 0:
                print(f"  ... generated {i} packets")
                
            category_idx = random.choices(categories, weights=normalized_weights, k=1)[0]
            
            if category_idx < len(rules):
                rule = rules[category_idx]
            else:
                rule = random.choice(unmatched_pseudo_rules)
            
            # Generate Source IP
            if rule["src_ip"] != 0:
                if rule["src_mask"] > 0:
                    shift = 32 - rule["src_mask"]
                    mask_base = (rule["src_ip"] >> shift) << shift
                    host_bits = random.randint(0, (1 << shift) - 1)
                    src_ip_int = mask_base | host_bits
                else:
                    src_ip_int = rule["src_ip"]
            else:
                src_ip_int = random.randint(16777216, 3758096383)
                
            # Generate Destination IP
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
                
            src_ip = socket.inet_ntoa(struct.pack('!I', src_ip_int))
            dst_ip = socket.inet_ntoa(struct.pack('!I', dst_ip_int))
            
            # Protocol and ports
            proto_val = rule["protocol"]
            if proto_val == 0:
                proto_val = random.choice([6, 17]) # TCP or UDP
                
            src_port = rule["src_port"] if rule["src_port"] != 0 else random.randint(1024, 65535)
            dst_port = rule["dst_port"] if rule["dst_port"] != 0 else random.randint(1024, 65535)
            
            eth = Ether(src=random_mac(), dst=random_mac())
            ip = IP(src=src_ip, dst=dst_ip)
            
            if proto_val == 6:
                l4 = TCP(sport=src_port, dport=dst_port, flags="S")
            else:
                l4 = UDP(sport=src_port, dport=dst_port)
                
            mock_data = os.urandom(PAYLOAD_SIZE)
            pkt = eth / ip / l4 / Raw(load=mock_data)
            
            pcap_writer.write(pkt)
            
            # Determine expected rule/action by simulation
            expected_rule, expected_action = simulate_first_match(
                rules, proto_val, src_ip_int, dst_ip_int, src_port, dst_port
            )
            csv_writer.writerow([i, expected_rule, expected_action])
            
    print(f"Finished generating {PACKET_COUNT} packets for {scenario_name}\n")

if __name__ == "__main__":
    output_directory = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
    generate_unbalanced_traffic(output_dir=output_directory)
