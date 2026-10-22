#!/usr/bin/env python3
"""只读官网 OTA 巡检，保留旧固件的信任链限制。/ Read-only OTA checks with legacy trust constraints."""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec, ed25519, ed448, padding, rsa

ORIGIN = "https://kiikoread.com"
# 原 rc96/97/98 的同一信任包；从正式 ELF/应用交叉提取并校验。/ Same verified bundle in rc96/97/98.
BASE_SHA = "163ef3e8b4138fc6b9641aef2f84b112b5b025cf0929a1554cbaa04bde785fca"
BUNDLE_SHA = "b3e27ad072b398df1404c73493e6441ef4929e523f944cc37423e0e4b86c6fb0"
BUNDLE_OFFSET, BUNDLE_SIZE = 2271648, 67817


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def bundle_keys(data: bytes) -> dict[bytes, bytes]:
    first, = struct.unpack_from("<I", data)
    assert 0 < first < len(data) and first % 4 == 0, "invalid bundle header"
    offsets = struct.unpack_from(f"<{first // 4}I", data)
    result = {}
    previous = b""
    for i, offset in enumerate(offsets):
        assert first <= offset <= len(data) - 4, "invalid bundle offset"
        n, k = struct.unpack_from("<HH", data, offset)
        end = offset + 4 + n + k
        assert n and k and end == (offsets[i + 1] if i + 1 < len(offsets) else len(data)), "invalid bundle bounds"
        name, key = data[offset + 4:offset + 4 + n], data[offset + 4 + n:end]
        assert name > previous, "unsorted/duplicate bundle subject"
        serialization.load_der_public_key(key)
        result[name], previous = key, name
    return result


def verify_signature(cert: x509.Certificate, key) -> None:
    if isinstance(key, rsa.RSAPublicKey):
        key.verify(cert.signature, cert.tbs_certificate_bytes,
                   cert.signature_algorithm_parameters or padding.PKCS1v15(), cert.signature_hash_algorithm)
    elif isinstance(key, ec.EllipticCurvePublicKey):
        key.verify(cert.signature, cert.tbs_certificate_bytes, ec.ECDSA(cert.signature_hash_algorithm))
    elif isinstance(key, (ed25519.Ed25519PublicKey, ed448.Ed448PublicKey)):
        key.verify(cert.signature, cert.tbs_certificate_bytes)
    else:
        raise ValueError("unsupported certificate public key")


def verify_legacy_chain(pem: bytes, keys: dict[bytes, bytes], now=None) -> list[dict]:
    chain = x509.load_pem_x509_certificates(pem)
    assert chain, "server did not expose its certificate chain"
    now = now or dt.datetime.now(dt.timezone.utc)
    names = []
    for i, cert in enumerate(chain):
        assert cert.not_valid_before_utc <= now <= cert.not_valid_after_utc, "certificate outside validity window"
        assert (cert.not_valid_after_utc - now).days >= 14, "certificate expires in less than 14 days"
        names.append({"subject": cert.subject.rfc4514_string(), "issuer": cert.issuer.rfc4514_string(),
                      "expires": cert.not_valid_after_utc.isoformat()})
        if i:
            assert cert.extensions.get_extension_for_class(x509.BasicConstraints).value.ca, "non-CA in chain"
        if i + 1 < len(chain):
            parent = chain[i + 1]
            assert cert.issuer.public_bytes() == parent.subject.public_bytes(), "server chain is not ordered"
            verify_signature(cert, parent.public_key())
    san = chain[0].extensions.get_extension_for_class(x509.SubjectAlternativeName).value
    assert "kiikoread.com" in [n.lower() for n in san.get_values_for_type(x509.DNSName)], "official apex missing from certificate SAN"
    # 无 cross-signed verify 的旧客户端查找末端 issuer，不会重建浏览器的备用链。
    # Legacy clients without cross-signed verification look up the terminal issuer, not an alternate browser path.
    terminal = chain[-1]
    key = keys.get(terminal.issuer.public_bytes())
    assert key is not None, "legacy firmware lacks terminal issuer: " + terminal.issuer.rfc4514_string()
    verify_signature(terminal, serialization.load_der_public_key(key))
    return names


