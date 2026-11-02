# dpReader 1.2.7-vn

- Trang local thêm tải xuống từng tệp và nút Tải log hệ thống trong tab Thẻ nhớ. Xuất log trên máy đọc trước khi tải bằng điện thoại.
- Thêm công tắc Tự động sao lưu mặc định bật, chỉ chạy trước Sleep và trước cập nhật firmware. Giữ bản sao trước, kiểm tra lỗi ghi và cho phép khôi phục thủ công từ định dạng cũ.
- Logo khởi động mới; đồng bộ logo Red Studio cho Web Flasher và trang local.
- Cải thiện thẻ Đọc tiếp trên Home, nét đậm hoạt động đúng các hướng xoay.

Tự khôi phục sau reboot chưa bật. Chưa đo đỉnh SRAM và chưa nghiệm thu bản này trên thiết bị. Báo cáo lật trang chậm đang chờ log mới; bản này không tuyên bố sửa tốc độ lật trang.

TF: dùng dpReader_metalio47_firmware.bin, giữ sách và thiết lập trên bố cục dpReader tương thích. USB: giải nén gói USB, dùng FLASH_USB.command (macOS) hoặc FLASH_USB.bat (Windows); script kiểm tra máy và chỉ ghi ứng dụng cùng lựa chọn OTA. Không tự động xóa NVS hoặc bộ nhớ sách.

Giữ bản chính thức 1.2.6-vn trong mục phiên bản trước trên Web Flasher.
