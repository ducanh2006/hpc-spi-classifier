# **CẨM NANG DPDK**

💡 **Ghi chú quan trọng về từ khóa `static inline`:**
Trong DPDK, 90% các API xử lý gói tin quan trọng (như `rx_burst`, `mtod`, `prefetch`, `timer`...) đều có từ khóa `static inline`. Điều này có nghĩa là DPDK **không tạo ra một lời gọi hàm (function call)** thực sự. Compiler (Trình biên dịch) sẽ "cắt dán" (inline) mã Assembly của DPDK trực tiếp vào vòng lặp `while(1)` của bạn. Việc này loại bỏ hoàn toàn chi phí đẩy thanh ghi vào stack (push/pop registers), giúp vòng lặp đạt tốc độ tối đa. Ngoài ra còn phải có thêm từ khóa `static` giúp tránh xung đột khi link nhiều file lại.

---

## **1. Nhóm `rte_eal_*` (Hạ tầng Hệ thống & Quản lý Luồng)**
**Viết tắt:** **EAL** - **E**nvironment **A**bstraction **L**ayer (Lớp trừu tượng hóa môi trường).

### **1.1. Khởi tạo lõi DPDK (`rte_eal_init`)**

* **Bản chất hoạt động:** EAL quét hệ điều hành Linux để tìm các Hugepages đã được cấp phát, sau đó dùng lệnh `mmap` (memory map) để ánh xạ toàn bộ chúng vào không gian bộ nhớ của tiến trình. DPDK tự quản lý "kho" bộ nhớ vật lý này, loại bỏ hoàn toàn việc gọi `malloc()` (vốn phải nhờ Linux kernel xử lý và cực kỳ chậm).
* **Chữ ký hàm:**
```c
int rte_eal_init(int argc, char **argv);
```
* **Phân tách tham số dòng lệnh (`--`):** Dòng lệnh ứng dụng DPDK gồm hai phần, ngăn cách bởi dấu `--`:
  * **Trước `--` (tham số EAL):** Do `rte_eal_init()` đọc và xử lý. Cấu hình hạ tầng chung — lõi CPU, hugepages, driver PCI/vdev, memory channels... — mà mọi ứng dụng DPDK đều cần. EAL loại bỏ các tham số này khỏi `argv` và trả về số lượng đã consume.
  * **Sau `--` (tham số ứng dụng):** Do code riêng của bạn parse. `rte_eal_init()` **không** đọc phần này; sau khi init xong, trượt `argv`/`argc` qua đúng số tham số EAL đã xử lý rồi tự parse (VD: `-r spi_rules.conf
  * **Ví dụ:** `./myapp -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 --vdev "net_pcap0,..." -- -r spi_rules.conf`
    * Phần `-d ... -l ... -n ... --vdev ...` → EAL.
    * Phần `-r spi_rules.conf` → ứng dụng.
* **Đầu vào:** `argc`, `argv` — Mảng tham số dòng lệnh truyền vào từ `main()`.
  * **Tham số EAL (trước `--`):**
    * `-d "$TMP_DRIVER_DIR"`: Chỉ định thư mục chứa file driver động (`.so`), giúp EAL load các Poll Mode Driver (PMD) như driver mạng ảo (`net_pcap`) hoặc driver card mạng vật lý từ một phân vùng an toàn để tránh lỗi bảo mật.
    * `-l 0-4`: Cấp phát các lõi xử lý (Logical Cores) từ 0 đến 4, trong đó Core 0 tự động làm Master điều phối/nhận gói và các Core 1-4 làm Worker xử lý song song độc lập.
    * `-n 4`: Khai báo số kênh nhớ (Memory Channels) của thanh RAM trên phần cứng, giúp DPDK kích hoạt thuật toán trải đều dữ liệu để tối ưu hóa băng thông khi nhiều core cùng truy cập Hugepages.
    * `--vdev "net_pcap0,rx_pcap=$pcap,infinite_rx=1"`: Khởi tạo thiết bị mạng ảo tên `net_pcap0` để đọc trực tiếp gói tin từ file PCAP nạp vào mbuf, đi kèm chế độ `infinite_rx=1` để tự động lặp lại file vô hạn giúp giả lập luồng traffic liên tục.
  * **Tham số ứng dụng (sau `--`, do bạn tự parse):**
    * `-r "$PROJECT_ROOT/spi_rules.conf"`: Chỉ định đường dẫn tới file cấu hình chứa tập luật phân loại (Shallow Packet Inspection), được hàm Rule Parser đọc và nạp vào mảng cấu trúc trước khi bắt đầu bắt gói.
* **Đầu ra:** `int` — Số lượng tham số EAL đã parse thành công. Nếu âm (VD: `-EINVAL`), khởi tạo thất bại.
* **Cách dùng:** Gọi đầu tiên trong hàm `main()`. Sau đó, trượt con trỏ `argv` đi một đoạn bằng chính giá trị trả về để tiếp tục parse các tham số riêng của ứng dụng (như `-r spi_rules.conf`).

### **1.2. Khởi chạy luồng Worker (`rte_eal_remote_launch`)**

* **Bản chất hoạt động:** DPDK không dùng `pthread_create` thông thường vì nó muốn "gắn chặt" (pin) một luồng vào một CPU core vật lý duy nhất để tránh hệ điều hành ngắt (context switch). Hàm này được Master Core gọi để ra lệnh cho một Core khác (Worker) bắt đầu chạy một hàm cụ thể.
* **Chữ ký hàm:**
```c
int rte_eal_remote_launch(int (*f)(void *), void *arg, unsigned worker_id);
```
* **Đầu vào:**
  * `f`: Con trỏ hàm chứa vòng lặp xử lý của Worker (VD: `worker_loop`). Hàm này phải có kiểu trả về `int` và nhận một tham số `void *`.
  * `arg`: Con trỏ chứa tham số bạn muốn truyền cho Worker (thường là một `struct` chứa ID của Worker hoặc con trỏ `rte_ring` tương ứng).
  * `worker_id`: ID của Logical Core mà bạn muốn nhắm tới (VD: lcore 1, 2, 3, 4).
* **Đầu ra:** `0` nếu khởi chạy thành công; `-EBUSY` nếu lcore đó đang chạy một nhiệm vụ khác và không rảnh.
* **Ví dụ sử dụng:**
```c
// Master core ra lệnh cho Worker Core số 1 chạy hàm worker_main
rte_eal_remote_launch(worker_main, (void *)worker_config, 1);
```

---

## **2. Nhóm `rte_pktmbuf_*` (Quản lý Bộ đệm & Gói tin)**
**Viết tắt:** **pktmbuf** - **P**ac**k**e**t** **M**essage **Buf**fer (Vùng đệm tin nhắn chuyên dụng để chứa gói tin mạng).

### **2.1. Cấu trúc dữ liệu cốt lõi (Core Data Structures)**

Để hiểu rõ cách DPDK quản lý gói tin, cần nắm vững hai cấu trúc dữ liệu quan trọng nhất: **`rte_mbuf`** (đại diện cho một gói tin) và **`rte_mempool`** (kho chứa các gói tin).

#### **2.1.1. Struct `rte_mbuf` (Cấu trúc gói tin)**
Đây là cấu trúc trung tâm của DPDK, đại diện cho một gói tin (packet) hoặc một phân đoạn (segment) của gói tin. Nó chứa metadata mô tả gói tin và trỏ đến vùng dữ liệu thực tế.

```c
struct rte_mbuf {
    /* 1. VÙNG DỮ LIỆU (DATA REGION) */
    void *buf_addr;                      // Con trỏ trỏ đến vùng đệm chứa dữ liệu gói tin thực tế (payload + headers).
                                         // Đây là nơi bắt đầu của bộ nhớ vật lý chứa gói tin.
    uint16_t buf_len;                    // Tổng kích thước của vùng đệm (buf_addr).
                                         // Thường là 2176 bytes (data_room_size) khi khởi tạo pool.

    /* 2. METADATA TRẠNG THÁI GÓI TIN (PACKET METADATA) */
    uint16_t data_off;                   // Độ dời (offset) từ buf_addr đến vị trí bắt đầu của dữ liệu gói tin.
                                         // Thường là 128 bytes (RTE_PKTMBUF_HEADROOM) để chèn thêm header.
    uint16_t data_len;                   // Độ dài dữ liệu thực tế chứa trong segment này.
    uint32_t pkt_len;                    // Tổng độ dài của toàn bộ gói tin (nếu gói tin bị chia thành nhiều segment).
    
    uint16_t port;                       // ID của cổng (port) mà gói tin được nhận vào hoặc sẽ được gửi đi.
    uint64_t ol_flags;                   // Offload flags: Các cờ báo hiệu tính năng phần cứng hỗ trợ 
                                         // (VD: RTE_MBUF_F_RX_IPV4, RTE_MBUF_F_TX_IP_CKSUM...).

    /* 3. QUẢN LÝ VÒNG ĐỜI (LIFECYCLE MANAGEMENT) */
    struct rte_mempool *pool;            // Con trỏ trỏ ngược về Mempool đã cấp phát ra mbuf này.
                                         // Rất quan trọng để hàm rte_pktmbuf_free() biết đường trả mbuf về đúng pool.
    uint16_t refcnt;                     // Reference count: Số lượng tham chiếu đến mbuf này.
                                         // Khi refcnt = 0, mbuf được coi là rảnh (free) và có thể tái sử dụng.
    
    /* 4. PHÂN ĐOẠN (SEGMENTATION - CHO JUMBO FRAMES) */
    struct rte_mbuf *next;               // Con trỏ trỏ đến segment tiếp theo. 
                                         // Nếu gói tin lớn hơn buf_len, nó sẽ được chia thành chuỗi (chain) các mbuf.
                                         // Nếu là segment cuối, next = NULL.
};
```

#### **2.1.2. Struct `rte_mempool` (Kho chứa gói tin)**
Đây là cấu trúc quản lý một tập hợp lớn các `rte_mbuf` được cấp phát sẵn từ Hugepages. Nó được thiết kế để tối ưu hóa cho việc cấp phát và giải phóng `mbuf` cực nhanh trên môi trường đa nhân.

```c
struct rte_mempool {
    /* 1. ĐỊNH DANH & QUẢN LÝ HỆ THỐNG */
    char name[RTE_MEMPOOL_NAMESIZE];       // Tên định danh của pool (VD: "MBUF_POOL")
    unsigned int size;                     // Tổng số lượng mbuf được cấp phát ban đầu trong pool (VD: 8192)
    int socket_id;                         // ID của NUMA Node (Socket CPU vật lý) quản lý vùng bộ nhớ Hugepage này

    /* 2. CẤU TRÚC KÍCH THƯỚC PHẦN TỬ (ELEMENT SIZING) */
    unsigned int elt_size;                 // Kích thước của 1 phần tử (Gồm: mbuf metadata + 2176 bytes data room)
    unsigned int header_size;              // Kích thước phần padding/header bảo vệ phía trước mỗi phần tử
    unsigned int trailer_size;             // Kích thước phần padding/trailer bảo vệ phía sau mỗi phần tử

    /* 3. CƠ CHẾ TỐI ƯU HÓA HIỆU NĂNG LÕI (PER-CORE PERFORMANCE) */
    unsigned int cache_size;               // Số lượng mbuf tối đa được phép "ôm" sẵn về kho riêng của mỗi CPU core
    struct rte_mempool_cache *local_cache; // Con trỏ trỏ tới mảng bộ nhớ đệm nội bộ (per-core cache) của từng lcore

    /* 4. BACKEND ĐIỀU PHỐI LOCKLESS */
    struct rte_ring *ring;                 // Vòng tròn hàng đợi lockless ngầm định dùng để chứa các mbuf đang ở trạng thái rảnh (free)
    void *pool_data;                       // Con trỏ chứa dữ liệu mở rộng tùy thuộc vào Driver điều khiển Mempool (Mempool Ops Backend)
};
```


### **2.2. Tạo kho chứa gói tin (`rte_pktmbuf_pool_create`)**

* **Bản chất:** Hàm này "rút" một phần bộ nhớ từ kho Hugepages để cắt thành các khối mbuf bằng nhau. DPDK sử dụng cơ chế **Per-core cache**: Lấy sẵn một số lượng mbuf "chia trước" cho từng CPU core. Nhờ vậy, khi các core cần mbuf, chúng lấy từ cache riêng mà không phải tranh chấp (lock) với global pool.
* **Chữ ký hàm:**
```c
struct rte_mempool * rte_pktmbuf_pool_create(
    const char *name, 
    unsigned n, 
    unsigned cache_size, 
    uint16_t priv_size, 
    uint16_t data_room_size, 
    int socket_id
);
```
* **Đầu vào:**
  * `name`: Tên mảng (VD: "MBUF_POOL").
  * `n`: Tổng số mbuf (VD: 8192).
  * `cache_size`: Kích thước cache nội bộ cho từng core (VD: 250).
  * `priv_size`: Kích thước metadata ẩn (thường để 0).
  * `data_room_size`: Dung lượng chứa payload mạng (VD: 2176 bytes - đủ chứa gói tin MTU chuẩn 1518 bytes + headroom 128 bytes + không gian dự phòng cho Jumbo Frames hoặc căn chỉnh alignment).
  * `socket_id`: Vị trí NUMA node (VD: `rte_socket_id()`).
* **Đầu ra:** Con trỏ `struct rte_mempool *` trỏ đến vùng nhớ Mempool, hoặc `NULL` nếu lỗi.

### **2.3. Cấp phát Mbuf từ Pool (`rte_pktmbuf_alloc`)**

* **Bản chất:** Hàm này lấy một `mbuf` ở trạng thái rảnh (free) từ Mempool để chuẩn bị chứa dữ liệu gói tin mới. Cơ chế ưu tiên lấy từ **Per-core cache** trước (nhanh, không lock), nếu cache trống mới "múc" thêm từ vòng tròn chung (`ring`).
* **Hành động ngầm định:** Tự động reset các trường metadata về giá trị mặc định an toàn: `data_off` = 128 (headroom), `pkt_len` = 0, `refcnt` = 1, xóa sạch các cờ offload cũ.
* **Chữ ký hàm:**
```c
static inline struct rte_mbuf *rte_pktmbuf_alloc(struct rte_mempool *mp);
```
* **Đầu vào:** Con trỏ `struct rte_mempool *` (pool đã tạo ở bước 2.2).
* **Đầu ra:** 
  * Con trỏ `struct rte_mbuf *` sẵn sàng sử dụng.
  * `NULL`: Nếu pool đã cạn kiệt (cần xử lý tránh crash/drop gói tin).
* **Lưu ý:** Trong luồng RX từ NIC, DPDK tự động alloc mbuf. Hàm này thường dùng khi cần tạo gói tin mới để gửi đi (TX) hoặc clone gói tin cho xử lý song song.

### **2.4. Ép kiểu và Bóc tách Header (`rte_pktmbuf_mtod`)**

**Viết tắt:** **M**buf **TO** **D**ata (Từ Mbuf trỏ tới Dữ liệu thực).

* **Bản chất:** Cấu trúc `mbuf` chỉ là metadata. Macro này dùng cơ chế **Zero-copy**, không tốn chi phí copy dữ liệu. Nó thực hiện phép cộng con trỏ cơ bản: `Địa chỉ gói tin = Địa chỉ bộ đệm + Độ dời (offset)`.
* **Chữ ký (Macro):**
```c
#define rte_pktmbuf_mtod(m, t) ((t)((uintptr_t)(m)->buf_addr + (m)->data_off))
```
* **Đầu vào:** `m` (Con trỏ mbuf), `t` (Kiểu dữ liệu đích muốn ép thành, VD: `struct rte_ether_hdr *`).
* **Đầu ra:** Con trỏ kiểu `t` trỏ thẳng vào vùng header chứa các bit của gói tin mạng.


### **2.5. Tránh rò rỉ bộ nhớ - Giải phóng Mbuf (`rte_pktmbuf_free`)**

* **Bản chất:** DPDK không có Garbage Collector. Phải tự dọn dẹp, nếu không Mempool sẽ cạn chỉ sau vài giây ở tốc độ 1Gbps. Hàm này sẽ trả vùng đệm về lại **per-core cache**, nếu cache đầy mới đẩy về global pool.
* **Chữ ký hàm:**
```c
static inline void rte_pktmbuf_free(struct rte_mbuf *m);
```
* **Đầu vào:** Con trỏ mbuf đã xử lý xong.
* **Đầu ra:** Bắt buộc Worker phải gọi sau khi đã match Rule/Action xong.

---

## **3. Nhóm `rte_ring_*` (Hàng đợi Siêu tốc & IPC)**

**Bản chất & Thuật toán:** `rte_ring` là cấu trúc nền tảng giúp IPC (giao tiếp giữa các core) mà không cần Lock. Mỗi worker-core có thể có một `rte_ring` của riêng mình hoặc là tất cả dùng chung một `rte_ring`.

* **Đánh đổi (Trade-off):** Loại bỏ hoàn toàn Mutex/Spinlock, chấp nhận **busy-waiting** (CPU luôn chạy 100% để check hàng đợi). Đổi lại, độ trễ và băng thông đạt mức lý tưởng.
* **Thuật toán 4 con trỏ (Maged M. Michael & Michael L. Scott - 1996):**
  * Gồm 4 con trỏ, tất cả đều được ban đầu khởi tạo = 0:
    * **`prod_head` (Producer Head):** Vị trí ranh giới xuất phát mà Master Core **định sẽ ghi** loạt mbuf tiếp theo vào ring. Khi Master muốn enqueue $n$ gói tin, nó sẽ kiểm tra xem còn đủ slot trống không, nếu đủ, nó sẽ tạm thời "đặt gạch" bằng cách cộng dịch `prod_head` lên $n$ đơn vị ngay lập tức để giữ chỗ (reserve), ngăn các luồng khác ghi đè vào.
    * **`prod_tail` (Producer Tail):** Vị trí đánh dấu dữ liệu hợp lệ mà Master Core **đã hoàn thành** việc sao chép (copy) các con trỏ mbuf vào các slot thành công. Chỉ khi Master thực sự ghi xong bộ byte dữ liệu, `prod_tail` mới được cập nhật tiến lên bằng với `prod_head`. Lúc này, Worker mới được phép nhìn thấy và bắt đầu đọc các gói tin này (commit).
    * **`cons_head` (Consumer Head):** Vị trí xuất phát mà Worker Core **định sẽ đọc** loạt mbuf tiếp theo ra khỏi ring. Khi Worker muốn xử lý một lô (burst) $n$ gói tin, nó sẽ dịch chuyển `cons_head` lên trước $n$ đơn vị để tranh suất lấy mbuf về kho riêng, cô lập các slot này để không luồng nào khác động vào.
    * **`cons_tail` (Consumer Tail):** Vị trí xác nhận Worker Core **đã hoàn thành** việc lấy dữ liệu ra và xử lý xong xuôi, đồng thời chính thức giải phóng (free) hoàn toàn các slot trống đó để quay vòng cho Master ghi tiếp. `cons_tail` chỉ dịch chuyển đuổi kịp `cons_head` sau khi quá trình đọc bộ mbuf thành công mỹ mãn.
  * **Ý nghĩa 4 con trỏ:**
    * Tách biệt thao tác của Producer và Consumer, giảm tối đa xung đột truy cập đồng thời lên vùng nhớ dùng chung.
    * Cho phép Producer và Consumer có thể hoạt động song song, tăng throughput.
  * **Tại sao phải dùng 4 con trỏ?**
    * Thuật toán 4 con trỏ sinh ra để giải quyết bài toán **Multi-Producer / Multi-Consumer (Nhiều người ghi / Nhiều người đọc)** cùng lúc mà không cần dùng đến Lock nặng nề.
    * **Lưu ý thêm:** Trong DPDK, ngay cả khi bạn cấu hình Ring ở chế độ **SP/SC** (Single-Producer / Single-Consumer), cấu trúc `rte_ring` vẫn giữ nguyên 4 con trỏ này để thống nhất API. Tuy nhiên, lúc này DPDK sẽ **bỏ qua** các lệnh đồng bộ phức tạp (CAS) và chỉ dùng Memory Barrier, giúp tốc độ đạt mức tối đa tuyệt đối.
  * **Ưu điểm:**
    * Tối ưu tốc độ nhờ loại bỏ tối đa lock, rất phù hợp cho môi trường multi-core.
    * Thuật toán non-blocking (không chặn), giảm thiểu độ trễ khi truyền dữ liệu giữa các core.
    * Được đánh giá là đơn giản, dễ implement, đã kiểm chứng thực tế qua nghiên cứu của Michael & Scott (1996).
  * **Nhược điểm:**
    * Busy-waiting: Nếu Producer hoặc Consumer quá chênh lệch tốc độ, 1 bên có thể phải liên tục kiểm tra trạng thái queue gây tốn CPU.
    * Quy mô lớn hoặc nhiều Producer/Consumer có thể yêu cầu thêm đồng bộ (hoặc lựa chọn biến thể khác).
  * **Tài liệu tham khảo:** Maged M. Michael & Michael L. Scott, "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms", 1996.
* **Bitwise vs Modulo:** Kích thước Ring **bắt buộc** là lũy thừa của 2 (VD: 1024, 2048). Để quay vòng index, DPDK dùng phép toán Bitwise AND (`&`) với `mask = size - 1` (chỉ tốn 1 cycle), thay vì dùng phép chia lấy dư (`%`) cực kỳ tốn kém.

### **3.1. Tạo Ring (`rte_ring_create`)**

* **Cách dùng tối ưu:** Mỗi Worker nên có một Ring riêng biệt (Single-Producer / Single-Consumer). Khi đó, DPDK **không cần dùng đến lệnh CAS (Compare-And-Swap)**, tối đa hóa hiệu năng.
* **Cấu trúc `rte_ring`:**
```c
struct rte_ring {
    char name[RTE_RING_NAMESIZE];      // Tên của ring (phục vụ debug/quản lý)
    unsigned size;                     // Tổng số slot trong ring (phải là lũy thừa của 2)
    unsigned mask;                     // Giá trị mặt nạ (mask = size - 1) để quay vòng index rất nhanh
    volatile unsigned prod_head;       // Producer Head: vị trí producer sẽ bắt đầu ghi (dùng để reserve slot)
    volatile unsigned prod_tail;       // Producer Tail: vị trí producer đã ghi xong dữ liệu (commit dữ liệu viết)
    volatile unsigned cons_head;       // Consumer Head: vị trí consumer sẽ chuẩn bị đọc (reserve quyền đọc)
    volatile unsigned cons_tail;       // Consumer Tail: vị trí consumer đã đọc xong hoàn toàn (giải phóng slot)
    void *ring[];                      // Vùng nhớ thực sự lưu các object (VD: con trỏ mbuf)
};
```
* **Đầu vào:**
  * `count`: Số slot (lũy thừa 2).
  * `flags`: Cờ định dạng, nên dùng `RING_F_SP_ENQ | RING_F_SC_DEQ` để bật chế độ SP/SC. `RING_F_SP_ENQ` nghĩa là chỉ có một producer (Single-Producer) thực hiện enqueue vào ring, còn `RING_F_SC_DEQ` là chỉ có một consumer (Single-Consumer) thực hiện dequeue từ ring. Kết hợp hai cờ này giúp tối ưu hiệu năng vì DPDK có thể bỏ qua các thao tác đồng bộ phức tạp.
  * *(Lưu ý: Từ DPDK 20.11+, các cờ này đã được thay thế bằng cơ chế Sync Mode, ví dụ truyền `RTE_RING_SYNC_ST` vào hàm `rte_ring_create`. Tuy nhiên, cách dùng cờ cũ vẫn được hỗ trợ ở các bản DPDK phổ biến trước đó).*
* **Đầu ra:** Con trỏ `struct rte_ring *` hoặc `NULL`.
* **Lưu ý về từ khóa `volatile`:**
> Từ khóa `volatile` trong định nghĩa các biến như `prod_head`, `prod_tail`, `cons_head`, `cons_tail` cho trình biên dịch biết rằng giá trị của biến này có thể bị thay đổi bất cứ lúc nào bởi một luồng khác, không chỉ trong phạm vi hàm hiện tại. Vì vậy, trình biên dịch sẽ luôn đọc giá trị trực tiếp từ bộ nhớ chứ không tối ưu hóa truy xuất giá trị cho các biến này vào thanh ghi tạm/thay đổi trình tự truy cập. Điều này đặc biệt quan trọng đối với các biến dùng chung phục vụ giao tiếp đa luồng/multi-core như trong cấu trúc `rte_ring`, giúp đảm bảo việc cập nhật và kiểm tra trạng thái queue luôn nhất quán giữa các core.

### **3.2. Đẩy từng dữ liệu một vào Ring (`rte_ring_enqueue`)**

* **Chữ ký hàm:**
```c
int rte_ring_enqueue(struct rte_ring *r, void *obj);
```
* **Đầu vào:** 
  * `r`: con trỏ tới ring đích
  * `obj`: con trỏ mbuf cần đẩy vào
* **Đầu ra:** 
  * `0` nếu đẩy thành công,
  * `-ENOBUFS` nếu Ring đã đầy.
* **Ví dụ sử dụng:**
```c
// Khởi tạo ring và pkt thật sự (ví dụ minh họa)
struct rte_ring *ring = rte_ring_create("example_ring", 1024, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
if (rte_ring_enqueue(ring, pkt) < 0) {
    // Xử lý khi Ring đầy, có thể free(pkt) hoặc drop/tính toán lại
} else {
    // Đã đẩy thành công
}
```

### **3.3. Đẩy một loạt dữ liệu (burst) vào Ring (`rte_ring_enqueue_burst`)**

* **Chữ ký hàm:**
```c
unsigned int rte_ring_enqueue_burst(struct rte_ring *r, void * const *obj_table, unsigned int n, unsigned int *free_space);
```
* **Đầu vào:** 
  * `r`: ring đích
  * `obj_table`: mảng con trỏ mbuf
  * `n`: số lượng mbuf muốn đẩy
  * `free_space` (có thể NULL): trả về số slot còn trống sau khi đẩy
* **Đầu ra:** Trả về số mbuf **thực tế** đã được ghi vào ring. Nếu ít hơn `n` tức là Ring đã đầy một phần, bạn phải xử lý phần còn lại.
* **Ví dụ sử dụng:**
```c
// Ví dụ: Đẩy burst mbuf vào ring; giải phóng phần chưa đẩy được nếu ring đầy
struct rte_ring *ring = /* giả lập khởi tạo ring */;
struct rte_mbuf *pkts[BURST_SIZE];
// ... khởi tạo/chuẩn bị đầy đủ pkts[] các mbuf ...
unsigned pushed = rte_ring_enqueue_burst(ring, (void * const *)pkts, BURST_SIZE, NULL);
if (pushed < BURST_SIZE) {
    // Ring đầy một phần, giải phóng những mbuf chưa được thêm vào ring
    for (unsigned i = pushed; i < BURST_SIZE; ++i) {
        rte_pktmbuf_free(pkts[i]);
    }
}
// pushed là số lượng mbuf đã thực sự được ghi vào ring
```

### **3.4. Rút từng mbuf một ra khỏi Ring (`rte_ring_dequeue`)**

* **Chữ ký hàm:**
```c
int rte_ring_dequeue(struct rte_ring *r, void **obj_p);
```
* **Đầu vào:** 
  * `r`: ring nguồn
  * `obj_p`: địa chỉ để lưu con trỏ mbuf rút được
* **Đầu ra:** 
  * `0` nếu rút thành công,
  * `-ENOENT` nếu ring trống.
* **Ví dụ sử dụng:**
```c
// Ví dụ: Rút một mbuf từ ring ra
struct rte_ring *ring = /* giả lập khởi tạo ring */;
struct rte_mbuf *pkt;
if (rte_ring_dequeue(ring, (void **)&pkt) == 0) {
    // Đã lấy thành công một gói tin từ ring
} else {
    // Ring đang trống, không có dữ liệu để rút
}
```

### **3.5. Rút một loạt dữ liệu (burst) ra khỏi Ring (`rte_ring_dequeue_burst`)**

* **Chữ ký hàm:**
```c
unsigned int rte_ring_dequeue_burst(struct rte_ring *r, void **obj_table, unsigned int n, unsigned int *available);
```
* **Đầu vào:** 
  * `r`: ring nguồn
  * `obj_table`: mảng con trỏ để nhận mbuf sau khi rút
  * `n`: số lượng mbuf muốn rút
  * `available` (có thể NULL): trả về số lượng còn lại trong ring sau khi rút
* **Đầu ra:** Trả về số mbuf **thực tế** lấy ra được. Nếu bằng 0 thì ring chưa có dữ liệu.
* **Ví dụ sử dụng:**
```c
// Ví dụ: Rút burst mbuf ra khỏi ring và xử lý/giải phóng chúng
struct rte_ring *ring = /* giả lập khởi tạo ring */;
struct rte_mbuf *pkts[BURST_SIZE];
unsigned pulled = rte_ring_dequeue_burst(ring, (void **)pkts, BURST_SIZE, NULL);
if (pulled > 0) {
    for (unsigned i = 0; i < pulled; ++i) {
        // Xử lý từng mbuf lấy ra được
        // ...
        rte_pktmbuf_free(pkts[i]);
    }
}
// pulled là số lượng mbuf thực sự lấy ra được khỏi ring
```

---

## **4. Nhóm `rte_eth_*` (Giao tiếp Mạng Ethernet)**
**Viết tắt:** **eth** - **Eth**ernet (Chuẩn công nghệ mạng cục bộ hữu tuyến phổ biến nhất hiện nay, dùng để định danh các hàm quản lý cấu hình và thu phát gói tin của card mạng).

### **4.1. Nhận gói tin từ Card mạng (`rte_eth_rx_burst`)**

**Viết tắt:** **RX** - **R**eceive, **Burst** - Xử lý theo lô.

* **Bản chất:** DPDK không nhận từng gói một. Lấy theo lô (burst) giúp tối ưu hóa băng thông bộ nhớ và giảm overhead hàm.
* **Chữ ký hàm:**
```c
static inline uint16_t rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);
```
* **Đầu vào:**
  * `port_id`: ID của card mạng (thường là 0 với vdev đơn).
  * `queue_id`: Hàng đợi RX (thường là 0).
  * `rx_pkts`: Mảng con trỏ mbuf đã khởi tạo để DPDK nhét gói tin vào.
  * `nb_pkts`: Số lượng tối đa muốn nhận (VD: 32).
* **Đầu ra:** Số lượng gói tin **thực tế** nhận được.

### **4.2. Phát gói tin ra Card mạng (`rte_eth_tx_burst`)**

**Viết tắt:** **TX** - **T**ransmit (Phát / Gửi dữ liệu), **Burst** - Xử lý theo lô.

* **Bản chất:** Chuyển quyền sở hữu một loạt con trỏ mbuf từ phần mềm xuống hàng đợi phần cứng (hoặc driver pcap) để truyền ra ngoài lộ tuyến, cũng chạy theo cơ chế gom lô để tối đa hóa hiệu năng.
* **Chữ ký hàm:**

```c
static inline uint16_t rte_eth_tx_burst(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
```

* **Đầu vào:**
    * `port_id`: ID của card mạng muốn phát dữ liệu đi (thường là 0 với vdev đơn).
    * `queue_id`: ID của hàng đợi TX (thường là 0).
    * `tx_pkts`: Mảng chứa các con trỏ mbuf đã được xử lý xong và đã sẵn sàng để gửi đi.
    * `nb_pkts`: Số lượng gói tin tối đa muốn phát đi trong lô này (thường set = 32 hoặc 64).
* **Đầu ra:** `uint16_t` — Số lượng gói tin **thực tế** đã được xếp hàng gửi đi thành công.

---

## **5. Nhóm API Thời gian (Time & Timer Hints)**

**Bản chất:** Đo lường băng thông (Mbps) và lưu lượng (pps) cực kỳ chính xác dựa trên thanh ghi chu kỳ đồng hồ của chính phần cứng (Time Stamp Counter - TSC).

**Lưu ý phân biệt:** Các hàm `rte_get_timer_*` thực chất thuộc về **EAL Time API** (nằm trong `rte_time.h`), dùng để đo lường thời gian (benchmark). Trong khi đó, thư viện `rte_timer` (nằm trong `rte_timer.h`) lại dùng để **đặt lịch chạy callback định kỳ** (giống như `cron`, ví dụ: cứ 5 giây thì gọi hàm A 1 lần).

### **5.1. Lấy thông số chu kỳ CPU**

* **Chữ ký hàm:**
```c
static inline uint64_t rte_get_timer_hz(void);
static inline uint64_t rte_get_timer_cycles(void);
```
* **Đầu ra:** 
  * `rte_get_timer_hz()`: Trả về tần số xung nhịp CPU (VD: 2.500.000.000 Hz).
  * `rte_get_timer_cycles()`: Trả về số cycle hiện tại tính từ lúc khởi động.
* **Cách dùng thực tế:**
Lấy `current_cycles - start_cycles`. Nếu lớn hơn hoặc bằng `rte_get_timer_hz()`, tức là đúng 1 giây đã trôi qua. Lúc này xuất thống kê ra màn hình và reset `start_cycles`.

---

## **6. Nhóm Tối ưu hóa Cấp độ Vi kiến trúc (Compiler & CPU Hints)**

### **6.1. Gợi ý nạp trước Cache (`rte_prefetch0`, `rte_prefetch1`, `rte_prefetch2`)**

* **Bản chất:** Gợi ý / Nhắc khéo (Hint) cho CPU: *"Tôi sắp dùng vùng dữ liệu ở địa chỉ này, nếu được hãy tranh thủ nạp nó từ RAM vào bộ nhớ đệm Cache"*. Vì chỉ là gợi ý, CPU có quyền bỏ qua nếu đang bận xử lý tác vụ quan trọng hơn. Nếu gọi trước khi dùng khoảng 2-4 gói tin, tỉ lệ trúng cache cực cao, giúp CPU không bị "stall" (đứng hình) chờ RAM. Tốc độ thực thi bản thân các lệnh này rất rẻ (chỉ 1-2 chu kỳ CPU) và không bao giờ gây lỗi crash/segfault nếu địa chỉ truyền vào bị sai hoặc không hợp lệ.
* **Chữ ký hàm:**

```c
static inline void rte_prefetch0(const volatile void *p); // Gợi ý nạp dữ liệu vào L1 Cache (Nhanh nhất, gần CPU nhất)
static inline void rte_prefetch1(const volatile void *p); // Gợi ý nạp dữ liệu vào L2 Cache
static inline void rte_prefetch2(const volatile void *p); // Gợi ý nạp dữ liệu vào L3 Cache (Chậm nhất trong 3 cấp, xa CPU nhất)

```

* **Đầu vào:** `p` — Con trỏ địa chỉ vùng bộ nhớ muốn nạp trước (thường là địa chỉ vùng payload gói tin lấy từ macro `rte_pktmbuf_mtod(mbuf, void*)`).
* **Đầu ra:** `void` (Không trả về giá trị, không báo trạng thái thành công hay thất bại).


### **6.2. Điều hướng rẽ nhánh (`likely` và `unlikely`)**

* **Bản chất:** Đây là các Macro giao tiếp với Trình biên dịch (GCC/Clang), không phải CPU.
  * `likely`: Báo cho GCC điều kiện thường xuyên đúng, GCC dịch mã sao cho block `if` nằm sát lệnh check để CPU đi thẳng (fall-through).
  * `unlikely`: Báo block `if` rất hiếm xảy ra (VD: lỗi null, lỗi format), GCC đẩy mã ra xa để tránh làm rác Instruction Cache.
* **Chữ ký (Macro):**
```c
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
```
* **Đầu vào:** `x` - Biểu thức logic (VD: `ip_hdr != NULL`).