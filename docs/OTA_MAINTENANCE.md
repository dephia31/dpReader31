# Bảo trì OTA dpReader

Với dpReader S31, nguồn hiện hành cho Full và Love Book là GitHub Pages tại `https://dephia31.github.io/dpReader31/`. Full dùng `/metalio-s31-update.json`. Love Book dùng `/releases/love-book/metalio-s31-update.json`. Cả hai có nguồn dự phòng tương ứng trên raw GitHub. Xem [tài liệu hai phiên bản](FIRMWARE_EDITIONS.md). Gói chính thức và gói lịch sử được lưu trên GitHub Releases. Thiết bị đã cài giữ nguyên thứ tự nguồn được nhúng trong firmware.

Các bản cũ rc97 và rc98 từng dùng nguồn `https://wegooo-cell.github.io/read-pico-reader/` và tên miền `https://kiikoread.com`. Thiết bị chạy rc97 đã tải và xác minh được rc98 từ GitHub Pages. Giữ URL đã phát hành và các nguồn HTTPS mà firmware cũ cho phép. Với client cũ, tránh URL đính kèm Release có chuyển hướng. Trình duyệt tải được chỉ xác nhận khả năng truy cập từ trình duyệt.

Trước khi phát hành và sau khi gia hạn chứng chỉ, kiểm tra các nguồn đang hỗ trợ. Công cụ cần Python có `cryptography` và curl hỗ trợ `%{certs}`.

```sh
python3 tools/ota_tls_host_test.py
python3 tools/check_official_ota.py --output build/ota-health.json
```

`check_official_ota.py` kiểm tra nguồn website cũ. Có thể truyền `--trust-image /path/to/original-rc97.bin` để dùng bản gốc cục bộ. Công cụ đối chiếu SHA đã cố định. Mặc định công cụ lấy ứng dụng rc97 gốc từ website. Báo cáo nằm trong thư mục build bị Git bỏ qua và không chứa token.

Kiểm tra TLS 1.2/HTTP 1.1, thời hạn và chữ ký chứng chỉ, khả năng tương thích của bên cấp ở cuối chuỗi với gói chứng chỉ rc96/97/98, manifest, phiên bản nhúng, kích thước, SHA-256 và nội dung trả về khi tải tiếp bằng HTTP 206 Range. Client cũ không dựng lại chuỗi ký chéo, vì vậy thấy tên gốc đáng tin ở giữa chuỗi vẫn chưa đủ để kết luận tương thích.

Mỗi bản phát hành cần đối chiếu nguồn GitHub Pages thực tế và thử luồng kiểm tra, tải và xác minh trên thiết bị chạy bản đã phát hành. Tình trạng của GitHub Pages và khả năng tương thích chứng chỉ của website cũ cần xác minh riêng. Giữ HTTPS và xác minh TLS/SHA, giữ nguyên byte của gói đã phát hành và dọn thông tin xác thực tạm sau khi thao tác chứng chỉ.

Tham khảo [gói chứng chỉ Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_crt_bundle.html) và [Cloudflare Universal SSL API](https://developers.cloudflare.com/api/resources/ssl/subresources/universal/subresources/settings/methods/edit/).
