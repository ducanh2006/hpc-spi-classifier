# **KIẾN TRÚC VÀ THUẬT TOÁN HỆ THỐNG SPIFAST**
## *(Tài liệu ôn thi hội đồng)*

---

## **1. Mô hình Pipeline Đa lõi Lock-free**

### **1.1. Tại sao không dùng Mutex/Spinlock?**

| Cơ chế | Vấn đề |
|--------|--------|
| **Mutex** | Khi một thread giữ lock, tất cả thread còn lại phải ngủ (sleep). Linux Kernel phải context-switch, tốn hàng ngàn chu kỳ CPU. |
| **Spinlock** | Thread không ngủ mà quay vòng (spin) liên tục kiểm tra. Hao CPU 100% và gây cache-line bouncing giữa các core. |
| **Lock-free (`rte_ring`)** | Dùng con trỏ nguyên tử + memory barrier. Không có core nào bị chặn, không có context switch. |

> **Câu hỏi hội đồng hay hỏi:** *"Tại sao không dùng mutex thông thường?"*
>
> **Trả lời:** Ở tốc độ 10-27 Mpps, mỗi microsecond xử lý ~10.000-27.000 gói tin. Một lần mutex lock/unlock tốn ~200-500ns = ~2.000-5.000 gói bị delay. Với kiến trúc lock-free SP/SC ring, overhead gần như bằng 0 vì chỉ cần 1 lệnh memory barrier.

### **1.2. Kiến trúc phân tầng nhiệm vụ**

```
PCAP Virtual NIC (librte_pmd_pcap / AF_PACKET)
          │
          │ rte_eth_rx_burst (BURST_SIZE=64 pkts)
          ▼
 MASTER CORE — Lcore 0 (Rx / Dispatch)
   1. parse_five_tuple()   → Zero-copy header extraction
   2. rte_jhash_3words()   → Flow Hash (Software RSS)
   3. hash & (N-1)         → target_worker (Fast Modulo)
   4. rte_ring_enqueue_burst() → Đẩy vào ring của Worker
          │
  ┌───────┼───────┬───────┐
  ▼       ▼       ▼       ▼
Ring0  Ring1  Ring2  Ring3  (SP/SC, Lock-free)
  │       │       │       │
  ▼       ▼       ▼       ▼
Worker0 Worker1 Worker2 Worker3  — Lcore 1-4 (Classify / Free)
  │
  ├── rte_acl_classify()    → SIMD AVX2/AVX-512 batch lookup
  ├── Cập nhật rule_hits, drop_count (per-core stats)
  └── rte_pktmbuf_free_bulk() → Trả mbuf về Mempool
```

**Control Thread (OS pthread, không pin vào lcore DPDK):**
```
spi_cli → Unix Domain Socket → ctrl_thread_fn → matcher_reload() → Atomic Swap
```

### **1.3. Tại sao Master Core không làm phân loại?**

Phân loại bằng DPDK ACL tốn CPU (xây dựng Trie, SIMD lookup). Nếu Master vừa nhận gói vừa phân loại, nó sẽ trở thành bottleneck. Tách Master (Rx/Dispatch) và Worker (Classify/Free) cho phép cả hai chạy song song hoàn toàn trên các lõi vật lý riêng biệt.

---

## **2. Thuật toán Software RSS — Jenkins Hash (`rte_jhash_3words`)**

### **2.1. Tại sao cần Flow Affinity?**

Nếu Master phân phối Round-Robin đơn giản (gói 1→W0, gói 2→W1, ...), các gói của cùng một kết nối TCP sẽ đi đến các Worker khác nhau. Mỗi Worker có cache L1/L2 riêng → không có trạng thái kết nối trong cache → **Cache Miss liên tục**.

**Software RSS giải quyết:** Tất cả gói của cùng một flow (cùng 5-tuple) **luôn đi về cùng một Worker**, giúp dữ liệu flow đó luôn nóng trong L1/L2 cache.

### **2.2. Công thức tính target worker**

```c
uint32_t hash = rte_jhash_3words(
    meta->tuple.src_ip,                            // word 1
    meta->tuple.dst_ip,                            // word 2
    ((uint32_t)meta->tuple.src_port << 16)         // word 3: ghép 2 port vào 1 uint32
        | meta->tuple.dst_port,
    meta->tuple.protocol                           // initval / seed
);
target_worker = hash & (num_workers - 1);  // Fast Modulo
```

