# **MESON & NINJA — HỆ THỐNG BUILD CỦA SPIFAST**

---

## **1. Tổng quan: Meson + Ninja là gì?**

| Công cụ | Vai trò |
|---------|---------|
| **Meson** | Build *system generator* — đọc file `meson.build`, phân tích dependency, tạo ra các file build tối ưu cho Ninja |
| **Ninja** | Build *executor* — đọc file Ninja được Meson tạo ra và thực sự biên dịch, link code cực nhanh (song song) |

**Tại sao không dùng Makefile?**
- Ninja chạy song song theo CPU cores mặc định, không cần viết rule parallel.
- Meson tự phát hiện DPDK qua `pkg-config`, không cần viết đường dẫn thủ công.
- Meson có `buildtype=release` / `debug` tích hợp sẵn.

---

## **2. Phân tích file `meson.build`**

```meson
project('spifast', 'c',
    version: '2.0.0',
    default_options: [
        'buildtype=release',   # Tối ưu hóa production (-O2 + strip debug info)
        'c_std=gnu11',         # Dùng C11 + GNU extensions (cho _Atomic, __builtin_expect...)
        'warning_level=3',     # Bật tất cả warning (tương đương -Wall -Wextra -Wpedantic)
        'b_lto=true'           # Link-Time Optimization: compiler tối ưu xuyên file .c
    ]
)
```

### **2.1. Tìm kiếm DPDK dependency**

```meson
# Bước 1: Thử tìm DPDK đã cài system-wide (/usr/lib, /usr/local/lib...)
dpdk_dep = dependency('libdpdk', required: false)

# Bước 2: Nếu không có, thử DPDK local trong third_party/
if not dpdk_dep.found()
    local_pc_dir = 'third_party/dpdk-24.11/build/meson-uninstalled'
    # Gọi pkg-config với PKG_CONFIG_PATH trỏ vào thư mục build local
    cflags_res = run_command('env', 'PKG_CONFIG_PATH=...', 'pkg-config', '--cflags', 'libdpdk')
    libs_res   = run_command('env', 'PKG_CONFIG_PATH=...', 'pkg-config', '--libs',   'libdpdk')
    dpdk_dep = declare_dependency(
        compile_args: cflags_res.stdout().strip().split(),
        link_args:    libs_res.stdout().strip().split()
    )
endif
```

### **2.2. Ba executable được build**

| Executable | File nguồn | Mục đích | Flag đặc biệt |
|:---|:---|:---|:---|
| `spifast` | `src/*.c` (trừ `spi_cli.c`) | Production binary | `-Ofast -march=native -mtune=native` |
| `spifast_debug` | Giống `spifast` | Debug/Kiểm thử chức năng | Thêm `-DDEBUG_MODE` |
| `spi_cli` | `src/spi_cli.c` | CLI gửi lệnh reload | Không cần DPDK |

### **2.3. Ý nghĩa các C flag tối ưu hóa**

```
-march=native     : Dùng tập lệnh CPU của máy hiện tại (AVX2, AVX-512...)
                    Quan trọng để DPDK ACL bật SIMD tự động
-mtune=native     : Tối ưu lịch lệnh (instruction scheduling) cho CPU hiện tại
-Ofast            : Mạnh hơn -O3, bật thêm các tối ưu có thể vi phạm IEEE 754
                    (acceptable vì không cần floating-point chính xác tuyệt đối)
-funroll-loops    : Mở vòng lặp nhỏ thành chuỗi lệnh thẳng, giảm overhead branch
-fomit-frame-pointer : Giải phóng thêm 1 register (rbp) cho compiler dùng
-falign-functions=64 : Căn chỉnh hàm vào biên 64-byte (1 cache line), tránh split
-DDEBUG_MODE      : Macro kích hoạt code ghi log CSV trong worker.c và master.c
-b_lto=true       : Link-Time Optimization — trình biên dịch nhìn toàn bộ project
                    khi link, có thể inline hàm xuyên file .c
```

---

## **3. Quy trình Build từ đầu đến cuối**

### **Bước 1: Setup Hugepages (một lần duy nhất)**
```bash
# Cấp phát 1024 trang 2MB = 2GB RAM cho DPDK
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages

# Kiểm tra đã cấp phát thành công
cat /proc/meminfo | grep Huge
# HugePages_Total:    1024
# HugePages_Free:     1024
```

### **Bước 2: Cấu hình build directory (chỉ chạy 1 lần)**
```bash
# Từ thư mục gốc dự án
meson setup build

# Hoặc build debug
meson setup build --buildtype=debug
```

Meson sẽ:
1. Đọc `meson.build`
2. Tìm DPDK (system → local `third_party/`)
3. Tạo thư mục `build/` với Ninja build files

