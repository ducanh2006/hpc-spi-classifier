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

# Rules definition
RULES = [
    {"name": "GTPU_TRAFFIC",  "proto": "UDP", "port": 2152, "action": "FORWARD", "mock": b'\x30\xff\x00\x1c\x00\x00\x00\x00'},
    {"name": "HTTPS_TRAFFIC", "proto": "TCP", "port": 443,  "action": "FORWARD", "mock": b'\x16\x03\x01\x00\x00'},
    {"name": "HTTP_TRAFFIC",  "proto": "TCP", "port": 80,   "action": "FORWARD", "mock": b'GET / HTTP/1.1\r\nHost: example.com\r\n\r\n'},
    {"name": "DNS_TRAFFIC",   "proto": "UDP", "port": 53,   "action": "FORWARD", "mock": b'\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03www\x06google\x03com\x00\x00\x01\x00\x01'},
    {"name": "SSH_BLOCK",     "proto": "TCP", "port": 22,   "action": "DROP",    "mock": b'SSH-2.0-OpenSSH_8.9p1\r\n'},
    {"name": "DEFAULT",       "proto": "TCP", "port": 9999, "action": "DROP",    "mock": b'UNKNOWN_PROTOCOL_DATA'}
]

WEIGHTS = {
    "GTPU_TRAFFIC": 0.45,
    "HTTPS_TRAFFIC": 0.30,
    "HTTP_TRAFFIC": 0.10,
    "DNS_TRAFFIC": 0.05,
    "SSH_BLOCK": 0.05,
    "DEFAULT": 0.05
}

def random_mac():
    return "%02x:%02x:%02x:%02x:%02x:%02x" % (
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255),
        random.randint(0, 255), random.randint(0, 255), random.randint(0, 255)
    )

def random_ip():
    return socket.inet_ntoa(struct.pack('>I', random.randint(1, 0xffffffff)))

def generate_telco_traffic(output_dir="."):
    scenario_name = "telco_traffic"
    pcap_filename = os.path.join(output_dir, "pcap", f"{scenario_name}.pcap")
    csv_filename = os.path.join(output_dir, "csv", f"{scenario_name}_map.csv")
    
    print(f"Generating dataset: {pcap_filename}")
    
    rules = list(WEIGHTS.keys())
    probs = [WEIGHTS[r] for r in rules]
    rule_map = {r["name"]: r for r in RULES}

    with PcapWriter(pcap_filename, append=False, sync=False) as pcap_writer, \
         open(csv_filename, 'w', newline='') as csv_file:
        
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow(["Packet_Index", "Expected_Rule", "Expected_Action"])
        
        for i in range(PACKET_COUNT):
            if i % 100000 == 0:
                print(f"  ... generated {i} packets")
                
            selected_rule_name = random.choices(rules, weights=probs, k=1)[0]
            rule = rule_map[selected_rule_name]
            
            eth = Ether(src=random_mac(), dst=random_mac())
            ip = IP(src=random_ip(), dst=random_ip())
            src_port = random.randint(1024, 65535)
            
            if rule["proto"] == "TCP":
                l4 = TCP(sport=src_port, dport=rule["port"], flags="S")
            else:
                l4 = UDP(sport=src_port, dport=rule["port"])
            
            mock_data = rule["mock"]
            pad_len = PAYLOAD_SIZE - len(mock_data)
            payload = mock_data + os.urandom(pad_len) if pad_len > 0 else mock_data[:PAYLOAD_SIZE]
                
            pkt = eth / ip / l4 / Raw(load=payload)
            
            pcap_writer.write(pkt)
            csv_writer.writerow([i, rule["name"], rule["action"]])
            
    print(f"Finished generating {PACKET_COUNT} packets for {scenario_name}\n")

if __name__ == "__main__":
    output_directory = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
    generate_telco_traffic(output_dir=output_directory)