### **2.3. Fast Modulo — Tại sao `& (N-1)` thay vì `% N`?**

Phép chia lấy dư (`%`) tốn 20-90 chu kỳ CPU (lệnh `IDIV`).

Khi `N` là **lũy thừa của 2** (N=4, N=8...), `& (N-1)` hoàn toàn tương đương:

| Phép toán | Kết quả | Chu kỳ CPU |
|:---:|:---:|:---:|
| `hash % 4` | = `hash & 3` | ~40 cycles |
| `hash & (4-1)` | = `hash & 0b011` | **1 cycle** |

SPIFast cố định `MAX_WORKERS = 4` (lũy thừa 2) để tối ưu phép toán này.

---

## **3. Cơ chế Hot-Reload — Double-Buffering Không khóa**

### **3.1. Vấn đề cần giải quyết**

Khi cần cập nhật bảng luật mới, không thể:
- Dừng toàn bộ hệ thống → downtime, rớt gói.
- Ghi trực tiếp vào bảng luật đang dùng → Worker đang đọc đồng thời → data race → crash hoặc phân loại sai.

### **3.2. Giải pháp: Double-Buffering + Atomic Pointer Swap**

```
g_rule_table_a  ←─── g_active_rules ───→  Worker đang đọc (Bảng A)
g_rule_table_b  ←─── (rảnh, shadow)
```

**Quy trình reload trong `matcher_reload()`:**
1. `current = atomic_load(g_active_rules, relaxed)` → biết đang dùng bảng nào.
2. `shadow = (current == A) ? B : A` → lấy bảng rảnh.
3. `parse_rules_into(file, shadow, ...)` → ghi vào shadow (Worker không đọc).
4. `build_acl_context(shadow, ...)` → build `new_ctx` ACL.
5. **Atomic swap** (memory_order_release) — thứ tự quan trọng:
   ```c
   atomic_store(&g_active_num_rules, new_count, memory_order_release);
   atomic_store(&g_active_rules,     shadow,    memory_order_release);
   atomic_store(&g_active_acl_ctx,   new_ctx,   memory_order_release);
   ```
6. `usleep(50000)` → Grace Period 50ms (đợi Worker hoàn thành burst hiện tại).
7. `rte_acl_free(old_ctx)` → giải phóng context cũ.

### **3.3. Tại sao `memory_order_release` / `memory_order_acquire`?**

- **Release (Writer):** Đảm bảo tất cả lệnh **ghi vào shadow table hoàn thành trước** khi swap con trỏ. CPU không được reorder ghi sau swap.
- **Acquire (Reader/Worker):** Đảm bảo Worker thấy **đầy đủ dữ liệu** mà control thread đã ghi vào shadow sau khi đọc con trỏ mới.

> **Câu hỏi hay hỏi:** *"Grace Period 50ms có đủ không?"*
>
> **Trả lời:** BURST_SIZE = 64 gói. Ở 27 Mpps, xử lý 64 gói mất ~2.4 µs. Grace Period 50ms = 50.000 µs, gấp **20.000 lần**. Rất an toàn. Nếu cần strict guarantee, có thể dùng RCU (Read-Copy-Update) với epoch counter, nhưng với bài toán này 50ms là quá đủ.

---

## **4. DPDK ACL — Phân loại gói tin bằng SIMD**

### **4.1. Tại sao không dùng vòng lặp if-else?**

Với 128 luật và 27M gói/giây → `128 × 27M = 3,456 tỷ phép so sánh/giây`. Bất khả thi với CPU đơn luồng.

### **4.2. Cách DPDK ACL hoạt động**

`librte_acl` xây dựng cấu trúc **Deterministic Finite Automaton (DFA/Trie)** từ tập luật 5-tuple. Mỗi gói tin duyệt cây **một lần duy nhất** theo các byte header → tìm được luật khớp nhất.

Hàm `rte_acl_classify()` xử lý **nhiều gói đồng thời** bằng SIMD:
- **AVX2:** So khớp 8 gói song song trong 1 lệnh 256-bit.
- **AVX-512:** So khớp 16 gói song song trong 1 lệnh 512-bit.

