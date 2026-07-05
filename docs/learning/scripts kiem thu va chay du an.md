# **HƯỚNG DẪN KIỂM THỬ — CÁC FILE SCRIPT `.sh`**

---

## **1. Tổng quan 5 Script**

| Script | Mục đích | Cần `sudo`? |
|:---|:---|:---:|
| `run_project_native.sh` | Chạy spifast 1 file PCAP (Native Mode, thủ công) | Có |
| `run_project_tcpreplay.sh` | Chạy spifast + tcpreplay (TCPReplay Mode, thủ công) | Có |
| `run_check_correctness.sh` | Kiểm thử tính đúng đắn chức năng (tự động) | Có |
| `run_benchmark_native.sh` | Đo benchmark tất cả PCAP (Native Mode, tự động) | Có |
| `run_benchmark_tcpreplay.sh` | Đo benchmark tất cả PCAP (TCPReplay Mode, tự động) | Có |
| `run_get_raw_throughput.sh` | Đo tốc độ baseline của veth (không qua spifast) | Có |

> **Lưu ý:** Tất cả script phải chạy từ thư mục gốc dự án và cần `sudo` vì DPDK
> yêu cầu quyền root để mount Hugepages và tạo/xóa virtual interface (`ip link`).

---

## **2. Script 1: `run_project_native.sh` — Chạy thủ công Native Mode**

### **Mục đích**
Chạy `spifast` với 1 file PCAP cụ thể ở chế độ Native Mode. Dùng để xem output thống kê
thực tế trên terminal, thử nghiệm nhanh.

### **Cách dùng**
```bash
# Dùng file PCAP mặc định (tls13-rfc8446.pcap)
sudo ./tests/judge/run_project_native.sh

# Chỉ định file PCAP khác
sudo ./tests/judge/run_project_native.sh ./tests/data/pcap/http.pcap
```

### **Những gì script làm**
```bash
# 1. Copy PMD driver vào thư mục an toàn
TMP_DRIVER_DIR="/opt/spifast_dpdk_drivers"
cp third_party/dpdk-24.11/build/drivers/librte_net_pcap.so     "$TMP_DRIVER_DIR/"
cp third_party/dpdk-24.11/build/drivers/librte_mempool_ring.so "$TMP_DRIVER_DIR/"

# 2. Set LD_LIBRARY_PATH để loader tìm được libdpdk.so
export LD_LIBRARY_PATH="./third_party/dpdk-24.11/build/lib:..."

# 3. Chạy spifast với vdev PCAP (infinite_rx=1 → lặp file vô hạn)
./build/spifast \
  -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_pcap=$PCAP_FILE,infinite_rx=1" \
  -- -r "./spi_rules.conf"
```

### **Output mẫu (in ra terminal mỗi giây)**
```
====================================================
Throughput: 45996.31 Mbps | Flow Rate: 19038208 pps
Master Rx: 19038208 pkts | Master Drop: 0 pkts
Worker Rx: 18000000 pkts | Worker Drop: 1038208 pkts
Packet Drop Rate: 0.0000% | Missing Packet Rate: 0.0000%
--- Rule Hits ---
Rule [f_l34_https_all]: 12500000 hits
Rule [f_l34_dns_udp]: 5538208 hits
====================================================
```

### **Dừng chương trình**
```bash
Ctrl + C   # Gửi SIGINT → force_quit = true → thoát gracefully
```

---

## **3. Script 2: `run_project_tcpreplay.sh` — Chạy thủ công TCPReplay Mode**

### **Mục đích**
Chạy `spifast` kết hợp với `tcpreplay` bơm gói qua cặp interface ảo `veth1 → veth0`.
Mô phỏng gần giống môi trường thực tế nhất (có Linux kernel network stack).

### **Cách dùng**
```bash
# Dùng balanced_traffic.pcap (mặc định)
sudo ./tests/judge/run_project_tcpreplay.sh

# Chỉ định file khác
sudo ./tests/judge/run_project_tcpreplay.sh ./tests/data/pcap/telco_traffic.pcap
```

### **Những gì script làm**
```bash
# 1. Tạo cặp virtual ethernet interface
ip link add veth0 type veth peer name veth1
ip link set veth0 up
ip link set veth1 up

# 2. Chạy tcpreplay bơm gói vào veth1 ở tốc độ tối đa, lặp vô hạn (background)
tcpreplay -t --loop=0 -i veth1 "$PCAP_FILE" > /dev/null 2>&1 &
TCPREPLAY_PID=$!

# 3. Chạy spifast lắng nghe trên veth0
./build/spifast \
  -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_iface=veth0" \   # <- Đọc từ interface, không phải file
  -- -r "./spi_rules.conf"

# 4. Cleanup khi thoát (trap EXIT INT TERM)
kill -9 $TCPREPLAY_PID
ip link delete veth0
```

