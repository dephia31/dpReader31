# Cài sạch NVS 256 KiB trên S31

Bản 1.4.2RC3 có một phân vùng NVS 256 KiB tại `0xFB0000` và storage 5.625 MiB tại `0xA10000`. Bảng phân vùng không khai báo `nvs_old`. Hai khe firmware và vùng chọn boot OTA giữ địa chỉ cũ.

Mở `FLASH_USB.command` trên macOS hoặc `FLASH_USB.bat` trên Windows. Nếu máy dùng bố cục cũ hoặc bảng phân vùng trống, launcher tự cài sạch trong một lần chạy. Cài sạch xóa cài đặt trong NVS, PIN, khóa BLE và tệp trong bộ nhớ trong `/flash`. Thẻ SD giữ nguyên. Bộ cài tạo sẵn ảnh FAT trống và ghi bảng phân vùng sau cùng. Máy đã có bố cục RC3 chỉ ghi ứng dụng và vùng chọn boot.

Có thể khôi phục thủ công từ bản sao lưu SD trong Cài đặt → Sao lưu & Khôi phục cấu hình. PIN và khóa ghép đôi BLE cần thiết lập lại. Các bản cập nhật OTA dùng bố cục mới sau khi cài sạch qua USB.

Nếu USB bị ngắt trước khi ghi bảng phân vùng, chạy lại launcher trên cùng máy. Nếu launcher báo bảng phân vùng không hợp lệ, dừng và giữ log để kiểm tra.