```c
// SPIFast: phân loại 64 gói trong 1 lần gọi
rte_acl_classify(acl_ctx, data_ptrs, results, num_valid, 1);
// results[i] = 0 → no match (DEFAULT DROP)
// results[i] = k → khớp rules[k-1]
```

### **4.3. Ánh xạ Priority**

DPDK ACL: **priority lớn hơn = ưu tiên cao hơn** (ngược với trực giác thông thường).

```
priority = 1000 - precedence
```

| Precedence | Priority ACL | Ý nghĩa |
|:---:|:---:|:---|
| 1 (cao nhất) | 999 | Luật cụ thể nhất, khớp đầu tiên |
| 100 | 900 | Luật thông thường |
| 1000 (thấp nhất) | 0 | Luật default-drop |

**Tại sao mốc 1000?** Giữ priority trong vùng an toàn [0, 999]. Nếu `precedence > 1000` thì `1000 - precedence` sẽ **underflow** (tràn số nguyên không dấu), biến giá trị âm thành `~4.29 tỷ` → luật ưu tiên thấp thành cao nhất.

### **4.4. Cấu trúc userdata — Cách Worker biết luật nào khớp**

```c
ar->data.userdata = i + 1;  // Lưu (rule_index + 1), để 0 = "no match"
```

Sau `rte_acl_classify()`:
```c
uint32_t res = results[i];
if (res > 0) {
    uint32_t rule_idx = res - 1;        // Chuyển về 0-based index
    if (rules[rule_idx].action_mask == ACTION_DROP)
        w_drop_pkts++;
} else {
    w_drop_pkts++;  // DEFAULT DROP
}
```

---

## **5. Zero-copy Parser (`parse_five_tuple`)**

### **5.1. Vấn đề của memcpy ở tốc độ cao**

Với 27M gói/giây × 54 bytes mỗi header = **~1.5 GB/s** chỉ riêng cho việc copy header. Mỗi lệnh `memcpy` còn làm "bẩn" cache (evict dữ liệu hữu ích). Kết hợp lại → bottleneck nghiêm trọng.

### **5.2. Zero-copy: Ép kiểu con trỏ trực tiếp**

```c
// Dữ liệu vẫn nằm tại chỗ trong Hugepages
// CPU chỉ đặt con trỏ trỏ tới đúng vị trí offset
struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
struct rte_ipv4_hdr  *ip_hdr  = (struct rte_ipv4_hdr *)((uint8_t *)eth_hdr + l3_offset);
struct rte_tcp_hdr   *tcp_hdr = (struct rte_tcp_hdr *)((uint8_t *)eth_hdr + l4_offset);
tuple->src_ip  = ip_hdr->src_addr;   // Đọc trực tiếp, 0 copy
tuple->dst_ip  = ip_hdr->dst_addr;
```

**0 byte được copy**. Đây là lý do hàm được khai báo `static inline __attribute__((always_inline))` — compiler dán thẳng assembly vào vòng lặp của Master, không có overhead function call.

### **5.3. Hỗ trợ VLAN (802.1Q)**

```c
if (ether_type == RTE_ETHER_TYPE_VLAN) {
    struct rte_vlan_hdr *vlan = (struct rte_vlan_hdr *)((uint8_t *)eth_hdr + l3_offset);
    ether_type = vlan->eth_proto;        // EtherType thực sau VLAN tag
    l3_offset += sizeof(struct rte_vlan_hdr);  // +4 bytes
}
```

---

## **6. Kỹ thuật Memory Prefetching**

### **6.1. Vấn đề: CPU Cache Miss**

RAM vật lý có độ trễ ~100ns = ~300 chu kỳ CPU. Nếu dữ liệu gói tin chưa trong cache khi CPU cần xử lý → **CPU stall 300 chu kỳ** (đứng chờ RAM).

### **6.2. Giải pháp: Nạp trước i+4**

