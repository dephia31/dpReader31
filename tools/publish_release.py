#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Công cụ tự động hóa phát hành Firmware dpReader Metalio 4.7 lên GitHub
và cập nhật kênh OTA tự động cho thiết bị.

Sử dụng:
    python3 tools/publish_release.py --version 1.0.1-vn --notes "Ghi chú phát hành tiếng Việt" --push
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
RELEASES_DIR = REPO_ROOT / "releases"
BUILD_DIR = REPO_ROOT / "build/boards"

OFFSETS = {
    "bootloader.bin": 0x2000,
    "partitions.bin": 0x8000,
    "ota_data_initial.bin": 0xd000,
    "firmware.bin": 0x10000,
}


def calculate_sha256(filepath: Path) -> str:
    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest()


def create_full_binary(packages_dir: Path, output_file: Path) -> None:
    print(f"[*] Đang tổng hợp firmware toàn diện từ {packages_dir}...")
    parts = {}
    max_end = 0
    for name, offset in OFFSETS.items():
        file_path = packages_dir / name
        if not file_path.exists():
            raise FileNotFoundError(f"Không tìm thấy thành phần bắt buộc: {file_path}")
        data = file_path.read_bytes()
        parts[offset] = data
        max_end = max(max_end, offset + len(data))

    # Flash ESP mặc định lấp đầy bằng 0xFF
    image = bytearray([0xFF] * max_end)
    for offset, data in parts.items():
        image[offset : offset + len(data)] = data

    output_file.write_bytes(image)
    print(f"[+] Đã tạo {output_file.name} (kích thước: {len(image):,} bytes)")


def update_manifests(version: str, sha256_app: str, size_app: int, notes: str) -> None:
    manifest_data = {
        "schema": 1,
        "version": version,
        "project": "Kiikoread_Metalio_S31",
        "board": "Metalio-EINK4-Plus-S31",
        "layout": "metalio-s31-dual-5m-v1",
        "minimum_base_version": "1.0.0",
        "url": "https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/dpReader_metalio47_firmware.bin",
        "sha256": sha256_app,
        "size": size_app,
        "notes": notes,
    }

    manifest_json = json.dumps(manifest_data, indent=2, ensure_ascii=False) + "\n"

    # Cập nhật releases/metalio-s31-update.json
    rel_manifest = RELEASES_DIR / "metalio-s31-update.json"
    rel_manifest.write_text(manifest_json, encoding="utf-8")
    print(f"[+] Đã cập nhật kênh OTA tại: {rel_manifest.relative_to(REPO_ROOT)}")

    # Cập nhật root metalio-s31-update.json (dành cho GitHub Pages)
    root_manifest = REPO_ROOT / "metalio-s31-update.json"
    root_manifest.write_text(manifest_json, encoding="utf-8")
    print(f"[+] Đã cập nhật kênh OTA tại: {root_manifest.relative_to(REPO_ROOT)}")


def update_checksums(firmware_bin: Path, full_bin: Path) -> str:
    sha_fw = calculate_sha256(firmware_bin)
    sha_full = calculate_sha256(full_bin)

    sums_content = (
        f"{sha_fw}  {firmware_bin.name}\n"
        f"{sha_full}  {full_bin.name}\n"
    )

    sums_file = RELEASES_DIR / "SHA256SUMS.txt"
    sums_file.write_text(sums_content, encoding="utf-8")
    print(f"[+] Đã cập nhật bảng mã băm SHA256SUMS.txt")
    return sha_fw


def archive_version(version: str) -> None:
    version_dir = RELEASES_DIR / version
    version_dir.mkdir(parents=True, exist_ok=True)

    files_to_copy = [
        "dpReader_metalio47_firmware.bin",
        "dpReader_metalio47_full.bin",
        "SHA256SUMS.txt",
        "metalio-s31-update.json",
    ]

    for filename in files_to_copy:
        src = RELEASES_DIR / filename
        dst = version_dir / filename
        if src.exists():
            dst.write_bytes(src.read_bytes())

    print(f"[+] Đã lưu trữ bản sao phát hành tại: {version_dir.relative_to(REPO_ROOT)}")