### **Bước 3: Biên dịch**
```bash
# Build tất cả 3 executables
meson compile -C build

# Hoặc chỉ build 1 target cụ thể
meson compile -C build spifast
meson compile -C build spifast_debug
meson compile -C build spi_cli
```

### **Bước 4: Clean & Rebuild**
```bash
# Xóa toàn bộ thư mục build và cấu hình lại
rm -rf build/
meson setup build
meson compile -C build
```

---

## **4. Cấu trúc thư mục sau khi build**

```
build/
├── spifast          ← Production binary (dùng chạy benchmark)
├── spifast_debug    ← Debug binary (dùng kiểm thử chức năng)
├── spi_cli          ← CLI tool (gửi lệnh reload rules)
├── build.ninja      ← File Ninja rules (do Meson tạo ra, không chỉnh tay)
└── compile_commands.json  ← Cho IDE (clangd, VSCode) biết compile flags
```

---

## **5. Lỗi thường gặp khi Build**

### **Lỗi: `Dependency "libdpdk" not found`**
```
Nguyên nhân: DPDK chưa được build trong third_party/
Giải pháp:
  cd third_party/dpdk-24.11
  meson setup build
  meson compile -C build
```

### **Lỗi: `pkg-config not found`**
```bash
sudo apt install pkg-config
```

### **Lỗi: `Cannot create mbuf pool` khi chạy**
```
Nguyên nhân: Hugepages chưa được cấp phát
Giải pháp: echo 1024 | sudo tee /proc/sys/vm/nr_hugepages
```

### **Lỗi: `No Ethernet ports - bye`**
```
Nguyên nhân: DPDK không tìm thấy vdev → thiếu tham số --vdev
Giải pháp: Dùng đúng lệnh từ script .sh có --vdev "net_pcap0,..."
```

### **Lỗi: `world-writable directory` (EAL security check)**
```
Nguyên nhân: DPDK từ chối load driver từ thư mục có permission 777
Giải pháp: Các script .sh đã copy driver vào /opt/spifast_dpdk_drivers/ (chmod 755)
           và truyền vào với tham số -d "$TMP_DRIVER_DIR"
```

---

## **6. Tham số dòng lệnh EAL — Giải thích chi tiết**

Lệnh chạy đầy đủ (từ `run_project_native.sh`):
```bash
./build/spifast \
  -d "/opt/spifast_dpdk_drivers" \   # Thư mục chứa PMD driver (.so)
  -l 0-4 \                            # Dùng lcore 0, 1, 2, 3, 4
  -n 4 \                              # 4 memory channels
  --vdev "net_pcap0,rx_pcap=FILE.pcap,infinite_rx=1" \  # Virtual NIC đọc từ PCAP
  -- \                                # Phân tách EAL args và App args
  -r "./spi_rules.conf"               # (App arg) Đường dẫn file luật
```

| Tham số EAL | Ý nghĩa |
|:---|:---|
| `-d DIR` | Load PMD driver từ thư mục DIR (bảo mật, phải chmod 755) |
| `-l 0-4` | Pin vào lcore 0, 1, 2, 3, 4 (Master + 4 Workers) |
| `-n 4` | Khai báo số memory channel của RAM (tối ưu băng thông bộ nhớ) |
| `--vdev "net_pcap0,rx_pcap=FILE,infinite_rx=1"` | Tạo NIC ảo đọc từ file PCAP, lặp vô hạn |
| `--vdev "net_pcap0,rx_iface=veth0"` | Tạo NIC ảo đọc từ interface Linux (dùng cho TCPReplay mode) |
| `--` | Dấu phân tách — EAL xử lý mọi thứ trước đây, app tự parse sau đây |

---

## **7. Sự khác biệt giữa 3 Executable**

```bash
# spifast — Production: In stats mỗi giây, chạy vô hạn đến Ctrl+C
sudo ./build/spifast -d /opt/spifast_dpdk_drivers -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_pcap=file.pcap,infinite_rx=1" -- -r spi_rules.conf

# spifast_debug — Debug: Ghi log phân loại từng gói ra CSV, tự dừng khi hết PCAP
# (chỉ dùng -l 0-1 vì DEBUG_MODE chỉ ghi log trên Worker 0)
sudo ./build/spifast_debug -d /opt/spifast_dpdk_drivers -l 0-1 \
  --vdev "net_pcap0,rx_pcap=func_test.pcap" -- -r spi_rules.conf

# spi_cli — CLI: Gửi lệnh reload rules đến spifast đang chạy
sudo ./build/spi_cli reload_rules ./spi_rules.conf
```
