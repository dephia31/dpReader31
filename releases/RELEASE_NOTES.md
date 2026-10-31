# dpReader Metalio 4.7 — v1.2.2-vn

- Chọn từ/đoạn bằng nhấn giữ và kéo; lưu highlight, ghi chú và danh sách ghi chú trên thẻ trong `dpReader_note`, giữ dấu khi đọc và đổi cỡ chữ.
- Tra từ điển offline trực tiếp StarDict `.ifo` + `.idx`/`.idx.gz` + `.dict`/`.dict.dz`, hỗ trợ `.syn`; tăng chữ định nghĩa, tối ưu đọc chỉ mục và thêm mục tải folder từ điển ở trang chuyển file local.
- Plugin dpReader cho KOReader trao đổi ghi chú hai chiều theo đoạn trích/chương, có kiểm tra vị trí và xung đột; xem giới hạn trong tài liệu.
- Cải thiện menu ghi chú, viền bảng, chạm ngoài để đóng; Xong tự lưu, Tô sáng tự đóng; bỏ mở mục lục bằng giữ màn hình.
- Thông báo KoSync chữ lớn có viền; thanh kéo nhanh có phản hồi Wi-Fi/Bluetooth. Sửa vùng refresh và tăng chiều cao bảng thống kê, thêm Danh sách ghi chú.
- Chuyển stack các worker SD phù hợp sang PSRAM, giữ task flash/OTA/NVS/DMA/ISR trong SRAM; kiểm thử host và build, chưa đo hiệu năng/nghiệm thu thực tế trên thiết bị.


Cập nhật bằng ứng dụng Bin qua thẻ nhớ/OTA hoặc gói USB đi kèm. Giữ nguyên phân vùng hiện có; không cần xóa dữ liệu.

Từ điển và ghi chú: xem [hướng dẫn](https://github.com/dephia31/dpReader31/blob/main/docs/HIGHLIGHT_DICTIONARY.md). Plugin KOReader ở `integrations/koreader/dpreader-sync.koplugin`. Chọn chữ hiện giới hạn văn bản ngang trong một trang; tra từ vẫn quét tuần tự; trao đổi KOReader cần sách/chương/đoạn trích phù hợp.

Đã build S31 và kiểm thử host. Chưa flash/nghiệm thu bản này trên thiết bị hoặc KOReader thật; không công bố số đo tốc độ/SRAM trên máy thật.
