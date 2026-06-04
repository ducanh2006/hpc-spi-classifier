file .h na ná cái interface trong java
#pragma once bằng ifndefine rồi ta define rồi cuối cùng endefine



1) Cấu hình Hugepages

TLB (Translation Lookaside Buffer) là một bộ nhớ đệm (cache) đặc biệt nằm ngay trong CPU, có nhiệm vụ tăng tốc độ dịch địa chỉ bộ nhớ.TLB là một bộ nhớ cache cực nhanh (nằm trong CPU, gần nhân xử lý) lưu trữ các kết quả dịch địa chỉ gần đây nhất
=> Nghĩa là TLB là bản nhỏ hơn của page table để nó nằm được ngay trong CPU.

Việc cấu hình Hugepages trong GRUB (hugepages=...) thực chất là ra lệnh cho Kernel: "Hãy dành riêng X MB RAM ngay từ đầu và đừng bao giờ dùng nó cho các tác vụ thông thường".
=> Không lo việc cấu hình này ảnh hưởng đến các phần còn lại.

Hệ thống sử dụng Hugepages (2MB) để thay đổi đơn vị ánh xạ bộ nhớ từ 4KB lên 2MB, nhằm tối ưu hóa hiệu năng TLB. Đồng thời, việc cấp phát các vùng nhớ vật lý liên tục (contiguous physical memory) giúp giảm thiểu độ trễ truy cập bộ nhớ nhờ tăng tỷ lệ hit của CPU Cache và hỗ trợ DMA hiệu quả, thay vì chỉ đơn thuần chuyển đổi từ truy cập ngẫu nhiên sang tuần tự.Việc sử dụng Hugepages giúp tối ưu hóa hiệu năng TLB, giảm thiểu latency truy cập bộ nhớ. Tuy nhiên, nó đánh đổi bằng việc tăng internal fragmentation (lãng phí bộ nhớ) và gây khó khăn cho việc cấp phát bộ nhớ liên tục (external fragmentation) khi hệ thống hoạt động lâu dài. Để cấu hình hai tham số Hugepages và contiguous physical memory ta phải can thiệp ngay khi hệ điều hành được boot lên bằng cách sửa các file hệ thống rồi reboot lại.

Cấu hình Hugepages thì nên cấu hình là 4KB*(512^x) là hoàn toàn đúng với kiến trúc phân trang 4 level của x86_64.
=> Vậy ta nên cấu hình: 2MB hoặc 1GB. Ta chọn 2MB (2048 bytes) là đủ rồi. Cấu hình 2048 pages là được tầm (4 GB ram được cấp phát liên tiếp ). 
=> Chính xác là như này: hugepagesz=2M hugepages=2048


2) Hướng đi cho dự án
Khi sử dụng file .pcap, hệ thống chuyển từ chế độ xử lý thời gian thực (real-time) sang chế độ replay dữ liệu. Lúc này, rào cản về băng thông mạng và hiệu năng card mạng được loại bỏ. Tốc độ xử lý tổng thể sẽ phụ thuộc chủ yếu vào năng lực tính toán của CPU (để thực thi logic DPI/Hyperscan) và tốc độ truy xuất bộ nhớ (RAM), do dữ liệu packet thường được nạp vào bộ nhớ trước khi xử lý.

3) Thư viện dpdk

+) Bình thường: Cáp mạng -> Card mạng (NIC) -> Ngắt phần cứng (Interrupt) -> Kernel OS (Linux Network Stack) -> Copy dữ liệu -> Ứng dụng (Socket thường). Quá trình này bị nghẽn ở bước qua OS do phải đổi ngữ cảnh (Context switch) và copy dữ liệu qua lại.
+) Với DPDK: Cáp mạng -> Card mạng (NIC) -> Driver của DPDK (vfio-pci) -> Vùng nhớ Hugepages (RAM liên tiếp) -> Ứng dụng đọc trực tiếp.
+) So sánh triết lý quản lý bộ nhớ (DPDK vs ReactJS):
Thay vì can thiệp trực tiếp vào tài nguyên hệ thống qua công cụ mặc định (như JavaScript tác động vào DOM thật, hay ngôn ngữ C dùng `malloc`/`free` cấp phát bộ nhớ ngẫu nhiên), cả hai thư viện đều tự tạo ra một tầng ảo hóa trung gian độc quyền để quản lý tập trung.
Lập trình viên bắt buộc phải tuân theo các cấu trúc đặc chủng của thư viện (ReactJS dùng Virtual DOM; DPDK dùng `rte_mempool`, `rte_mbuf`) để tối ưu hóa hiệu năng và loại bỏ hoàn toàn các bước xử lý thừa thãi của hệ thống.


4) Đầu vào và ra của chương trình:
+) Đầu vào được giải quyết bằng:
https://wiki.wireshark.org/SampleCaptures?action=AttachFile&do=get&target=
+)

Tóm lại, đầu ra của phần mềm **High‑Performance SPI Message Classification System** (dự án số 2) gồm **hai phần chính**:

+) Metadata gắn vào mỗi packet trích xuất:
👉 Nơi lưu trữ: Lưu trong RAM ( cái vùng mà ta cấp phát Hugepages dành riêng cho chương trình ).
-) Định danh luồng (SPI ID / Flow ID)
-) Kết quả quét Hyperscan (match_id, rule_id, hs_match_id)
-) Dấu thời gian Nhận gói tin (timestamp_rx, rx_tsc, ingress_tsc)
-) Hành động Điều hướng ( Pipeline Action)
-) Định vị Payload L7 (payload_offset & payload_len) 

+) Các chỉ số benchmark:
👉 Nơi lưu trữ: Trong RAM -> In ra Màn hình Console (Terminal) và ghi ra Ổ cứng (File Text/JSON).
-) Throughput:  6/6 AI đồng ý
-) P99 Latency:  6/6 AI đồng ý
-) Drop Rate / Packet Loss : 6/6 AI đồng ý
-) Thread Scalability / Scaling Ratio (Khả năng mở rộng theo luồng): 5/6 AI đồng ý
-) Total Matches / Match Ratio (%): 5/6 AI đồng ý
-) Average Latency: 4/6 AI đồng ý

Note: Các cái trên được cân nhắc, tổng hợp câu trả lời từ các AI gồm gemini, qwen, chatGPT, deepseek, claude, kimi rồi xem cái nào được đồng ý nhiều nhất.
