# Hệ thống điều phối xe công nghệ

Đồ án môn **Cấu trúc dữ liệu và Giải thuật** (DASA230179) — Khoa Công nghệ Thông tin, Trường Đại học Công nghệ Kỹ thuật TP. Hồ Chí Minh.

Hệ thống điều phối xe công nghệ thu nhỏ viết bằng **C++**, lấy cảm hứng từ các ứng dụng gọi xe như Grab, Be, Xanh SM. Hệ thống giúp chọn tài xế phù hợp cho khách, quản lý chuyến đi, cho phép hoàn tác khi lỡ hủy chuyến và gợi ý địa điểm khi người dùng gõ. Mỗi chức năng được xây dựng trên một cấu trúc dữ liệu của môn học.

## Chức năng

| Chức năng | Mô tả | Cấu trúc dữ liệu |
|-----------|-------|------------------|
| Quản lý tài xế | Lưu và truy xuất thông tin, vị trí, trạng thái, đánh giá của tài xế | Hash Table |
| Điều phối xe | Chọn và xếp hạng tài xế phù hợp nhất quanh vị trí khách | Priority Queue (Max-Heap) |
| Tìm kiếm địa điểm | Gợi ý địa điểm tự động theo ký tự người dùng đang gõ | Prefix Tree (Trie) |
| Quản lý chuyến đi và Undo | Theo dõi trạng thái chuyến và hoàn tác yêu cầu hủy trong 5 giây | Stack |

## Thư viện sử dụng

Dự án viết bằng **C++17**, dùng thư viện chuẩn STL cùng **hai thư viện ngoài** đã kèm sẵn trong thư mục `SourceCode` (không cần cài thêm):

- [**cpp-httplib**](https://github.com/yhirose/cpp-httplib) (`httplib.h`): dựng máy chủ HTTP và REST API.
- [**nlohmann/json**](https://github.com/nlohmann/json) (`json.hpp`): đọc, ghi và xử lý dữ liệu JSON.

Giao diện kiểm thử là trang `index.html` (HTML và JavaScript thuần).

## Cấu trúc thư mục

```
SourceCode/
├── main.cpp                    # Khởi động máy chủ và các API
├── Driver.hpp / .cpp           # Kiểu dữ liệu tài xế
├── HashTable.hpp / .cpp        # Quản lý tài xế
├── operation.hpp / .cpp        # Đánh giá và hiệu suất tài xế
├── queue.hpp / .cpp            # Điều phối xe
├── stackundo.hpp / .cpp        # Chuyến đi và hoàn tác
├── Trie.hpp / .cpp             # Gợi ý địa điểm
├── VietnameseFixed.hpp / .cpp  # Chuẩn hóa tiếng Việt
├── httplib.h, json.hpp         # Thư viện ngoài
├── index.html                  # Giao diện kiểm thử
└── database.json               # Dữ liệu tài xế mẫu
```

## Cách chạy

```bash
cd SourceCode
g++ -std=c++17 -O2 -pthread -o server main.cpp Driver.cpp HashTable.cpp \
    operation.cpp queue.cpp stackundo.cpp Trie.cpp VietnameseFixed.cpp
./server
```

Máy chủ chạy tại `http://localhost:8080`. Mở tệp `index.html` bằng trình duyệt để sử dụng.

> Chương trình được phát triển trên Windows. Khi dùng MinGW, thêm `-lws2_32` vào lệnh biên dịch. Trên Linux hoặc macOS, cần bọc hai dòng `SetConsoleOutputCP` và `SetConsoleCP` trong `main.cpp` bằng `#ifdef _WIN32 ... #endif`.

## Thành viên thực hiện

- Huỳnh Trí Đức
- Đỗ Duy Toàn
- Nguyễn Duy
- Nguyễn Quốc Anh Khoa
- Hồ Bảo Liêm
- Si Tiểu Yến 

Giảng viên hướng dẫn: **Vũ Đình Bảo**
