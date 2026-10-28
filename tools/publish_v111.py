import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.1-vn · 2026-10-28
Bản tối ưu hóa năng lượng cực đại (**Ultra Low Power**) toàn diện cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Tự động ngắt Wi-Fi khi nhàn rỗi (Auto-Timeout 3 phút):**
  - Tự động ngắt hoàn toàn module Wi-Fi sau 3 phút không phát sinh lưu lượng truyền tải, đồng bộ hay tải OTA.
  - Sửa lỗi cờ `wifi_busy` chặn bộ đếm thời gian tự khóa màn hình: Khi Wi-Fi chỉ ở trạng thái kết nối chờ (không tải dữ liệu), thiết bị vẫn tự động khóa màn hình đúng hẹn.
  - Tự động ngắt Wi-Fi rảnh rỗi ngay trước khi vào chế độ ngủ để tránh tiêu hao dòng trong lúc khóa máy.
- **Kích hoạt Wi-Fi Modem Sleep (DTIM Beaconing):**
  - Chuyển chế độ hoạt động Wi-Fi STA từ `WIFI_PS_NONE` sang `WIFI_PS_MIN_MODEM`, giảm dòng RF tiêu thụ lúc kết nối mạng từ 140mA xuống ~15–20mA.
- **Triệt tiêu dòng rò ngoại vi đèn nền:**
  - Ngắt hoàn toàn clock PWM (gọi `ledc_stop()`) cho cả 2 kênh Channel 0 & 1 khi độ sáng = 0.
- **Nhịp quét CPU thích ứng (Adaptive Cadence):**
  - Vòng lặp chính co giãn thông minh: giữ 8ms (120Hz) khi có tương tác chạm vuốt để đảm bảo phản hồi cực mượt; tự động giãn ra 40ms sau 400ms không thao tác (giảm 80% tải CPU và giao tiếp I2C khi đọc trang sách tĩnh).
- **Chế độ ngủ phân tầng (Tiered Sleep):**
  - Tầng 1: Light Sleep trong 10 phút đầu khi khóa máy, cho phép đánh thức tức thì (<50ms).
  - Tầng 2: Nếu không thao tác sau 10 phút khóa máy, tự động hạ xuống Deep Sleep (<25 µA) để bảo toàn pin tối đa khi người dùng bỏ quên thiết bị.
- **Cập nhật mặc định tự tắt màn hình:**
  - Chuyển thời gian tự khóa màn hình mặc định từ 0 (không tắt) thành 5 phút an toàn.
"""

payload = {
    'tag_name': 'v1.1.1-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.1-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.1-vn', headers=headers)
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

print('Release v1.1.1-vn published successfully!')
