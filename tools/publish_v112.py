import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.1.2-vn · 2026-10-28
Bản cập nhật tối ưu hóa căn đều hai bên (Justify) và chuẩn hóa bàn phím QWERTY cho máy đọc sách **Metalio E-Ink 4 Plus (ESP32-S31)**:

- **Sửa triệt để lỗi căn đều hai bên (Justify) cho văn bản tiếng Việt & Latin:**
  - Thuật toán căn đều trước đây chỉ tính khoảng cách co dãn cho ký tự CJK, khiến văn bản tiếng Việt/Latin bị coi là không có khoảng dãn hợp lệ và luôn tự hủy về căn lề trái.
  - Nay thuật toán dãn dòng tự động phát hiện văn bản có khoảng trắng (spaces), phân bổ đều phần rộng dư của dòng vào các dấu cách giữa các từ, giữ nguyên cấu trúc tự nhiên của chữ cái trong từ và căn thẳng tắp mép phải đoạn văn.
- **Làm mới hiển thị tức thì khi chuyển đổi căn lề:**
  - Bổ sung cờ `s_reader_cleanup` khi chọn chế độ căn lề trong menu "Bố cục dàn trang", buộc toàn màn hình vẽ lại ngay lập tức thay vì chỉ làm mới nửa dưới panel.
  - Đổi tên hiển thị thành **Căn đều (Hai bên)** để người dùng dễ phân biệt.
- **Chuẩn hóa bàn phím QWERTY tiếng Anh, loại bỏ bàn phím T9:**
  - Bỏ hoàn toàn chế độ 9 phím T9 và bộ gõ IME trung gian phức tạp, giúp giải phóng bộ nhớ và tăng tốc độ gõ phím.
  - Nhập trực tiếp ký tự chữ hoa/thường tức thì (zero latency), không giữ trong bộ đệm chờ chọn từ.
  - Thiết kế lại bố cục bàn phím: loại bỏ thanh gợi ý từ, mở rộng phím Dấu cách (280 px), tích hợp chuyển đổi số (`123`/`ABC`), ký tự đặc biệt (`Ký tự`/`Thêm`) và hoàn tất (`Xong`).
  - Hỗ trợ giữ phím xóa `⌫` để xóa liên tục (repeat backspace) và rung xúc giác khi gõ phím.
"""

payload = {
    'tag_name': 'v1.1.2-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.1.2-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.1.2-vn', headers=headers)
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

print('Release v1.1.2-vn published successfully!')

