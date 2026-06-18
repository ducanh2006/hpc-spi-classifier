import os
import csv
import sys

def check_correctness(expected_csv_path, actual_csv_path):
    if not os.path.exists(expected_csv_path):
        print(f"[ERROR] Expected CSV not found: {expected_csv_path}")
        return False
    if not os.path.exists(actual_csv_path):
        print(f"[ERROR] Actual CSV not found: {actual_csv_path}")
        return False

    expected_data = {}
    with open(expected_csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            expected_data[row['Packet_Index']] = row

    actual_data = {}
    with open(actual_csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            actual_data[row['Packet_Index']] = row

    total_packets = len(expected_data)
    if total_packets == 0:
        print("[WARNING] Expected CSV is empty.")
        return False

    matched = 0
    mismatched = []
    missing = []

    for pkt_idx, exp_row in expected_data.items():
        if pkt_idx not in actual_data:
            missing.append(pkt_idx)
            continue
            
        act_row = actual_data[pkt_idx]
        if exp_row['Expected_Action'] == act_row.get('Action') and exp_row['Expected_Rule'] == act_row.get('Rule'):
            matched += 1
        else:
            mismatched.append({
                'Packet_Index': pkt_idx,
                'Expected': f"{exp_row['Expected_Rule']} -> {exp_row['Expected_Action']}",
                'Actual': f"{act_row.get('Rule', 'UNKNOWN')} -> {act_row.get('Action', 'UNKNOWN')}"
            })

    accuracy = (matched / total_packets) * 100

    print("====================================================")
    print("          Correctness Evaluation                    ")
    print("====================================================")
    print(f"Total Packets Checked: {total_packets}")
    print(f"Matched: {matched}")
    print(f"Missing in Output: {len(missing)}")
    print(f"Mismatched: {len(mismatched)}")
    print(f"Accuracy: {accuracy:.2f}%")
    
    if len(mismatched) > 0:
        print("\nSample Mismatches (up to 5):")
        for m in mismatched[:5]:
            print(f"  Packet {m['Packet_Index']} | Expected: {m['Expected']} | Actual: {m['Actual']}")
            
    output_csv = os.path.join(os.path.dirname(actual_csv_path), "testcase_results.csv")
    with open(output_csv, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Test_Type", "Metric", "Value"])
        writer.writerow(["Functional", "Total Packets", total_packets])
        writer.writerow(["Functional", "Matched", matched])
        writer.writerow(["Functional", "Missing", len(missing)])
        writer.writerow(["Functional", "Mismatched", len(mismatched)])
        writer.writerow(["Functional", "Accuracy", f"{accuracy:.2f}%"])
        if len(mismatched) > 0:
            writer.writerow([])
            writer.writerow(["Sample Mismatches"])
            writer.writerow(["Packet_Index", "Expected", "Actual"])
            for m in mismatched[:5]:
                writer.writerow([m['Packet_Index'], m['Expected'], m['Actual']])
    print(f"Results exported to: {output_csv}")
            
    if accuracy == 100.0:
        print("\n[RESULT] PASSED! Perfect Match.")
        return True
    else:
        print("\n[RESULT] FAILED! Accuracy below 100%.")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python check_correctness.py <expected_csv> <actual_csv>")
        sys.exit(1)
        
    expected_file = sys.argv[1]
    actual_file = sys.argv[2]
    
    success = check_correctness(expected_file, actual_file)
    sys.exit(0 if success else 1)