```c
for (uint16_t i = 0; i < nb_rx; i++) {
    // Gợi ý CPU: "Hãy nạp gói i+4 vào L1 cache ngay bây giờ"
    if (likely(i + 4 < nb_rx)) {
        rte_prefetch0(bufs[i + 4]);                          // Prefetch mbuf metadata
        rte_prefetch0(rte_pktmbuf_mtod(bufs[i + 4], void*)); // Prefetch packet data
    }
    // Xử lý gói i — dữ liệu đã được prefetch từ 4 iteration trước
    process_packet(bufs[i]);
}
```

**Tại sao i+4?** Độ trễ prefetch trung bình ~4-12 chu kỳ. Pipeline 64 gói: prefetch i+4 tạo "thời gian trống" đủ để RAM trả về dữ liệu trước khi CPU cần. Áp dụng ở cả Master Core (trong `master_loop`) và Worker Core (trong `worker_loop`).

---

## **7. Thống kê Per-core (Cache-aligned, Không khóa)**

### **7.1. False Sharing là gì?**

Nếu nhiều core ghi vào các biến gần nhau trong bộ nhớ (cùng cache line 64 bytes):
- Core A ghi → invalidate cache line trên Core B, C, D.
- Các core B, C, D phải re-fetch từ RAM (cache miss).
- Gọi là **False Sharing** — chia sẻ cache line vô tình dù không cùng dữ liệu.

### **7.2. Giải pháp trong SPIFast**

```c
typedef struct {
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t dropped_packets;
    uint64_t rule_hits[MAX_RULES];
} __rte_cache_aligned worker_stats_t;  // Mỗi struct chiếm ít nhất 1 cache line riêng
```

Mỗi Worker chỉ ghi vào `g_worker_stats[worker_id]` của riêng mình. Stats thread đọc tổng hợp 1 lần/giây. Không có racing, không có locking.

---

## **8. Format File Cấu hình Luật (`spi_rules.conf`)**

```ini
[GROUPS_SECTION]
# Group_Name, Precedence, Action
fg_l34_facebook,100,FORWARD
fg_l34_udp_sdf1006,106,DROP

[FILTERS_SECTION]
# Filter_Name, Group, Proto, SrcIP/Mask, DstIP/Mask, SrcPort, DstPort
f_l34_facebook_1, fg_l34_facebook, *, *, 31.13.64.0/18, *, *
f_l34_http_all,   fg_l34_http,    tcp, *,           *, *,  80
f_l34_dns_udp,    fg_l34_dns,     udp, *,           *, *,  53
```

**Wildcard:** `*` hoặc `ANY` → bất kỳ giá trị nào (IP mask = 0, port range = 0-65535).

**Thứ tự khớp:** Luật có precedence nhỏ hơn (priority ACL lớn hơn) được ưu tiên. Nếu không khớp luật nào → DEFAULT DROP.

---

## **9. Câu hỏi Hội đồng Thường gặp & Gợi ý Trả lời**

### **Q1: DPDK khác gì Linux Kernel network stack?**

| | Linux Kernel | DPDK |
|---|---|---|
| **I/O model** | Interrupt-driven | Polling (busy-wait 100% CPU) |
| **Memory** | sk_buff, kernel↔user memcpy | Hugepages, Zero-copy |
| **Threading** | Kernel scheduler, context switch | Core pinning, no preemption |
| **Tốc độ** | ~1-5 Mpps | 10-100+ Mpps |
| **Chi phí CPU** | Thấp khi idle | Cao cố định (polling) |

### **Q2: Hugepages là gì? Tại sao cần?**

RAM chia thành pages 4KB. Mỗi truy cập địa chỉ ảo → MMU tra TLB (Translation Lookaside Buffer). TLB chỉ có ~64-1024 entries. Với 4KB pages và 2GB data = 500.000 pages → TLB miss liên tục → mỗi miss tốn ~100 chu kỳ tra page table.

Hugepages 2MB: Cùng 2GB data = chỉ **1024 pages** → TLB luôn trúng → không bao giờ bị TLB miss với dữ liệu gói tin.

### **Q3: Hệ thống có thể xử lý IPv6 không?**

Hiện tại chỉ hỗ trợ `RTE_ETHER_TYPE_IPV4`. Để hỗ trợ IPv6 cần:
1. Thêm nhánh xử lý `RTE_ETHER_TYPE_IPV6` trong `parse_five_tuple()`.
2. Mở rộng `five_tuple_t`: `uint32_t` src_ip/dst_ip → `uint8_t[16]`.
3. Cấu hình lại DPDK ACL field definitions với `size = 16` cho IP fields.

