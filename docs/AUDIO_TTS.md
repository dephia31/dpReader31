# Âm thanh và đọc thành tiếng trên S31

## Bước1: nghe WAV từ thẻ

Bản1.2.2RC1-vn thêm đường phát âm thanh cục bộ để kiểm tra loa trước khi tích hợp TTS. Chép `TEST_AUDIO.wav` trong gói thử vào thẻ nhớ. Trên máy vào **Tệp → duyệt thẻ nhớ**, chọn tệp rồi **Mở**, chạm **Phát lại từ đầu**. Phím giữa phát/dừng; trái/phải trở về. WAV nhận PCM không nén16bit,16kHz,mono hoặc stereo; stereo được trộn mono rồi đưa ra hai slotI2S. Chưa nhận MP3/AAC hay WAV24kHz/float. Âm lượng thử cố định15%.

Có thể tạo mẫu không cần Internet bằng `python3 tools/generate_audio_test.py /path/TEST_AUDIO.wav`. Mẫu gồm ba đoạn âm440/660/880Hz ngắn, không phải giọng TTS. Để chuyển giọng có sẵn: `ffmpeg -i input.mp3 -ar 16000 -ac 1 -c:a pcm_s16le output.wav` nếu máy tính có FFmpeg.

Chỉ phát sau thao tác người dùng, không tự phát khi khởi động hoặc mở trang. Dừng và chờ đóng tệp/tài nguyên trước khi rời trang, khóa/ngủ hoặc thông báo mất thẻ. Không tự tiếp tục sau mở khóa. Trong lúc phát không cập nhật tiến độ màn hình liên tục; trạng thái kết thúc/lỗi được vẽ một lần. Phát lại bắt đầu từ đầu. Không thay ánh sáng, âm lượng phím tắt, VCOM hoặc waveform.

## Phần cứng và giới hạn

Driver S31 dùng BH1098G, UART1 TX39/RX38 ở115200, I2S0slave BCLK42/WS43/DOUT44,16kHz/stereo32bit, PA qua expanderP4. Cấu hình UART dựa trên mã board nhà sản xuất: AT+RX=2, chờ700ms, AT+MODE=1 và xác nhận SET MODE1. Thiếu xác nhận hoặc lỗiI2S dừng với thông báo; không tự reset P14 dùng chung và không đổi nguồn/waveform màn hình. UART/I2S được giải phóng khi kết thúc. Mỗi lần phát có task6KiB, bufferPCM1KiB và slot2KiB cùng bufferDMA/UART củaSDK; không nạp cả WAV vào RAM.

Build và hosttest không xác nhận có âm thanh trên máy thật. Cần kiểm tra BHmodeACK, tiếng mẫu, phát/dừng nhiều lần, rời trang, khóa/mở khóa và rút thẻ. Nếu BH chưa xác nhận, ghi nhận nguyên thông báo; không suy ra loa hỏng hoặc tự bật reset nguồn. Các cấu hình máy khác không dùng driver S31 này.

## Các bước TTS tiếp theo

Sau khi đường phát được xác nhận: chia văn bản sách theo câu/đoạn, thêm bộ đệm có giới hạn và hủy theo sách/vị trí, rồi ghép nguồn Edge/Azure/server vào đườngPCM. Tải âm thanh về thẻ để nghe lại khi không có mạng khác với tự tổng hợp giọng offline. TTS tiếng Việt neural offline chưa được chứng minh phù hợpRAM/CPU hiện tại; eSpeak là hướng nghiên cứu riêng.

Kiểm tra tái lập: `python3 tools/audio_wav_host_test.py`, `python3 tools/audio_lifecycle_host_test.py`, `python3 tools/audio_page_host_test.py`, `python3 tools/audio_driver_host_test.py`.
