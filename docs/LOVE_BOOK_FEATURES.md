# Cây tính năng Love Book trên Metalio S31

```text
dpReader Love Book
├── Home · I Love Book
│   ├── Đọc tiếp → sách và vị trí đọc gần nhất
│   ├── Bìa sách, trích đoạn hoặc tên chương
│   └── Vuốt lên hoặc vuốt sang trái (kể cả vuốt chéo) → Mở sách · Truyền file · OPDS · Cài đặt
├── Mở sách
│   ├── Thư mục mặc định Books
│   ├── Duyệt thư mục · lên thư mục cha · tải lại · chuyển trang
│   ├── Mở TXT / EPUB · xem định dạng và dung lượng
│   ├── Xóa → xác nhận tên tệp → tải lại danh sách
│   └── Bật USB → chép sách từ máy tính → dừng USB → tải lại
├── Trình đọc TXT / EPUB
│   ├── Lật trang · chuyển chương · mục lục · nhảy trang
│   ├── Lưu tiến độ · dấu trang · ghi chú · tra từ · thống kê
│   ├── Ảnh EPUB · phóng ảnh · phông chữ nhúng
│   ├── Cài đặt đọc
│   │   ├── Phông, cỡ chữ, lề, giãn cách, căn lề, đọc dọc
│   │   ├── Nền trang · dẫn dòng · hiệu ứng và chu kỳ làm mới
│   │   └── Thanh chân trang
│   │       ├── Hiện chân trang → bật/tắt toàn bộ
│   │       └── Tùy chọn thanh tiến độ, số trang, tên chương, phần trăm
│   └── KoSync → đồng bộ vị trí đọc
├── Kết nối và tệp
│   ├── USB · Wi-Fi · điểm phát
│   ├── OPDS → duyệt kho và tải sách
│   ├── Quản lý tệp qua lối tắt Tệp tin
│   └── Bluetooth → thiết bị lật trang
└── Cài đặt thiết bị
    ├── Phông hệ thống · độ tương phản · làm mới màn hình
    ├── Tên thiết bị
    ├── Đèn nền, nhiệt màu và rung
    ├── Nút cứng · cử chỉ · menu thao tác nhanh
    ├── Màn hình chờ · thẻ đọc sách, hình nền, khung PNG
    ├── Sleep · tự ngủ · đánh thức bằng nút nguồn · tắt nguồn
    └── Sao lưu/khôi phục · cập nhật · chẩn đoán
```

Trong trình đọc, mở **Cài đặt phông chữ → Thanh chân trang → Hiện chân trang**. Khi tắt, nội dung dùng thêm vùng chân trang và trình đọc dàn trang lại theo vị trí đang đọc. Khi bật lại, các tùy chọn thành phần chân trang vẫn được giữ.

**Xóa** trong **Mở sách** áp dụng cho tệp TXT/EPUB. Hộp xác nhận hiển thị tên tệp trước khi xóa. Các thư mục vẫn dùng thao tác duyệt.

Love Book dùng giao diện I Love Book cố định. Các mục kệ sách, màn hình khóa ghép bìa, TTS, Dark mode và xác thực PIN đã được loại khỏi luồng sử dụng.

Love Book bỏ qua mã PIN đã đặt khi chạy Full. Nếu cài lại Full, mã PIN cũ vẫn có thể được yêu cầu.

Home giữ tối đa bốn bìa đã giải mã trong PSRAM (tối đa khoảng 165 KiB ảnh). Khi đổi sách, Home tái dùng bìa còn trong cache và kiểm tra đường dẫn, kích thước, thời gian sửa, tiêu đề và tác giả. Nếu PSRAM trống dưới 2 MiB, lịch sử bìa được thu hồi khi vào Home hoặc mở sách. Sau khi tắt nguồn, thiết bị có thể đọc thumbnail còn hợp lệ trên thẻ.