### **Tại sao dùng `veth pair`?**
`veth` là cặp interface ảo "nối đầu": gói tin đi vào `veth1` → tự động xuất hiện ở `veth0`.
`tcpreplay` bơm gói vào `veth1`. `spifast` lắng nghe trên `veth0` qua driver `net_pcap` + `libpcap`.

---

## **4. Script 3: `run_check_correctness.sh` — Kiểm thử chức năng (Tự động)**

### **Mục đích**
Kiểm tra tính đúng đắn 100% của thuật toán phân loại. Script tự động:
1. Sinh bộ luật và file PCAP test case
2. Chạy `spifast_debug` để thu thập kết quả phân loại thực tế
3. So sánh 1-1 với kết quả kỳ vọng

### **Cách dùng**
```bash
sudo ./tests/judge/run_check_correctness.sh
```

### **Luồng thực thi chi tiết**
```bash
# Bước 1: Sinh dữ liệu test
source "$PROJECT_ROOT/venv/bin/activate"
python "$PROJECT_ROOT/tests/gen_tests/gen_func_test.py"
# → Tạo tests/data/pcap/func_test.pcap  (file PCAP với các gói test)
# → Tạo tests/data/csv/func_test_map.csv (kết quả kỳ vọng: packet_idx → rule → action)

# Bước 2: Build debug binary
/usr/bin/meson compile -C "$PROJECT_ROOT/build" spifast_debug

# Bước 3: Chạy spifast_debug (chỉ cần -l 0-1, DEBUG_MODE chỉ ghi log ở Worker 0)
"$PROJECT_ROOT/build/spifast_debug" \
  -d "$TMP_DRIVER_DIR" \
  -l 0-1 \
  --vdev "net_pcap0,rx_pcap=$PROJECT_ROOT/tests/data/pcap/func_test.pcap" \
  -- -r "$PROJECT_ROOT/spi_rules.conf"
# → Ghi ra: tests/results/actual.csv
# → Tự dừng sau khi PCAP stream kết thúc (idle_loops > 5,000,000)

# Bước 4: So sánh actual vs expected
python "$SCRIPT_DIR/check_correctness.py" \
  "$PROJECT_ROOT/tests/data/csv/func_test_map.csv" \  # expected
  "$PROJECT_ROOT/tests/results/actual.csv"             # actual
# → Ghi ra: tests/results/testcase_results.csv
```

### **Cơ chế `DEBUG_MODE`**

Khi biên dịch với `-DDEBUG_MODE`, `worker.c` sẽ mở file `tests/results/actual.csv` và ghi:
```
Packet_Index,Rule,Action
0,f_l34_https_all,FORWARD
1,f_l34_dns_udp,FORWARD
2,DEFAULT,DROP
...
```

`master.c` gán `packet_index` tăng dần cho mỗi gói. `worker.c` (chỉ Worker 0) dùng index đó để ghi log theo thứ tự.

### **Kết quả mong đợi**
```
tests/results/testcase_results.csv:
  Functional,Total Packets,329
  Functional,Matched,329
  Functional,Accuracy,100.00%
```

---

## **5. Script 4: `run_benchmark_native.sh` — Benchmark Native Mode (Tự động)**

### **Mục đích**
Đo hiệu năng xử lý cực hạn của ứng dụng trên **tất cả** file PCAP trong `tests/data/pcap/`.
Kết quả trung bình hóa qua 20 giây đo cho mỗi file.

### **Cách dùng**
```bash
# Đo 20 giây mỗi file (mặc định)
sudo ./tests/judge/run_benchmark_native.sh

# Đo 60 giây mỗi file (chính xác hơn)
sudo ./tests/judge/run_benchmark_native.sh 60
```

