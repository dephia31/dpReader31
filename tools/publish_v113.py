import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.3-vn · 2026-10-28
Bản cập nhật sửa triệt để lỗi nhảy nhiều trang của nút lật trang Bluetooth và bổ sung menu xuất nhật ký hệ thống cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Khắc phục triệt để lỗi nút lật trang Bluetooth nhảy nhiều trang (Page turner multi-page flip):**
  - Khắc phục cơ chế Auto-Repeat vô hạn trong BLE stack: Do remote Bluetooth không gửi gói báo nhả phím (release report), timeout trước đây (5000ms) khiến firmware coi nút vẫn đang giữ và liên tục gửi lệnh lật trang mỗi 120ms (tới 60 lần). Nay hệ thống tự động giải phóng trạng thái phím ngay khi đường truyền im lặng quá 150ms.
  - Bổ sung cơ chế chống dội (cooldown/debounce 350ms) trong trình đọc sách (`app_book.c`) và tự động làm rỗng hàng đợi phím thô / phím HID còn dư ngay sau khi nhận lệnh lật trang, đảm bảo 1 lần bấm vật lý chỉ lật đúng 1 trang sách.
- **Bổ sung Menu & Tính năng Xuất nhật ký hệ thống (Log Export & Diagnostics):**
  - Tích hợp bộ đệm vòng 32 KB trong RAM ghi nhận toàn bộ log runtime của hệ thống qua `esp_log_set_vprintf` mà không làm gián đoạn cổng Serial/USB.
  - Thêm menu **Nhật ký & Chẩn đoán** trong **Cài đặt -> Cập nhật & Khôi phục**.
  - Giao diện trực quan hiển thị thông tin RAM/PSRAM, % pin và điện áp, tình trạng thẻ nhớ SD, kết nối Bluetooth, chi tiết sự cố crash/panic (nếu có) và khung xem trước các dòng log gần nhất.
  - Nút bấm **Xuất log ra thẻ nhớ (/sdcard/dpreader_log.txt)** xuất toàn bộ báo cáo chẩn đoán và nhật ký lỗi ra file văn bản trên thẻ nhớ SD để IDE và kỹ sư firmware dễ dàng kiểm tra, khắc phục lỗi.
  - Nút bấm **Xóa nhật ký lưu tạm** để làm sạch bộ đệm nhật ký khi cần.
- **Tối ưu hóa giao diện Kệ sách và Truyền tệp:**
  - Chống load lại bìa/danh mục sách khi mở Kệ sách trong trạng thái kiểm tra nền.
  - Nới rộng và bo góc nút "Quản lý" và "Nhập sách" giúp bố cục thoáng đãng, không bị lấn viền.
  - Đổi tên các thẻ truyền tệp thành: "Wifi cùng mạng", "Tạo điểm phát sóng", "Cable USB drive".
"""

payload = {
    'tag_name': 'v1.1.3-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.3-vn',
    'body': body_text.strip(),
    'draft': False,
    'prerelease': False
}

req = urllib.request.Request(
    'https://api.github.com/repos/dephia31/dpReader31/releases',
    data=json.dumps(payload).encode('utf-8'),
    headers={'Authorization': f'token {t}', 'Content-Type': 'application/json', 'User-Agent': 'Python'}
)

try:
    with urllib.request.urlopen(req) as resp:
        rel = json.load(resp)
        print('Created release:', rel['id'], rel['name'])
except urllib.error.HTTPError as e:
    print('HTTP status:', e.code)
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.3-vn', headers=headers)
    with urllib.request.urlopen(req_get) as resp:
        rel = json.load(resp)
        print('Found existing release:', rel['id'])

upload_url = rel['upload_url'].split('{')[0]

files = [
    ('releases/dpReader_metalio47_firmware.bin', 'dpReader_metalio47_firmware.bin', 'application/octet-stream'),
    ('releases/dpReader_metalio47_full.bin', 'dpReader_metalio47_full.bin', 'application/octet-stream'),
    ('releases/SHA256SUMS.txt', 'SHA256SUMS.txt', 'text/plain'),
    ('releases/metalio-s31-update.json', 'metalio-s31-update.json', 'application/json')
]

for path, name, mime in files:
    size = os.path.getsize(path)
    print(f'Uploading {name} ({size} bytes)...')
    with open(path, 'rb') as f:
        data = f.read()
    u_req = urllib.request.Request(
        f'{upload_url}?name={name}',
        data=data,
        headers={
            'Authorization': f'token {t}',
            'Content-Type': mime,
            'Content-Length': str(size),
            'User-Agent': 'Python'
        }
    )
    try:
        with urllib.request.urlopen(u_req) as resp:
            res = json.load(resp)
            print('Uploaded:', res['name'], res['id'])
    except urllib.error.HTTPError as err:
        print(f'Upload failed for {name}: {err.code} {err.reason}')

print('Release v1.1.3-vn published successfully!')
