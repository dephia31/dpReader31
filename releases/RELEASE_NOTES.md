## Tiếng Việt

Áp dụng cho dpReader Full và Love Book trên Metalio E-Ink 4 Plus 4,7 inch.

- Vuốt cạnh trái trong trình đọc để chỉnh đèn theo 10 nấc, cộng mức 0%. Hộp **Độ sáng** hiển thị phần trăm, tự tắt sau một giây kể từ lúc ngừng vuốt và có thể tắt trong **Cài đặt đọc sách**.
- Đổi nhãn thành **Xoay ngang màn hình**. Chuyển chu kỳ làm mới toàn trang vào **Hiệu ứng lật trang**.
- Khi nhận lệnh khóa, máy tắt đèn nền và giữ trang hiện tại cho đến khi ảnh khóa sẵn sàng. Với PNG trong suốt và khung tham chiếu tin cậy, máy chỉ làm mới vùng thay đổi để giảm nháy.
- Chuyển FreeRTOS trên S31 sang 100 Hz. Giải phóng ngăn xếp luồng rung khi rảnh và ghi thời gian Light Sleep vào nhật ký.

## English

Applies to dpReader Full and Love Book on the Metalio E-Ink 4 Plus 4.7-inch reader.

- Swipe along the left edge while reading to select one of ten brightness steps or 0%. The **Brightness** notice shows the percentage, closes one second after the swipe stops, and can be disabled in **Reading settings**.
- Rename the **Rotate** option and move the full-refresh interval into **Page-turn effects**.
- On manual lock, the frontlight turns off and the current page stays visible until the lock art is ready. Transparent PNG lock art updates only the changed region when a valid reference frame is available.
- Run FreeRTOS at 100 Hz on S31. Free the haptic worker stack when idle and log Light Sleep duration.

[Web Flash](https://dephia31.github.io/dpReader31/) · [Web Flash in English](https://dephia31.github.io/dpReader31/?lang=en)