### **Luồng thực thi**
```bash
for pcap in tests/data/pcap/*.pcap; do
    # Chạy spifast với timeout 20 giây, infinite_rx=1 (lặp file)
    timeout 20 ./build/spifast \
      -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
      --vdev "net_pcap0,rx_pcap=$pcap,infinite_rx=1" \
      -- -r spi_rules.conf > "$LOG_FILE" 2>&1

    # Parse kết quả từ log (lấy trung bình tất cả dòng stats)
    THROUGHPUT=$(grep "Throughput:" "$LOG_FILE" | awk '{sum+=$2; count++} END {printf "%.2f", sum/count}')
    FLOW_RATE=$(grep "Flow Rate:" "$LOG_FILE"  | awk '{sum+=$7; count++} END {printf "%.0f", sum/count}')
    MASTER_DROP=$(grep "Master Drop:" "$LOG_FILE" | tail -n 1 | awk '{print $8}')
    WORKER_DROP=$(grep "Worker Drop:" "$LOG_FILE" | tail -n 1 | awk '{print $8}')

    echo "$pcap_name,$THROUGHPUT,$FLOW_RATE,$MASTER_DROP,$WORKER_DROP" >> benchmark_native_summary.csv
done
```

### **Kết quả**
```
tests/results/benchmark_native_summary.csv:
  PCAP_File,Throughput_Mbps,Flow_Rate_pps,Master_Drop_Packets,Worker_Drop_Packets
  func_test.pcap,14806.26,27297929,323,131926001
  http.pcap,45147.49,27262976,23419908,0
  tls13-rfc8446.pcap,45996.31,19038208,0,140589828
  balanced_traffic.pcap,0.00,0,0,0        ← File quá lớn, tràn Mempool
```

---

## **6. Script 5: `run_benchmark_tcpreplay.sh` — Benchmark TCPReplay Mode (Tự động)**

### **Mục đích**
Đo hiệu năng xử lý thực tế qua giao tiếp mạng ảo `veth`, mô phỏng sát môi trường thực.

### **Cách dùng**
```bash
sudo ./tests/judge/run_benchmark_tcpreplay.sh

# Hoặc đo 60 giây
sudo ./tests/judge/run_benchmark_tcpreplay.sh 60
```

### **Luồng thực thi**
```bash
for pcap in tests/data/pcap/*.pcap; do
    # Tạo veth pair
    ip link add veth0 type veth peer name veth1
    ip link set veth0 up && ip link set veth1 up

    # Bơm gói bằng tcpreplay (background)
    tcpreplay -t --loop=0 -i veth1 "$pcap" > /dev/null 2>&1 &
    TCPREPLAY_PID=$!

    # Chạy spifast lắng nghe veth0
    timeout 20 ./build/spifast \
      -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
      --vdev "net_pcap0,rx_iface=veth0" \
      -- -r spi_rules.conf > "$LOG_FILE" 2>&1

    # Cleanup
    kill -9 $TCPREPLAY_PID
    ip link delete veth0

    # Parse và lưu kết quả
    ...
done
```

### **Kết quả**
```
tests/results/benchmark_tcpreplay_summary.csv:
  PCAP_File,Throughput_Mbps,Flow_Rate_pps,Master_Drop_Packets,Worker_Drop_Packets
  balanced_traffic.pcap,2187.30,486499,0,3890156
  telco_traffic.pcap,2138.93,477438,0,4200619
  tls13-rfc8446.pcap,851.98,352640,0,2766
  http.pcap,74.92,45243,0,39
  func_test.pcap,479.50,884046,0,4272469
```

---

## **7. Script 6: `run_get_raw_throughput.sh` — Đo Baseline veth (Tự động)**

### **Mục đích**
Đo tốc độ truyền tải tối đa (ceiling) của môi trường mạng ảo `veth` **không** qua spifast.
Đây là mốc baseline để tính hiệu suất suy hao của ứng dụng.

### **Cách dùng**
```bash
sudo ./tests/judge/run_get_raw_throughput.sh

# Đo 60 giây mỗi file
sudo ./tests/judge/run_get_raw_throughput.sh 60
```

### **Cơ chế đo — Không dùng spifast**
```bash
# Đọc counters từ sysfs Linux (không qua spifast)
for i in $(seq 0 $BENCHMARK_TIME); do
    rx_pkts_1=$(cat /sys/class/net/veth0/statistics/rx_packets)
    rx_bytes_1=$(cat /sys/class/net/veth0/statistics/rx_bytes)
    sleep 1
    rx_pkts_2=$(cat /sys/class/net/veth0/statistics/rx_packets)
    rx_bytes_2=$(cat /sys/class/net/veth0/statistics/rx_bytes)

    pkts_per_sec=$((rx_pkts_2 - rx_pkts_1))
    bytes_per_sec=$((rx_bytes_2 - rx_bytes_1))

    # Cộng thêm Ethernet overhead (preamble 8B + IFG 12B + FCS 4B = 24B/frame)
    wire_bytes=$((bytes_per_sec + pkts_per_sec * 24))
    mbps=$(echo "scale=2; $wire_bytes * 8 / 1000000" | bc -l)
done
```

