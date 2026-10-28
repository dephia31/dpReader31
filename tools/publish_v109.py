import urllib.request, json, subprocess, os

token = subprocess.check_output(['git', 'credential', 'fill'], input=b'protocol=https\nhost=github.com\n\n').decode()
t = [line.split('=')[1] for line in token.splitlines() if line.startswith('password=')][0]
headers = {'Authorization': f'token {t}', 'Accept': 'application/vnd.github.v3+json', 'User-Agent': 'Python'}

body_text = """### dpReader v1.0.9-vn · 2026-10-28
- **Sửa dứt điểm lỗi đồng bộ tiến độ KoSync với KOReader:**
  - Phân tách định dạng XPointer: chuỗi crengine `.../text().[index]` chỉ lấy số hiệu chương (fragment number) và tính vị trí theo tỉ lệ %, không nhầm lẫn chỉ số ký tự thành byte offset gây nhảy về đầu chương hoặc sai trang.
  - Sửa lỗi lượng tử hóa tỉ lệ 1%: So sánh trực tiếp tỉ lệ số thực dấu phẩy động thay vì làm tròn số nguyên, giúp kéo về chính xác khi tiến độ trên máy chủ KOReader mới hơn.
  - Sửa lỗi bỏ qua tiến độ khi mở sách: Tự động đồng bộ và áp dụng tiến độ từ máy chủ theo chương và tỉ lệ kể cả khi chuỗi pointer không có byte offset chính xác.
  - Bổ sung bộ kiểm thử `tools/kosync_progress_host_test.py` khoá chặt định dạng pointer và quy tắc chọn hướng đồng bộ.
"""

payload = {
    'tag_name': 'v1.0.9-vn',
    'name': 'dpReader Metalio 4.7 Firmware v1.0.9-vn',
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
    req_get = urllib.request.Request('https://api.github.com/repos/dephia31/dpReader31/releases/tags/v1.0.9-vn', headers=headers)
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
    with urllib.request.urlopen(u_req) as resp:
        res = json.load(resp)
        print('Uploaded:', res['name'], res['id'])

print('Release v1.0.9-vn published successfully!')

