import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.4-vn · 2026-10-28
Bản cập nhật tinh chỉnh giao diện đọc sách, tái cấu trúc menu cài đặt phông chữ và bổ sung tùy chọn hiển thị chân trang cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Tái cấu trúc menu Cài đặt phông chữ & Gom nhóm "Tùy Chỉnh":**
  - Tạo riêng menu con **Tùy chỉnh** gom gọn 3 mục: *Phông chữ nhúng trong sách*, *Đường dẫn dòng* (thước đọc sách) và *Khử răng cưa* (làm mịn nét chữ).
  - Giúp bảng Cài đặt phông chữ chính trở nên thoáng đãng, phân cấp logic và dễ sử dụng.
- **Kéo Menu "Thanh chân trang" (Footer) ra ngoài:**
  - Chuyển mục cài đặt chân trang ra trực tiếp bảng Cài đặt phông chữ chính (nằm ngang hàng với Bố cục dàn trang), không còn bị ẩn sâu ở đáy bảng Bố cục như trước.
- **Bổ sung các tùy chọn hiển thị chi tiết cho Chân trang (Footer Display Options):**
  - Bổ sung tùy chọn bật/tắt độc lập từng thành phần ở chân trang sách:
    - **Thanh tiến độ**: Tùy chỉnh độ dày vạch (*Ẩn / Mảnh / Dày / Rất mảnh*).
    - **Số trang / chương (trái)**: Bật/Tắt hiển thị số trang hiện tại trong chương (ví dụ `Trang 3/12`).
    - **Tên Chương (nằm trên thanh tiến độ)**: Bật/Tắt hiển thị tên chương nằm căn giữa phía trên thanh tiến độ.
    - **% tiến độ sách (phải)**: Bật/Tắt hiển thị % đọc toàn cuốn sách ở góc phải (ví dụ `45%`).
  - Toàn bộ cài đặt được lưu trữ bền vững vào NVS và tự động đồng bộ khi chuyển trang.
"""

payload = {
    'tag_name': 'v1.1.4-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.4-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.4-vn', headers=headers)
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

print('Release v1.1.4-vn published successfully!')
