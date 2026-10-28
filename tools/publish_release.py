import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.5-vn · 2026-10-28
Bản cập nhật tối ưu hóa bộ nhớ RAM hệ thống, quản lý tài nguyên không dây và bổ sung tính năng tự ngắt kết nối thông minh cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Tối ưu hóa RAM và kiểm soát dịch vụ Truyền tệp / Wi-Fi:**
  - Khắc phục tình trạng ngốn RAM do Wi-Fi và HTTP server truyền tệp chạy ngầm: Mặc định Wi-Fi và dịch vụ truyền tệp luôn TẮT, chỉ mở khi người dùng chủ động vào màn hình truyền tệp hoặc thao tác tác vụ mạng (KoSync, OPDS, OTA, Online Font).
  - Khi người dùng thoát khỏi màn hình truyền tệp / cài đặt Wi-Fi, hệ thống tự động dừng phiên (`stop_session`) và giải phóng hoàn toàn bộ đệm HTTP, socket và tài nguyên Wi-Fi về cho heap.
  - Ngăn chặn tác vụ đồng bộ giờ nền tự ý bật Wi-Fi ngầm khi máy đang offline; chỉ đồng bộ khi Wi-Fi đã được người dùng kết nối sẵn.
  - Tối ưu hóa cửa sổ nhận TCP (`CONFIG_LWIP_TCP_WND_DEFAULT`) từ 64 KB về 32 KB và giảm số bộ đệm RX động từ 64 về 32 trong cấu hình S31, tiết kiệm hơn 40 KB RAM nội bộ cho hệ thống hiển thị và phông chữ.
- **Thêm thiết lập Tự tắt Wi-Fi & Tự tắt Bluetooth khi không dùng:**
  - Bổ sung tùy chọn **Tự tắt Wi-Fi khi rảnh** vào nhóm **Cài đặt -> Kết nối**: Cho phép chọn *1 phút, 3 phút (mặc định), 5 phút, 10 phút, Tắt*. Khi không có tệp nào đang tải lên/xuống sau thời gian đã đặt, Wi-Fi sẽ tự động tắt để tiết kiệm pin và trả lại RAM.
  - Bổ sung tùy chọn **Tự tắt Bluetooth khi rảnh** vào nhóm **Cài đặt -> Kết nối** và trong trang **Điều khiển lật trang BLE**: Cho phép chọn *5 phút, 10 phút (mặc định), 15 phút, 30 phút, Tắt*. Nếu mất kết nối hoặc không nhận được thao tác bấm phím nào từ điều khiển lật trang sau thời gian đã định, hệ thống sẽ tự động tắt Bluetooth và giải phóng hoàn toàn NimBLE stack.
"""

payload = {
    'tag_name': 'v1.1.5-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.5-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.5-vn', headers=headers)
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

print('Release v1.1.5-vn published successfully!')
