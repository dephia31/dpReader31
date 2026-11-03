# dpReader 1.2.9-vn

- Giữ Wi-Fi trong suốt phiên duyệt OPDS; sửa mở danh mục con và tiếp tục duyệt sau khi tải sách.
- Tự kết nối lại điều khiển BLE đã lưu sau khi trang đọc sẵn sàng; giảm tần suất thử lại khi mất kết nối.
- Xác nhận dừng Wi-Fi/Bluetooth khi khóa hoặc Sleep; Wi-Fi chỉ bật lại khi người dùng hoặc tác vụ cần mạng yêu cầu. Bảo vệ kiểm tra cập nhật, OTA và truyền tệp khỏi tự ngắt mạng.
- Đồng bộ giờ tự động mỗi ngày khi Wi-Fi đã kết nối, bỏ lượt khi offline hoặc lỗi và không tự mở Wi-Fi.
- Thêm lịch bật/tắt và độ sáng riêng cho đèn trắng, đèn ấm; Sleep vẫn tắt cả hai đèn.
- Vuốt xuống trong nội dung sách để mở bảng cài đặt nhanh.
- Giảm kiểm tra nền khi đọc, ưu tiên cache và thao tác lật trang; cải thiện tiếp nhận nút nguồn sau Sleep và bảo vệ lịch đèn khi lỗi đọc thiết lập.
- Gói USB macOS tự đóng cửa sổ Terminal riêng sau khi cài thành công; giữ thông báo khi có lỗi.

TF/OTA dùng `dpReader_metalio47_firmware.bin`. Gói USB có `FLASH_USB.command` cho macOS và `FLASH_USB.bat` cho Windows; kiểm tra đúng máy và giữ sách/thiết lập trên bố cục tương thích. Giữ bản chính thức 1.2.8-vn tại mục phiên bản trước.