def run_git_commands(version: str, push: bool) -> None:
    tag = f"v{version}"
    print(f"[*] Đang thực hiện commit Git và gắn thẻ tag {tag}...")

    # Stage các file cần thiết
    subprocess.run(["git", "add", "-A"], check=True)

    # Commit nếu có thay đổi
    status = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout
    if status.strip():
        subprocess.run(["git", "commit", "-m", f"release: cập nhật phiên bản {tag} và kênh OTA"], check=True)
        print(f"[+] Đã commit thay đổi phát hành.")
    else:
        print(f"[*] Không có thay đổi mới cần commit.")

    # Tạo tag
    tags = subprocess.run(["git", "tag"], capture_output=True, text=True).stdout.split()
    if tag in tags:
        print(f"[*] Thẻ tag {tag} đã tồn tại, cập nhật lại thẻ...")
        subprocess.run(["git", "tag", "-f", tag], check=True)
    else:
        subprocess.run(["git", "tag", "-a", tag, "-m", f"Bản phát hành chính thức {tag}"], check=True)
    print(f"[+] Đã tạo / cập nhật git tag {tag}")

    if push:
        print(f"[*] Đang đẩy nhánh main và thẻ tag {tag} lên GitHub...")
        subprocess.run(["git", "push", "origin", "main"], check=True)
        subprocess.run(["git", "push", "origin", tag, "--force"], check=True)
        print(f"[+] ĐÃ ĐẨY LÊN GITHUB THÀNH CÔNG!")
        print(f"[*] GitHub Actions sẽ tự động tạo GitHub Release và kích hoạt kênh OTA.")
    else:
        print(f"\n[!] Để đẩy bản phát hành lên GitHub, bạn hãy chạy:")
        print(f"    git push origin main")
        print(f"    git push origin {tag} --force\n")


def main():
    parser = argparse.ArgumentParser(description="Quy trình đóng gói và phát hành firmware dpReader lên GitHub")
    parser.add_argument("--version", type=str, default="1.0.2-vn", help="Số phiên bản (vd: 1.0.2-vn)")
    parser.add_argument("--notes", type=str, default="", help="Ghi chú cập nhật cho phiên bản này")
    parser.add_argument("--push", action="store_true", help="Tự động đẩy commit và tag lên GitHub")
    parser.add_argument("--build", action="store_true", help="Biên dịch firmware trước khi phát hành")
    args = parser.parse_args()

    version = args.version.lstrip("v")
    notes = args.notes
    if not notes:
        notes = f"dpReader Metalio 4.7 v{version}: Hỗ trợ tiếng Việt Bitter font toàn hệ thống, tùy biến phím tắt và cử chỉ đọc sách, cải thiện ghosting E-ink, kênh OTA GitHub trực tiếp."

    print("=======================================================")
    print(f" QUY TRÌNH PHÁT HÀNH FIRMWARE dpReader Metalio 4.7")
    print(f" Phiên bản: v{version}")
    print("=======================================================")

    RELEASES_DIR.mkdir(parents=True, exist_ok=True)
    firmware_bin = RELEASES_DIR / "dpReader_metalio47_firmware.bin"
    full_bin = RELEASES_DIR / "dpReader_metalio47_full.bin"

    # Kiểm tra xem có cần build không
    if args.build:
        print("[*] Đang biên dịch firmware...")
        cmd = [
            sys.executable,
            "tools/build_firmware.py",
            "--board", "metalio47",
            "--output", "build/boards"
        ]
        subprocess.run(cmd, check=True)

    # Nếu có thư mục packages từ bản build, cập nhật file binary vào releases/
    packages_dir = BUILD_DIR / "packages"
    if (packages_dir / "firmware.bin").exists():
        print(f"[*] Cập nhật binaries từ {packages_dir.relative_to(REPO_ROOT)}...")
        firmware_bin.write_bytes((packages_dir / "firmware.bin").read_bytes())
        create_full_binary(packages_dir, full_bin)
    else:
        if not firmware_bin.exists() or not full_bin.exists():
            print(f"[!] Lỗi: Không tìm thấy file binary trong {packages_dir} và cũng chưa có trong {RELEASES_DIR}!")
            print(f"    Hãy chạy biên dịch firmware trước hoặc dùng cờ --build.")
            sys.exit(1)
        print(f"[*] Sử dụng file binary sẵn có trong thư mục releases/.")

    # 1. Tính mã băm checksums
    sha256_fw = update_checksums(firmware_bin, full_bin)
    size_fw = firmware_bin.stat().st_size

    # 2. Cập nhật manifest OTA
    update_manifests(version, sha256_fw, size_fw, notes)

    # 3. Lưu trữ bản sao snapshot theo phiên bản
    archive_version(version)

    # 4. Thực hiện git commit và tag
    run_git_commands(version, args.push)

    print("\n=======================================================")
    print(" QUY TRÌNH PHÁT HÀNH HOÀN TẤT THÀNH CÔNG!")
    print("=======================================================")
    print(f"- Firmware OTA: {firmware_bin.name} ({size_fw:,} bytes)")
    print(f"- Firmware Full: {full_bin.name} ({full_bin.stat().st_size:,} bytes)")
    print(f"- SHA256: {sha256_fw}")
    print(f"- Kênh OTA JSON: https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/metalio-s31-update.json")
    print(f"- Kênh OTA Pages: https://dephia31.github.io/dpReader31/metalio-s31-update.json")


if __name__ == "__main__":
    main()
