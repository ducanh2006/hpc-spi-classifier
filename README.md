Dưới đây là toàn bộ nội dung file README của bạn đã được dịch sang tiếng Việt, sử dụng các thuật ngữ chuyên ngành Công nghệ thông tin, Mạng máy tính và Điện toán hiệu năng cao (HPC) một cách chuẩn xác và mượt mà:

---

# SPIFast - Hệ thống phân loại gói tin hiệu năng cao dùng DPDK

SPIFast là một hệ thống Kiểm tra gói tin nông (Shallow Packet Inspection - SPI) hiệu năng cao được xây dựng bằng ngôn ngữ C11 và thư viện DPDK. Hệ thống thực hiện phân loại lưu lượng mạng dựa trên các trường tiêu đề 5-tuple (IP nguồn, IP đích, Cổng nguồn, Cổng đích, Giao thức) ở tốc độ đường truyền Gigabit (Line-rate) nhờ vào kiến trúc đường ống đa lõi không dùng khóa (lock-free multi-core pipeline architecture).

## 🚀 Tính năng nổi bật

* **Kiến trúc Không-sao-chép (Zero-Copy):** Phân tích cú pháp gói tin trực tiếp ngay trên bộ nhớ (in-place) sử dụng các phân vùng mempool của DPDK.
* **Đa luồng Không-dùng-khóa (Lock-Free Multi-Threading):** Các lõi Master (Nhận/Phân phối) và Worker giao tiếp trực tiếp với nhau thông qua hàng đợi `rte_ring`.
* **Cân bằng tải động:** Cơ chế phần mềm RSS (Sử dụng hàm băm Jenkins Hash) đảm bảo tính đồng nhất luồng dữ liệu (flow affinity) một cách định trước.
* **Cập nhật luật thời gian thực (Hot-Reloadable):** Cập nhật bộ luật phân loại ngay khi ứng dụng đang chạy thông qua cơ chế Unix Domain Sockets mà không cần phải khởi động lại hệ thống.

---

## 🛠️ Yêu cầu hệ thống

* Hệ điều hành Linux (Khuyến nghị dùng Ubuntu 20.04/22.04/24.04)
* Các công cụ: GCC, Make, Python 3, `tcpreplay`
* Trình quản lý build Meson (>= 1.3.2) và Ninja
* DPDK 24.11 (được cài đặt cục bộ thông qua script đi kèm)

---

## ⚙️ 1. Cài đặt và Cấu hình dự án

Thực hiện chính xác theo các bước sau để thiết lập môi trường, biên dịch DPDK và cấp phát bộ nhớ Hugepages.

### Bước 1.1: Cài đặt các gói phụ thuộc (Dependencies)

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-pip python3-venv tcpreplay pkg-config cmake
pip3 install meson ninja

```

### Bước 1.2: Thiết lập môi trường ảo Python (Phục vụ cho các script sinh test)

```bash
python3 -m venv venv
source venv/bin/activate
pip install scapy

```

### Bước 1.3: Tải và Biên dịch DPDK

Dự án đã tích hợp sẵn một script tự động để tải và biên dịch DPDK cục bộ ngay bên trong thư mục `third_party/`.

```bash
chmod +x scripts/setup_dpdk/setup_dpdk.sh
./scripts/setup_dpdk/setup_dpdk.sh

