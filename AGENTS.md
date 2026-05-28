# AGENTS.md - Hệ thống Phân loại Bản tin SPI Hiệu năng cao

## 1. Mục tiêu Dự án (The WHY)
Dự án này xây dựng một hệ thống xử lý và phân tích luồng mạng tốc độ cao (kiến trúc đa luồng, zero-copy) chạy trên Linux. Hệ thống bóc tách các gói tin mạng, quản lý trạng thái kết nối và phát hiện các tên miền/URL nhằm mục đích phân loại lưu lượng dịch vụ.


## 2. Kiến trúc & Thành phần (The WHAT)
Mã nguồn được chia làm 3 module chính theo dạng Pipeline tuần tự. **Trọng tâm tối ưu hóa thuật toán và hiệu năng nằm hoàn toàn ở Module 2 (SPI).**
- **Dispatcher (Project 3):** Nhận gói tin thô bằng DPDK, băm (hash) 5-tuple, điều phối gói tin về các Worker Core. (Chỉ cần code chạy đúng, KHÔNG cần tối ưu thêm).
- **SPI System (Project 2 - TRỌNG TÂM CỐT LÕI):** Quản lý trạng thái kết nối (Stateful tracking). Bóc tách Header L2-L4, theo dõi cờ TCP, lọc bỏ header mạng và chỉ chuyển tiếp Payload ứng dụng sạch lên tầng trên. (Cần tối ưu tối đa tại đây).
- **DPI Scanner (Project 1):** Nhận payload sạch từ tầng SPI, gọi thư viện Hyperscan của Intel để quét biểu thức chính quy (Regex). (Chỉ cần gọi hàm API của Intel, KHÔNG cần tối ưu thêm).

## 3. Công nghệ & Kiểm thử (The HOW)
- **Ngôn ngữ:** Bắt buộc sử dụng **C++17**.
- **Thư viện lõi:** DPDK (bỏ qua nhân Linux - Kernel Bypass) và Hyperscan (quét Regex song song).
- **Môi trường test:** Dự án chạy trên PC/Server Lab Linux, không cần router phần cứng. Sử dụng file `.pcap` thông qua cấu hình DPDK PCAP PMD hoặc tự sinh gói tin bằng Scapy/Tcpreplay để giả lập traffic.
- **Cấu hình máy tính**: ntel Core i7-13700HX (Kiến trúc kết hợp 16 Cores / 24 Threads trong đó có 8 P-core và 8 E-core, xung nhịp lên đến 5.0 GHz).

## 4. Tài liệu Hướng dẫn Chi tiết (Chỉ đọc khi cần thiết)
Không tự ý đoán cơ chế hoạt động. Hãy đọc các file tài liệu chuyên biệt sau đây khi xử lý các phần việc liên quan:
- `docs/rules.md`: Các quy tắc bắt buộc về lập trình hiệu năng cao (Mempool, Ring Buffer, Lockless).
- `docs/network_headers.md`: Cấu trúc byte của packet header và logic xử lý trạng thái TCP.
- `docs/testing_and_benchmarks.md`: Hướng dẫn cách build, nạp file pcap test và đo thông lượng (Throughput Gbps/Mpps).