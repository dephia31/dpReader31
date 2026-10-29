# Chế độ ngủ và hao pin trên Metalio S31

Phạm vi: Metalio E-ink 4 Plus / ESP32-S31, mã nguồn firmware hiện tại. Đây là phân tích đường chạy và phần cứng được bật/tắt trong mã nguồn, chưa phải kết quả đo dòng trên thiết bị. Không dùng số dòng của ESP32-S3/Pico để suy ra dòng toàn máy S31.

| Trạng thái | Hành vi thực tế | Kiểu hao pin dự kiến |
| --- | --- | --- |
| Đang đọc / Home, chưa khóa | CPU và các tài nguyên giao diện vẫn hoạt động; đèn, Wi-Fi, BLE tùy cài đặt. Build hiện tại không bật `CONFIG_PM_ENABLE`. | Vẫn tiêu thụ pin khi màn hình đứng yên. Đèn và phiên mạng làm tăng tiêu thụ; hình không đổi không đồng nghĩa CPU đã ngủ. |
| Khóa / ngủ nhẹ | Tắt đèn, ngừng radio, tạm dừng file/font, ngừng LCD, cho cảm ứng ngủ, hạ hai rail qua IO expander rồi gọi light sleep. Giữ RAM và ngữ cảnh để trở lại trang đang đọc. Có lát timer tối đa 60 giây để kiểm tra USB/đường đánh thức. | Thấp hơn trạng thái hoạt động, nhưng RAM và mạch đánh thức vẫn cần điện; timer/đánh thức giả có thể tạo các đợt tăng dòng. |
| Ngủ nhẹ n giờ rồi tắt nguồn | Chọn 1–24 giờ, mặc định 1 giờ. Đây là lựa chọn mặc định S31. Khóa thường cũng dùng quy trình theo số giờ đã chọn. Hết thời gian: lưu/đồng bộ cần thiết rồi thử tắt nguồn phần cứng. Nếu tắt nguồn chưa thành công, giữ khóa/ngủ nhẹ và thử lại; không tự công bố đã tắt. | n giờ đầu là hao pin ngủ nhẹ; sau khi tắt nguồn thành công là mức chờ thấp hơn. Thời gian giữ RAM tùy số giờ đã chọn. |
| Ngắt nguồn hoàn toàn | Lựa chọn trong menu Tắt nguồn; lưu tiến độ/checkpoint, đồng bộ RTC, đóng I/O, tắt đèn/nguồn màn rồi gọi điều khiển nguồn S31. Bật lại bằng nút nguồn và khởi động lại. | Dự kiến thấp nhất trong các trạng thái nghỉ: mạch nguồn/RTC và tự xả pin vẫn tồn tại, không được coi là 0 mA tuyệt đối. |
| Deep sleep của SoC | Có triển khai `APP_SLEEP_DEEP`: dừng radio, đèn, LCD, cảm ứng/rail, giữ chân rồi gọi `esp_deep_sleep_start()`. Là đường dự phòng/nội bộ, không phải lựa chọn riêng trong giao diện S31 hiện tại. | Thấp hơn ngủ nhẹ về phần SoC; dòng cả máy còn phụ thuộc mạch nguồn/rail và linh kiện còn được cấp điện. Chưa có số đo để so chính xác với tắt nguồn. |
| Khóa nhưng bị hoãn ngủ | USB/VBUS, phiên truyền/cập nhật/đồng bộ, I/O chưa sẵn sàng hoặc chuẩn bị ngủ thất bại có thể ngăn vào trạng thái ngủ thật. Có cơ chế đợi/thử lại; trong lúc đó CPU có thể vẫn chạy dù đã có hình khóa. | Cao hơn ngủ nhẹ thực sự. Cần đo khi đã rút USB; không dùng dòng lúc cắm cáp để kết luận hao pin chờ bằng pin. |

Lưu ý về tên chế độ:

- S31 hiện chỉ có hai lựa chọn ở giao diện: **Ngủ nhẹ n giờ rồi tắt nguồn** và **Ngắt nguồn hoàn toàn**. Chúng điều khiển thao tác Tắt nguồn; khóa thông thường vẫn ngủ nhẹ ngay, rồi tắt sau số giờ đã chọn. Giá trị này lưu NVS; chọn bằng nút −/+ trong Cài đặt → Chế độ tắt nguồn / ngủ. Thời gian tự khóa (1/5/10 phút hoặc tắt) là bộ đếm riêng trước khi bắt đầu ngủ nhẹ.
- Enum `APP_SLEEP_LIGHT/DEEP/OFF` và giá trị sleep cũ vẫn có trong phần cài đặt dùng chung, nhưng giá trị `app_settings_sleep_mode()` không được đường khóa S31 hiện tại sử dụng để chọn chế độ.
- `SOFT_SLEEP` của PMU Pico không phải một chế độ riêng đang được dùng trên S31. Không đồng nhất nhãn “ngủ sâu” còn trong một số log/comment với việc chắc chắn đã gọi deep sleep; đường sau thời gian đã chọn ưu tiên tắt nguồn phần cứng.
- Nếu chuẩn bị ngủ hoặc tắt nguồn thất bại, không thể mặc định máy đã đạt mức tiêu thụ thấp chỉ vì màn hình hiển thị khóa.

Chưa có cơ sở để công bố mA, % pin/ngày hoặc số ngày chờ. Để có số đó cần đo dòng tại pin, rút USB, cùng độ sáng/cấu hình radio, tách bốn mốc: đang đọc, vừa khóa, sau thời gian đã chọn, tắt nguồn. Với dòng trung bình đo được `I` mA và dung lượng sử dụng thực `C` mAh: hao pin lý tưởng mỗi ngày xấp xỉ `24 × I / C × 100%`; đây chỉ là phép quy đổi, không thay thế phép đo.

Mã nguồn đối chiếu: [ngủ/khóa S31](../boards/metalio47/src/main/sleep.c), [điều khiển rail và nguồn](../components/metalio_hw/metalio_hw.c), [điều kiện USB](../components/metalio_hw/metalio_usb.c), [giao diện chọn chế độ](../main/apps/app_device_settings.c), [khóa tự động](../main/app/app_loop.c).