```

### Bước 1.4: Cấp phát bộ nhớ Hugepages (Bắt buộc)

DPDK yêu cầu bộ nhớ Hugepages để thiết lập các pool bộ nhớ không-sao-chép (zero-copy memory pool). **Bạn bắt buộc phải chạy lệnh này sau mỗi lần khởi động lại máy tính.**

```bash
chmod +x scripts/setup_hugepages/*.sh
sudo ./scripts/setup_hugepages/setup_hugepages.sh

```

---

## 🏗️ 2. Biên dịch dự án

Cấu hình thư mục build bằng `meson` và thiết lập biến môi trường `PKG_CONFIG_PATH` trỏ tới phân vùng DPDK vừa biên dịch cục bộ.

```bash
# Xuất biến PKG_CONFIG_PATH để meson tìm thấy bản DPDK cục bộ của chúng ta
export PKG_CONFIG_PATH="$PWD/third_party/dpdk-24.11/build/meson-uninstalled"

# Thiết lập thư mục build
meson setup build

# Tiến hành biên dịch dự án
ninja -C build

```

Sau khi biên dịch thành công, 3 file thực thi (binary) sẽ được tạo ra trong thư mục `build/`:

* `spifast`: Ứng dụng phân loại SPI bản Production đã được tối ưu hóa kịch khung.
* `spifast_debug`: Phiên bản chạy ở chế độ Debug kèm theo các cờ kiểm thử tính đúng đắn của chức năng.
* `spi_cli`: Công cụ dòng lệnh hỗ trợ nạp lại cấu hình thời gian thực (live reload).

---

## 🏃 3. Vận hành dự án

### Chạy ở chế độ Production / Native Mode

Mở ứng dụng trực tiếp bằng các script đo kiểm đi kèm. Các script này đã tự động xử lý các tham số EAL của DPDK và cấu hình map thiết bị mạng ảo PCAP `vdev` một cách chuẩn xác.

```bash
sudo ./tests/judge/run_project_native.sh

```

### Chạy ở chế độ truyền dữ liệu với `tcpreplay` (Dành cho file PCAP lớn)

```bash
sudo ./tests/judge/run_project_tcpreplay.sh

```

### Cập nhật luật thời gian thực (Hot-Reloading)

Trong khi ứng dụng `spifast` vẫn đang chạy, bạn hãy mở một cửa sổ terminal mới. Bạn có thể chỉnh sửa file luật `spi_rules.conf` và tiến hành nạp trực tiếp bộ luật mới vào hệ thống mà không làm rớt bất kỳ gói tin nào:

```bash
sudo ./build/spi_cli reload_rules ./spi_rules.conf

```

---

## 🧪 4. Đo kiểm hiệu năng (Benchmarking) & Kiểm thử tính đúng đắn

Thư mục `tests/judge/` chứa toàn bộ các bộ kiểm thử tự động.
*Lưu ý: Đảm bảo rằng bạn đã kích hoạt môi trường ảo Python `venv` trước khi chạy các bài test tính đúng đắn, vì kịch bản này cần thư viện `scapy` để sinh dữ liệu.*

### Kiểm tra tính đúng đắn của chức năng (Functional Correctness)

Kịch bản này sẽ tự động sinh ra 240 gói tin định trước bao phủ toàn bộ các trường hợp biên và kiểm thử lỗi, đẩy chúng qua đường ống xử lý SPI, sau đó đối chiếu kết quả thực tế thu được với kết quả kỳ vọng ban đầu.

```bash
# Hãy chắc chắn rằng venv đã được kích hoạt trước!
source venv/bin/activate
sudo ./tests/judge/run_check_correctness.sh

```

### Chạy đo kiểm hiệu năng hệ thống (Performance Benchmarks)

Hệ thống cung cấp hai chế độ đo hiệu năng tùy thuộc vào dung lượng file PCAP của bạn:

```bash
# 1. Native Mode (Dành cho file PCAP nhỏ < 8K gói tin)
# Đạt hiệu năng kịch trần (line-rate) nhờ việc bypass (bỏ qua) hoàn toàn nhân Linux kernel.
sudo ./tests/judge/run_benchmark_native.sh

# 2. TCPReplay Mode (Dành cho file PCAP lớn trên 1M gói tin)
# Truyền luồng dữ liệu thông qua giao tiếp mạng ảo (veth). Tránh được lỗi cạn kiệt bộ nhớ mbuf pool của DPDK.
sudo ./tests/judge/run_benchmark_tcpreplay.sh

```

### Kết quả đầu ra (Results Output)

Toán bộ file log chi tiết và kết quả đo kiểm hiệu năng sẽ được tự động xuất ra thư mục `tests/results/` dưới dạng các file định dạng `.csv` và `_log.txt`.

Để xem hướng dẫn chi tiết về khung kiểm thử, vui lòng tham khảo [Tài liệu hướng dẫn đo kiểm (Benchmarking Guide)](tests/judge/README.md).