### **Q4: Worker Drop Rate có thực sự = 0% không?**

**Không hoàn toàn đúng.** Ở chế độ TCPReplay Mode với file PCAP lớn, kết quả thực tế từ `benchmark_tcpreplay_summary.csv`:
- `balanced_traffic.pcap`: ~3.89M worker drop packets
- `telco_traffic.pcap`: ~4.2M worker drop packets

**Master Drop = 0** (hoàn toàn đúng — ring buffer không bao giờ đầy ở tốc độ tcpreplay). Nhưng Worker Ring bị đầy vì tcpreplay bơm gói nhanh hơn Worker tiêu thụ. **Điểm cần làm rõ trong báo cáo.**

### **Q5: Tại sao chọn BURST_SIZE = 64?**

- **SIMD hiệu quả:** AVX2 xử lý 8 gói/lệnh → 8 lần × 8 = 64 gói/lần gọi `rte_acl_classify`.
- **Latency vs Throughput:** Lớn hơn 64 tăng throughput nhưng tăng độ trễ mỗi gói. 64 là điểm cân bằng tốt.
- **Cache friendly:** 64 con trỏ mbuf = 64 × 8 = 512 bytes = 8 cache lines, vừa fit trong L1 cache.

### **Q6: Control Thread có cần pin vào CPU core không?**

**Không cần.** Control Thread là OS pthread thông thường, chỉ hoạt động khi CLI gửi lệnh (hầu hết thời gian nó đang `accept()` blocking — tốn 0% CPU). Thiết kế này đảm bảo **Control Plane** (quản trị) và **Data Plane** (xử lý gói) hoàn toàn tách biệt về tài nguyên CPU.

### **Q7: Nếu file luật mới bị lỗi syntax, hệ thống sẽ ra sao?**

`parse_rules_into()` trả về `-1`. `matcher_reload()` không swap con trỏ, trả về `-1`. Control thread gửi `"ERROR: failed to load ..."` về CLI. **Bảng luật cũ vẫn tiếp tục hoạt động** — không có downtime, không mất gói. Đây là một trong các ưu điểm thiết kế của Double-Buffering.

### **Q8: Tại sao dùng Unix Domain Socket thay vì TCP?**

- **Không có network overhead** (không qua TCP/IP stack).
- **Nhanh hơn:** Giao tiếp qua kernel memory trực tiếp.
- **Bảo mật hơn:** Chỉ tiến trình local có quyền truy cập `/tmp/spifast_ctrl.sock` (kiểm soát qua file permission).
- **Đơn giản hơn:** Không cần bind port, không lo conflict.

### **Q9: SPI khác DPI như thế nào? Khi nào dùng cái nào?**

| | SPI (Shallow) | DPI (Deep) |
|---|---|---|
| **Kiểm tra đến tầng** | L2, L3, L4 (Header) | L7 (Payload) |
| **Tốc độ** | Rất cao (10-100+ Mpps) | Thấp hơn (~1-10 Mpps) |
| **CPU** | Thấp | Cao (regex scanning, pattern matching) |
| **Use case** | Firewall, Load Balancer, UPF | IDS/IPS, Content filtering, DRM |

**SPIFast** dùng SPI để lọc thô L3/L4 trước. Nếu cần giám sát sâu, các gói vượt qua SPI có thể được chuyển sang Hyperscan/Vectorscan để quét L7 Payload.

### **Q10: Luồng giao tiếp giữa `spi_cli` và `spifast` diễn ra như thế nào?**

```
spi_cli (tiến trình riêng)
  └─ connect() đến /tmp/spifast_ctrl.sock
  └─ send() đường dẫn file luật mới: "/path/to/spi_rules.conf"
  └─ recv() phản hồi: "OK: loaded 15 rules from ..."
  └─ close()

spifast (Control Thread)
  └─ accept() ← chờ client
  └─ recv() đọc đường dẫn
  └─ matcher_reload("/path/to/spi_rules.conf")
  └─ send() "OK: loaded N rules from ..."
  └─ close()
```
