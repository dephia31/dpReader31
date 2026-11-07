# Đóng góp cho dpReader

Bạn có thể đóng góp firmware, mã hỗ trợ bo mạch, trình điều khiển, công cụ và tài liệu. Có thể viết issue và pull request bằng tiếng Việt, tiếng Anh, tiếng Trung giản thể hoặc tiếng Nhật. Mỗi PR nên tập trung vào một vấn đề. Hãy thảo luận trước trong issue nếu đề xuất thay đổi lớn về hành vi hoặc phần cứng.

Xem [quy định sử dụng, thương mại và donate](docs/LICENSING.md). Việc chấp nhận PR không cấp phép thương mại cho người đóng góp hoặc dịch vụ của họ.

## Giấy phép đóng góp

- Đọc [LICENSE](LICENSE) trước khi đóng góp. Giữ giấy phép, thông báo bản quyền và mã SPDX của các tệp hiện có và tài liệu bên thứ ba. Việc sửa một tệp không tự động biến tệp đó thành nội dung cấm sử dụng thương mại.
- Nội dung mới do bạn sáng tạo chỉ có thể dùng [Giấy phép học tập cá nhân 1.0](licenses/kiikoread-Personal-Study-1.0.txt) khi chủ sở hữu quyền đồng ý rõ ràng và thông báo bản quyền chính xác. Đánh dấu tệp mới bằng `SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0`. Với tệp có nhiều giấy phép, xác định rõ phần bổ sung và giấy phép của phần đó. Khi chưa phân định được, giữ giấy phép hiện có và thảo luận trước khi gửi.
- Ghi trong PR giấy phép của phần đóng góp mới và nguồn của phần dùng lại. Việc gửi PR không chuyển giao bản quyền, cho phép đổi giấy phép thay người khác hay thu hồi quyền đã cấp. Làm rõ căn cứ giấy phép trước khi merge.
- Giấy phép học tập cá nhân cho phép chia sẻ miễn phí để học tập và yêu cầu chấp thuận riêng bằng văn bản cho mục đích thương mại. Apache-2.0, MIT, ISC, LGPL và OFL tiếp tục áp dụng cho từng phần tương ứng, bao gồm quyền sửa đổi, gỡ lỗi và liên kết lại theo LGPL.

## Phát triển

Đọc [README.md](README.md) về phần cứng và cách build bằng ESP-IDF v6.1. Đọc [AGENTS.md](AGENTS.md) về trách nhiệm từng thành phần, callback màn hình và các quyết định cố định. Tận dụng component và hàm vẽ hiện có. Thêm trang demo qua bảng đăng ký.

Giữ timing theo bo trong `sdkconfig.defaults`. `sdkconfig.ci` dùng để kiểm tra biên dịch. VCOM do nhà máy hiệu chuẩn trong PMU và trang nguồn SY7636A chỉ đọc. Nếu cần thay đổi một quyết định cố định, giải thích lý do và cập nhật phần mô tả quyết định trước khi sửa hành vi.

## Pull request

- Nêu vấn đề, hành vi sau thay đổi, thành phần liên quan và issue tương ứng.
- Ghi rõ các bước đã kiểm tra và phần còn chờ xác minh. Khi thử trên máy thật, ghi bo mạch và cấu hình. Thay đổi chỉ về tài liệu có thể xác minh bằng kiểm tra nội dung và liên kết.
- Đồng bộ các bản ngôn ngữ của README khi có thể, hoặc ghi rõ phần còn chờ dịch.
- Giữ thông báo SPDX và giấy phép bên thứ ba. Chỉ gửi nội dung bạn có quyền phân phối. Tệp hãng, phông chữ, waveform và tài nguyên mới phải có nguồn và quyền phân phối rõ ràng.
- Bỏ thông tin đăng nhập, mã thiết bị riêng, nội dung cá nhân và tài liệu mật của nhà cung cấp khỏi tệp, log và ảnh chụp.

Xem [Security](SECURITY.md) để báo cáo riêng, [Support](SUPPORT.md) để được trợ giúp và [Code of Conduct](CODE_OF_CONDUCT.md) khi tham gia cộng đồng.
