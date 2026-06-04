# **BÀI TẬP LỚN / MINI PROJECT**

## **SPIFast: Hệ Thống Kiểm Tra Gói Tin Hiệu Năng Cao (SPI) Sử Dụng DPDK**


**Môi trường thử nghiệm công bố:** Chạy trực tiếp trên 01 máy cá nhân (PC/Laptop Linux), giả lập mạng bằng cơ chế PCAP Virtual Device (vdev) PMD không phụ thuộc card mạng rời phần cứng.  
**Người hướng dẫn (Mentor):** Nguyễn Ngọc Dũng (Email: dungnn11@viettel.com.vn)

# 1. Bối cảnh

Trong các hệ thống mạng lõi hiện đại (Firewall, UPF trong mạng 5G, Load Balancer), việc phân loại lưu lượng là chức năng quan trọng để áp dụng chính sách xử lý và phân tải. Kỹ thuật **Shallow Packet Inspection (SPI)** giúp hệ thống chỉ kiểm tra các trường thông tin tiêu đề (Header) L2/L3/L4 (như IP nguồn/đích, Giao thức, Port nguồn/đích, VLAN ID) nhằm đạt hiệu năng cực cao mà không tốn tài nguyên bóc tách Payload ứng dụng như DPI.

Để mô phỏng chính xác và tạo điều kiện triển khai linh hoạt nhất cho sinh viên trên máy tính cá nhân (PC/Laptop) mà không yêu cầu trang bị phần cứng card mạng chuyên dụng đắt tiền (Intel X520/X710...), đồ án này ứng dụng driver giả lập mạng net_pcap ảo (PCAP Virtual Device PMD) của thư viện DPDK. Cơ chế này cho phép nạp trực tiếp file lưu vết gói tin (.pcap) tuần hoàn vào bộ đệm mbuf của CPU, mô phỏng chính xác dòng lưu lượng line-rate lên tới 1 Gbps.

# 2. Mục tiêu dự án

Mini Project hướng tới xây dựng một chương trình SPI đơn giản sử dụng DPDK để thực hiện packet classification theo rule.

Các mục tiêu chính gồm:

- Khởi tạo môi trường DPDK
- Nhận packet từ NIC bằng DPDK
- Parse Ethernet Header
- Parse IPv4 Header
- Parse TCP/UDP Header
- Trích xuất thông tin Five-Tuple
- Đọc bộ rule từ file cấu hình
- Thực hiện so khớp packet với rule
- Thực hiện action tương ứng
- Phân tải packet tới worker phù hợp
- Thu thập và hiển thị thống kê runtime

Thông qua Mini Project này, sinh viên sẽ hiểu được:

- Nguyên lý hoạt động của SPI
- Cách xây dựng Rule Engine trong hệ thống mạng
- Cơ chế xử lý packet tốc độ cao bằng DPDK
- Thiết kế hệ thống networking đa luồng
- Tối ưu hiệu năng trong data-plane

# 3. Giải pháp kỹ thuật & Kiến trúc giả lập vdev

## 3.1 Cơ chế Replay mạng ảo qua PCAP vdev

Hệ thống không sử dụng driver card vật lý thông thường mà thay bằng driver mạng ảo ảo hóa 'librte_pmd_pcap'. Khi khởi chạy ứng dụng, DPDK sẽ tự động ánh xạ một file lưu vết 'traffic_sample.pcap' (được chuẩn bị từ trước bằng Wireshark) thành một interface mạng ảo logic. Mỗi lần mã nguồn gọi hàm API `rte_eth_rx_burst()`, Driver ảo này sẽ giả lập việc nhận gói tin bằng cách đọc liên tục các block byte từ file pcap nạp thẳng vào cấu trúc cấu hình bộ nhớ Hugepages.

## 3.2 Phân lớp luồng kiến trúc mạng đa lõi

Để tối ưu hiệu năng xử lý trên PC, kiến trúc luồng dữ liệu (Data Path) được tổ chức theo mô hình Pipeline phi khóa tuần tự:

- **Master lcore (Rx / Dispatcher Core):** Chạy vòng lặp vô hạn gọi `rte_eth_rx_burst()` để lấy các mbuf từ card mạng ảo vdev. Thực hiện gọi hàm Parser tách 5-tuple, chạy Rule Matcher để tìm Rule ID và đẩy con trỏ mbuf vào các ring thích hợp thông qua `rte_ring_enqueue()`.
- **Worker lcores (Worker 0 -> 3):** Chạy độc lập trên các lõi CPU khác nhau. Liên tục kiểm tra hàng đợi thông qua `rte_ring_dequeue()` để rút mbuf ra, cập nhật bộ đếm xử lý riêng biệt và giải phóng mbuf về lại Mempool bằng hàm `rte_pktmbuf_free()`.
- **Lock-free Queue (rte_ring):** Đóng vai trò cầu nối giao tiếp IPC siêu tốc giữa Master Core và Worker Cores, triệt tiêu hoàn toàn việc dùng khóa bảo vệ (Mutex / Spinlock) - tác nhân chính gây tụt giảm hiệu năng xử lý mạng.

# 4. Đặc tả bộ luật SPI & Cấu trúc mã nguồn

```c
// Định nghĩa cấu trúc lưu trữ nội bộ tối ưu hóa bộ nhớ đệm Cache-line
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
    uint64_t hit_count; // Điểm cộng: Thống kê số lần match luật
} spi_rule_t;
```

## 4.1 File cấu hình mẫu (spi_rules.conf)

```text
HTTP_TRAFFIC,TCP,*,*,*,80,FORWARD_WORKER_0
HTTPS_TRAFFIC,TCP,*,*,*,443,FORWARD_WORKER_1
DNS_TRAFFIC,UDP,*,*,*,53,FORWARD_WORKER_2
GTPU_TRAFFIC,UDP,*,*,*,2152,FORWARD_WORKER_3
SSH_BLOCK,TCP,*,*,*,22,DROP
DEFAULT,*,*,*,*,*,DROP
```

# 5. Tiêu chuẩn & Chỉ số hiệu năng bắt buộc (KPIs)

Mặc dù chạy giả lập trên một PC duy nhất thông qua cơ chế nạp file PCAP vdev, hệ thống vẫn phải tuân thủ nghiêm ngặt các chỉ tiêu đo kiểm hiệu năng phần cứng data-plane của card 1 Gbps để đảm bảo tư duy tối ưu hóa của sinh viên:

| Tham số Hiệu năng | Mức Đạt (Pass) | Mức Xuất Sắc (Excellent) | Phương pháp đo & Công cụ hỗ trợ |
| :--- | :--- | :--- | :--- |
| **Thông lượng băng thông (Throughput)** | ≥ 700 Mbps<br>*(Khi replay file PCAP chứa các gói kích thước trung bình 512B - 1024B)* | Từ 950 - 990 Mbps<br>*(Tiệm cận tối đa tốc độ line-rate của card mạng 1Gbps)* | Đo đạc dựa trên tổng số Byte nhận được chia cho thời gian Delta t của đồng hồ runtime. |
| **Mật độ xử lý gói tin (Flow Rate)** | ≥ 500,000 pps<br>*(0.5 Mpps)* | ≥ 1,488,000 pps<br>*(Line-rate gói tin 64B của chuẩn mạng Ethernet 1Gbps)* | Tính toán trực tiếp bằng cách lấy hiệu số số lượng gói nhận tại hàm thống kê ứng dụng định kỳ. |
| **Tỷ lệ rơi gói tin (Packet Drop Rate)** | ≤ 0.1%<br>*(tại mức tải tối đa của CPU)* | 0% (Zero Packet Drop)<br>*(Xử lý mbuf mượt mà không bị nghẽn nghẹt tại ring)* | Đối chiếu tỷ lệ gói rơi do tràn hàng đợi ring buffer nội bộ của các luồng Worker. |
| **Tỷ lệ bỏ sót gói (Missing Rate)** | 0% Tuyệt đối<br>*(Không có gói tin biến mất)* | 0% Tuyệt đối<br>*(Không sai lệch bộ đếm mạng)* | Tổng số gói đọc ra từ file PCAP gốc bắt buộc phải khớp chính xác: Tổng Match + Tổng Default Drop. |

# 6. Hướng dẫn thực thi lệnh kiểm thử chi tiết trên Single-PC

## 6.1 Cấu hình tài nguyên hệ thống Linux

