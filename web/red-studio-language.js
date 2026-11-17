(() => {
  const url = new URL(window.location.href);
  const english = url.searchParams.get("lang") === "en";
  for (const language of ["vi", "en"]) {
    const link = document.getElementById(`language-${language}`);
    const destination = new URL(url);
    if (language === "en") destination.searchParams.set("lang", "en");
    else destination.searchParams.delete("lang");
    link.href = destination.href;
    if (english === (language === "en")) link.setAttribute("aria-current", "page");
  }

  if (!english) return;
  document.documentElement.lang = "en";
  const copy = new Map(Object.entries({
  "dpReader Metalio Plus Flasher - Trình Nạp Firmware Web": "dpReader Metalio Plus Web Flasher",
  "MỘT KẾT NỐI. MỘT KHỞI ĐẦU MỚI.": "USB FIRMWARE INSTALLER",
  "Sẵn sàng cho chương tiếp theo.": "Install or update dpReader",
  "Trình nạp & Nâng cấp Firmware trực tiếp qua cổng USB cho máy đọc sách Metalio 4.7 (ESP32-S31)": "Flash firmware over USB on the Metalio 4.7 e-reader (ESP32-S31)",
  "⚠️ Trình duyệt của bạn chưa hỗ trợ Web Serial. Vui lòng mở trang này trên Google Chrome, Microsoft Edge, Opera hoặc Brave.": "⚠️ This browser does not support Web Serial. Open this page in Chrome, Edge, Opera or Brave.",
  "✓ Trình duyệt hỗ trợ nạp Web Serial trực tiếp": "✓ Web Serial is available",
  "Kết nối máy đọc sách qua cáp USB Type-C": "Connect the e-reader over USB-C",
  "Cắm cáp USB Type-C nối máy đọc sách với máy tính, sau đó bấm nút bên dưới và chọn cổng kết nối.": "Connect the reader to your computer using a USB-C data cable, then click the button below and select its port.",
  "🔌 Kết Nối Thiết Bị": "🔌 Connect device",
  "✕ Ngắt Kết Nối": "✕ Disconnect",
  "Trạng thái": "Status",
  "Đã kết nối ✓": "Connected ✓",
  "Loại Chip": "Chip",
  "Địa chỉ MAC": "MAC address",
  "Đang đọc...": "Reading…",
  "Thiết bị": "Device",
  "Lựa chọn Firmware & Chế độ nạp": "Select firmware and installation mode",
  "Phiên bản firmware": "Firmware edition",
  "Full — đầy đủ tính năng và thư viện": "Full — all features and the library",
  "Love Book — I Love Book, mở sách bằng file, không TTS/kệ sách": "Love Book — simplified Home and file picker",
  "Cập nhật dpReader qua USB": "Update dpReader over USB",
  "Đang dùng 1.4.1 trở xuống hoặc firmware khác": "Running 1.4.1 or earlier, or other firmware",
  "· Chọn ô": "· Select",
  "Nạp toàn phần": "Full install",
  "để nạp ảnh 16 MB. Trước khi nạp, sao lưu cấu hình ra thẻ SD. Nạp toàn phần xóa dữ liệu trong máy và giữ nguyên thẻ SD. Sau khi nạp, chọn": "to flash a 16 MB image. Back up settings to the SD card first. A full install erases internal data and preserves the SD card. After flashing, select",
  "Khôi phục cấu hình từ thẻ nhớ": "Restore settings from SD",
  "Cài đặt → Sao lưu & Khôi phục": "Settings → Backup & Restore",
  "Đang dùng 1.4.2": "Running 1.4.2",
  "Cập nhật ứng dụng": "Application update",
  "để cập nhật bằng Web Flash. Bạn cũng có thể dùng bộ cài USB. Khi nhận diện đúng phân vùng NVS 256 KiB, trang chỉ nạp ứng dụng và giữ cài đặt. Từ 1.4.3 có thể cập nhật qua Wi-Fi.": "to update in this browser. The USB installer also works. With a compatible 256 KiB NVS layout, only the application is written and settings are kept. Wi-Fi updates are available from 1.4.3.",
  "Bản 1.4.2 được chọn trong danh sách luôn cài sạch toàn phần.": "Selecting the archived 1.4.2 build always performs a full install.",
  "Chọn Full hoặc Love Book, kết nối Metalio 4.7 S31 ở chế độ BOOT và bấm": "Choose Full or Love Book, connect the Metalio 4.7 S31 in BOOT mode and click",
  "Bắt đầu nạp firmware": "Start flashing",
  ". Với bộ cài tải về, chạy": ". For the downloadable installer, run",
  "trên macOS hoặc": "on macOS or",
  "trong": "under",
  "trên Windows.": "on Windows.",
  "Ảnh 16 MB dành cho máy đang dùng 1.4.1 trở xuống, firmware khác hoặc cần cứu máy. Xóa dữ liệu trong máy, giữ thẻ SD.": "16 MB image for 1.4.1 or earlier, other firmware or recovery. Erases internal data and preserves the SD card.",
  "Dành cho máy đã dùng dpReader 1.4.2 trở lên với NVS 256 KiB. Giữ cài đặt và dữ liệu.": "For dpReader 1.4.2 and later with 256 KiB NVS. Keeps settings and data.",
  "Phiên bản để nạp:": "Version to install:",
  "1.5.2-vn — mới nhất": "1.5.2-vn — latest",
  "1.5.1-vn — bản chính thức trước": "1.5.1-vn — previous release",
  "Tải ứng dụng cập nhật qua thẻ TF": "Download the microSD update",
  "Tải bản đầy đủ cho USB": "Download the full USB image",
  "Tải bộ cài USB cho máy tính": "Download the desktop USB installer",
  "Phiên bản đã chọn:": "Selected version:",
  "Đang tải...": "Loading…",
  "Đang kiểm tra thông tin bản phát hành mới nhất từ máy chủ...": "Checking release information…",
  "Nạp từ file .bin trên máy tính": "Flash a local .bin file",
  "Xóa toàn bộ flash trong máy rồi ghi ảnh cài toàn phần từ 0x0. Thẻ SD giữ nguyên.": "Erases internal flash and writes a full installation image at 0x0. Preserves the SD card.",
  "Chọn tệp .bin:": "Select a .bin file:",
  "Địa chỉ Offset (Hex):": "Offset (hex):",
  "Tốc độ nạp (Baud Rate):": "Baud rate:",
  "921,600 baud (Tốc độ cao)": "921,600 baud (high speed)",
  "230,400 baud (Ổn định khi cáp dài)": "230,400 baud (more stable with long cables)",
  "115,200 baud (Khuyến nghị cho ESP32-S31)": "115,200 baud (recommended for ESP32-S31)",
  "Tiến hành nạp Firmware": "Flash firmware",
  "Xác nhận trước khi nạp": "Before flashing",
  "Nạp firmware có thể làm mất thiết lập hoặc dữ liệu, khiến thiết bị không khởi động nếu chọn sai gói hoặc quá trình bị gián đoạn. Hãy sao lưu cấu hình ra thẻ SD và giữ kết nối USB ổn định.": "Flashing can erase settings or data. An incompatible image or an interruption can leave the device unable to boot. Back up settings to the SD card and keep USB connected.",
  "Tôi đã đọc, hiểu và tự chịu trách nhiệm về các rủi ro khi nạp firmware vào thiết bị của mình.": "I understand and accept the risks of flashing this device.",
  "Kết nối thiết bị và tích chấp nhận rủi ro để bắt đầu.": "Connect the device and accept the risks to continue.",
  "Tiến trình nạp": "Flash progress",
  "Sẵn sàng...": "Ready…",
  "Nhật ký giao tiếp Serial:": "Serial log:",
  "Xóa log": "Clear log",
  "Sẵn sàng kết nối thiết bị qua Web Serial...": "Ready to connect over Web Serial…",
  "💡 Hướng dẫn & Khắc phục sự cố": "💡 Troubleshooting",
  "Trường hợp máy tính không nhận cổng USB hoặc nạp báo lỗi:": "If the computer cannot see the USB port or flashing fails:",
  "Đưa máy vào chế độ BOOT (Download Mode) thủ công:": "Enter BOOT (download mode) manually:",
  "Giữ phím": "Hold the",
  "bên hông trong khi nhấn giữ phím": "side button while holding",
  "Nguồn": "Power",
  "khoảng 3-5 giây rồi nhả cả hai ra (hoặc giữ phím AI rồi cắm cáp USB vào máy tính).": "for 3–5 seconds, then release both. You can also hold AI while plugging in USB.",
  "Kiểm tra cáp USB Type-C:": "Check the USB-C cable:",
  "Đảm bảo sử dụng cáp Type-C có đủ 4 chân dữ liệu (Data cable), tránh dùng cáp chỉ sạc.": "Use a USB-C cable that supports data.",
  "Quyền truy cập cổng Serial:": "Serial port permissions:",
  "Trên Linux, bạn có thể cần thêm người dùng vào nhóm": "On Linux, you may need to add your user to the",
  "). Trên macOS và Windows, trình duyệt sẽ tự động xin quyền truy cập.": "). On macOS and Windows, the browser prompts for access.",
  "MỘT CHÚT ỦNG HỘ, MỘT NIỀM VUI LỚN": "SUPPORT",
  "Yêu thích dpReader? Ủng hộ Mận Quý nhé!": "Enjoy dpReader? Support Mận Quý",
  "Nếu yêu thích firmware dpReader, bạn có thể ủng hộ cửa hàng Mận Quý của mình bằng một lượt đánh giá 5 sao trên Google hoặc Thích / Theo dõi fanpage. Cảm ơn bạn rất nhiều!": "You can support Mận Quý by leaving a Google review or following our Facebook page. Thank you!",
  "Đánh giá 5 sao trên Google": "Review on Google",
  "Ghé fanpage Mận Quý": "Visit Mận Quý on Facebook",
  "Dự án": "Project",
  "· Firmware Metalio 4.7 Plus (ESP32-S31) · Mã nguồn mở Apache-2.0": "· Firmware for Metalio 4.7 Plus (ESP32-S31) · Apache-2.0",
  "Giao diện Home": "Home layouts",
  "Đầy đủ": "Full"
}));
  // Translate text nodes without replacing controls or changing firmware URLs.
  const nodes = document.createTreeWalker(document.documentElement, NodeFilter.SHOW_TEXT);
  while (nodes.nextNode()) {
    const node = nodes.currentNode;
    if (node.parentElement.closest("script, style")) continue;
    const value = node.nodeValue.trim();
    const translation = copy.get(value);
    if (translation !== undefined) node.nodeValue = node.nodeValue.replace(value, translation);
  }
  document.getElementById("language-switch").setAttribute("aria-label", "Page language");
})();
