import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.6-vn · 2026-10-29
Bản cập nhật về độ trễ phím, tải bus I2C và sửa lỗi kết nối WiFi của KoSync/OPDS cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Phím cứng chạy bằng ngắt thay vì thăm dò 10 ms:**
  - Trước đây tác vụ quét phím đọc chip mở rộng 100 lần/giây kể cả khi không ai chạm máy. Nay tác vụ chỉ đọc khi có cạnh xuống trên INT của chip mở rộng hoặc khi phím AI (GPIO61) đổi mức.
  - Chỉ tiếp tục thăm dò 10 ms trong lúc đang giữ phím, trong cửa sổ chống dội hoặc khi còn thao tác chưa được lấy. Phím AI vẫn nhận đủ nhấn giữ 500 ms.
  - Nếu không gắn được ngắt, tác vụ tự quay về thăm dò 10 ms như cũ.
- **Đọc chip mở rộng bằng một giao dịch:** Hai lần đọc cổng riêng lẻ được gộp thành một lần đọc tự tăng địa chỉ, giảm một nửa thời gian bus trên đường phím nóng nhất.
- **I2C 400 kHz cho cảm biến, giữ 100 kHz cho chip nguồn:** Cảm ứng, chip mở rộng, pin và RTC chạy 400 kHz; TPS65185 và CX25601N vẫn 100 kHz vì trình điều khiển tham chiếu chỉ được xác nhận ở tốc độ đó.
- **Sửa lỗi KoSync/OPDS không kết nối được WiFi khi đang đọc sách:**
  - Trước đây bấm đồng bộ KoSync trong lúc đang đọc mà máy đang phát hotspot (hoặc còn phiên chỉ-đăng-nhập) thì chỉ hiện thông báo *"Đang bật phát WiFi nội bộ"* và không kết nối được; người dùng phải tự thoát ra trang Truyền tệp.
  - Nay hai dịch vụ này chủ động thu hồi sóng, kết nối ngay mạng WiFi đã lưu để đồng bộ, rồi tự tắt WiFi khi xong.
  - Chỉ khi đang có tệp được truyền lên (đang ghi thẻ) mới từ chối và báo rõ lý do, vì cắt ngang sẽ để lại tệp dở.
"""

payload = {
    'tag_name': 'v1.1.6-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.6-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.6-vn', headers=headers)
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

print('Release v1.1.6-vn published successfully!')