Trước khi chạy ứng dụng DPDK, sinh viên bắt buộc phải cấp phát tài nguyên trang bộ nhớ lớn Hugepages trên máy:

```bash
# Cấp phát 1024 trang bộ nhớ Hugepages loại 2MB (Tương đương 2GB RAM cho PC)
sudo sysctl -w vm.nr_hugepages=1024

# Kiểm tra trạng thái cấp phát thành công
cat /proc/meminfo | grep Huge
```

## 6.2 Lệnh thực thi ứng dụng mô phỏng luồng PCAP Loop Replay

Sinh viên chuẩn bị một file mẫu 'traffic_sample.pcap' đặt tại thư mục chạy. Thực hiện chạy lệnh ứng dụng ép lõi lõi cứng (Core affinity):

```bash
# Thực thi ứng dụng sử dụng core 0 làm Rx/Dispatcher, core 1->4 làm Worker
./build/spifast -l 0-4 -n 4 --vdev "net_pcap0,rx_pcap=traffic_sample.pcap,tx_pcap=out_drop.pcap" -- -r spi_rules.conf
```

# 7. Quy trình triển khai chi tiết cho sinh viên

- **Bước 1:** Cấu hình Hugepages trên Ubuntu Linux/PC cá nhân.
- **Bước 2:** Sử dụng Wireshark ghi lại một file traffic_sample.pcap chứa hỗn hợp các gói tin HTTP (port 80), HTTPS (port 443), DNS (port 53), SSH (port 22) để chuẩn bị bộ dữ liệu đầu vào mạng.
- **Bước 3:** Viết mã nguồn C khởi tạo môi trường DPDK EAL, tạo bộ nhớ đệm Mempool kích thước vừa phải thích hợp cho PC (ví dụ 8192 hoặc 16384 mbufs).
- **Bước 4:** Hiện thực cấu trúc tệp tin Rule Parser đọc tệp tin cấu hình spi_rules.conf.
- **Bước 5:** Phát triển logic bóc tách Header Parser (Zero-copy ép kiểu con trỏ mbuf sang các struct header mạng có sẵn của DPDK) và thuật toán so khớp First Match.
- **Bước 6:** Tạo liên kết đa luồng đa lõi bằng hàm `rte_eal_remote_launch()` kết hợp truyền mbuf qua `rte_ring`.
- **Bước 7:** Viết hàm Statistics xuất ra màn hình console định kỳ 1 giây các giá trị quy đổi Mbps, pps, bộ đếm hit của từng Rule mạng để nghiệm thu kết quả.

# 8. Tiêu chí đánh giá & Kết quả bàn giao yêu cầu

- Mã nguồn C chuẩn (Gồm tệp cấu hình Makefile/CMakeLists.txt) có thể compile sạch sẽ không lỗi (Warning/Error) trên môi trường GCC Linux.
- File testcase (Excel): Function test & performance test.
- Tài liệu phân tích hệ thống giải trình rõ ràng các hàm API DPDK đã sử dụng và sơ đồ ánh xạ điều hướng gói tin giữa các CPU lcores.
- Bảng thống kê kết quả chạy thực tế trên máy in ra từ ứng dụng, minh chứng rõ ràng việc hệ thống đạt được KPIs phân loại dữ liệu (pps, Mbps) tương ứng với môi trường giả lập mạng 1 Gbps.


# 9. Lưu ý quan trọng*
> **Về cấu hình Action trong file `spi_rules.conf`:** > Các giá trị hành động như `FORWARD_WORKER_0`, `FORWARD_WORKER_1`... trong file mẫu chỉ mang tính chất **ví dụ minh họa** để hình dung luồng đi của dữ liệu.
> Hệ thống **không nhất thiết và không bắt buộc phải gán cứng** cố định một giao thức hay một luật mạng vào một Core xử lý duy nhất. Học viên được khuyến khích tự do tối ưu hóa hiệu năng bằng các giải pháp nâng cao khác, ví dụ như cơ chế **Cân bằng tải động (Dynamic Load Balancing)** giữa các Worker Core (qua thuật toán Round-Robin, RSS Hash, hoặc Shared Ring) nhằm tận dụng tối đa tài nguyên CPU và tránh hiện tượng rơi gói tin (Packet Drop).