### **Tại sao cộng thêm 24 bytes?**
Mỗi Ethernet frame khi truyền trên đường dây thực tế có thêm:
- **8 bytes Preamble/SFD** (đồng bộ hóa clock)
- **12 bytes Inter-Frame Gap** (khoảng trống giữa 2 frame)
- **4 bytes FCS** (Frame Check Sequence - CRC)

Tổng = 24 bytes overhead không được tính trong `rx_bytes` của kernel.

### **Kết quả**
```
tests/results/benchmark_raw_throughput_summary.csv:
  balanced_traffic.pcap,2942.94,627693,N/A,N/A
  telco_traffic.pcap,3062.50,655389,N/A,N/A
  tls13-rfc8446.pcap,1170.35,448730,N/A,N/A
  http.pcap,110.58,59840,N/A,N/A
  func_test.pcap,925.60,1317882,N/A,N/A
```

---

## **8. Tóm tắt Quy trình Kiểm thử Toàn bộ**

```bash
# Bước 0: Đảm bảo Hugepages
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages

# Bước 1: Build project
meson setup build   # Chỉ cần 1 lần
meson compile -C build

# Bước 2: Kiểm thử chức năng
sudo ./tests/judge/run_check_correctness.sh
# → Xem: tests/results/testcase_results.csv
# → Xem: tests/results/actual.csv (chi tiết từng gói)

# Bước 3: Đo baseline veth
sudo ./tests/judge/run_get_raw_throughput.sh
# → Xem: tests/results/benchmark_raw_throughput_summary.csv

# Bước 4: Benchmark Native Mode
sudo ./tests/judge/run_benchmark_native.sh 20
# → Xem: tests/results/benchmark_native_summary.csv
# → Xem: tests/results/*.pcap_log.txt (log chi tiết)

# Bước 5: Benchmark TCPReplay Mode
sudo ./tests/judge/run_benchmark_tcpreplay.sh 20
# → Xem: tests/results/benchmark_tcpreplay_summary.csv

# Hot-Reload test (khi spifast đang chạy ở terminal khác)
sudo ./build/spi_cli reload_rules ./spi_rules.conf
```

---

## **9. Câu hỏi Hội đồng về Testing**

### **Q: Tại sao kiểm thử chức năng cần `spifast_debug` thay vì `spifast`?**

`spifast` production không có code ghi log CSV (bị loại bỏ bởi `#ifdef DEBUG_MODE`).
`spifast_debug` được biên dịch với `-DDEBUG_MODE` → Worker 0 ghi từng gói ra `actual.csv`.
Nếu dùng production binary, không có cách nào biết từng gói được phân loại thế nào.

### **Q: Tại sao benchmark dùng `timeout`?**

`spifast` chạy vô hạn (while loop + polling). `timeout 20` gửi SIGTERM sau 20 giây.
`signal_handler()` bắt SIGTERM → set `force_quit = true` → tất cả vòng lặp thoát gracefully.
`--preserve-status` đảm bảo exit code gốc được giữ nguyên.

### **Q: Benchmark trung bình hóa số liệu như thế nào?**

Script dùng `awk` để tính trung bình tất cả dòng stats in ra:
```bash
grep "Throughput:" log.txt | awk '{sum+=$2; count++} END {printf "%.2f", sum/count}'
```
Mỗi giây `stats_print_periodic()` in 1 dòng → với 20s benchmark sẽ có ~20 dòng → trung bình.

### **Q: `infinite_rx=1` hoạt động như thế nào?**

Khi PCAP file đọc hết (EOF), driver `net_pcap` lặp lại từ đầu file thay vì trả về 0 gói.
Điều này cho phép đo benchmark ổn định với file PCAP nhỏ mà không cần file gigabyte.

### **Q: Sự khác biệt `--loop=0` trong tcpreplay?**

`tcpreplay --loop=0` = lặp vô hạn (0 có nghĩa là infinite).
`-t` = top speed (không giới hạn tốc độ bơm gói, tối đa phần cứng).
Kết hợp: bơm nhanh nhất có thể, lặp vô hạn.