def fetch(url: str, folder: Path, label: str, byte_range=None):
    assert url.startswith(ORIGIN + "/") and "@" not in url, "OTA must stay on the official origin"
    body, headers = folder / (label + ".bin"), folder / (label + ".headers")
    command = ["curl", "--silent", "--show-error", "--fail", "--http1.1", "--tlsv1.2", "--tls-max", "1.2",
               "--connect-timeout", "15", "--max-time", "90", "--proto", "=https",
               "-H", "User-Agent: PICO_HTTPS_OTA_V1", "-H", "Accept-Encoding: identity",
               "-H", "Cache-Control: no-cache", "-D", str(headers), "-o", str(body), "-w", "%{certs}"]
    if byte_range:
        command += ["--range", byte_range]
    proc = subprocess.run(command + [url], capture_output=True, check=False, timeout=95)
    assert proc.returncode == 0, f"{label}: HTTPS failed: {proc.stderr.decode(errors='replace')[:300]}"
    raw = headers.read_text()
    assert re.search(r"^HTTP/\S+ (200|206)\b", raw, re.M), label + ": unexpected status"
    return body.read_bytes(), raw, proc.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trust-image", type=Path, help="original rc97 application, SHA checked")
    parser.add_argument("--output", type=Path, default=Path("build/ota-health.json"))
    args = parser.parse_args()
    report = {"checked_at": dt.datetime.now(dt.timezone.utc).isoformat(), "origin": ORIGIN, "ok": False}
    try:
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            if args.trust_image:
                original = args.trust_image.read_bytes()
            else:
                original, _, _ = fetch(ORIGIN + "/Pico-update-0.3.3-rc97.bin", folder, "rc97")
            assert sha(original) == BASE_SHA, "original rc97 trust image SHA mismatch"
            bundle = original[BUNDLE_OFFSET:BUNDLE_OFFSET + BUNDLE_SIZE]
            assert sha(bundle) == BUNDLE_SHA, "original trust bundle SHA mismatch"
            keys = bundle_keys(bundle)
            payload, _, chain = fetch(ORIGIN + "/update.json", folder, "feed")
            feed = json.loads(payload)
            assert feed["schema"] == 1 and feed["project"] == "Read_Pico" and feed["board"] == "RDP-G01-W"
            assert feed["layout"] == "pico-dual-4m-v1" and 0 < feed["size"] <= 0x400000
            assert feed["url"] == ORIGIN + "/Pico-update-" + feed["version"] + ".bin"
            image, _, image_chain = fetch(feed["url"], folder, "image")
            assert len(image) == feed["size"] and sha(image) == feed["sha256"], "application size/SHA mismatch"
            assert image[48:80].split(b"\0")[0].decode() == feed["version"], "embedded app version mismatch"
            assert image[80:112].split(b"\0")[0] == b"Read_Pico", "embedded project mismatch"
            start = len(image) - min(len(image), 65536)
            partial, headers, range_chain = fetch(feed["url"], folder, "range", str(start) + "-")
            assert re.search(r"^HTTP/\S+ 206\b", headers, re.M), "Range request not honored"
            assert f"bytes {start}-{len(image)-1}/{len(image)}" in headers and partial == image[start:], "resume range differs"
            report.update(version=feed["version"], size=len(image), sha256=sha(image), range_ok=True, trust_roots=len(keys))
            report["server_chain"] = [{"subject": c.subject.rfc4514_string(), "issuer": c.issuer.rfc4514_string(),
                                       "sha256": sha(c.public_bytes(serialization.Encoding.DER)),
                                       "expires": c.not_valid_after_utc.isoformat()}
                                      for c in x509.load_pem_x509_certificates(chain)]
            report["chain"] = verify_legacy_chain(chain, keys)
            verify_legacy_chain(image_chain, keys)
            verify_legacy_chain(range_chain, keys)
            report["ok"] = True
    except Exception as exc:
        report["error"] = str(exc)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report["ok"] else 1)


if __name__ == "__main__":
    main()
