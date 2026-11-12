import { ESPLoader, Transport } from "./esptool-bundle.js";

// DOM Elements
const browserWarning = document.getElementById("browser-warning");
const connectBtn = document.getElementById("connect-btn");
const disconnectBtn = document.getElementById("disconnect-btn");
const flashBtn = document.getElementById("flash-btn");
const deviceInfo = document.getElementById("device-info");
const chipNameEl = document.getElementById("chip-name");
const macAddrEl = document.getElementById("mac-address");
const baudRateSelect = document.getElementById("baud-rate");
const progressContainer = document.getElementById("progress-container");
const progressBarFill = document.getElementById("progress-bar-fill");
const progressPercent = document.getElementById("progress-percent");
const progressStatus = document.getElementById("progress-status");
const terminalEl = document.getElementById("terminal");
const clearTerminalBtn = document.getElementById("clear-terminal-btn");
const radioApp = document.getElementById("mode-app");
const radioFull = document.getElementById("mode-full");
const radioCustom = document.getElementById("mode-custom");
const customFileContainer = document.getElementById("custom-file-container");
const customFileInput = document.getElementById("custom-file");
const customOffsetInput = document.getElementById("custom-offset");

let device = null;
let transport = null;
let esploader = null;
let isFlashing = false;
let editionReady = false;
const editionSelect = document.getElementById("firmware-edition");
const riskAccepted = document.getElementById("risk-accepted");
const flashReadiness = document.getElementById("flash-readiness");
function automaticLayoutSelected() {
  const version = document.getElementById("release-version")?.value ?? "";
  const parts = /^(\d+)\.(\d+)\.(\d+)(?:-love)?-vn$/.exec(version);
  return version === "" || Boolean(parts && (+parts[1] > 1 || (+parts[1] === 1 &&
    (+parts[2] > 4 || (+parts[2] === 4 && +parts[3] >= 2)))));
}
function syncInstallationGuide() {
  const automatic = automaticLayoutSelected();
  const version = document.getElementById("release-version")?.value ||
    (editionSelect?.value === "love-book" ? "1.4.5-love-vn" : "1.4.5-vn");
  const archivedTransition = /^1\.4\.2(?:-love)?-vn$/.test(version);
  document.getElementById("flash-modes").hidden = archivedTransition;
  document.getElementById("legacy-downloads").hidden = automatic;
  for (const id of ["nvs256-guide", "nvs256-download"]) document.getElementById(id).hidden = !automatic;
  document.getElementById("usb-only-guide").hidden = !archivedTransition;
  document.getElementById("mode-full-description").textContent = automatic
    ? "Ảnh 16 MB dành cho máy đang dùng 1.4.1 trở xuống, firmware khác hoặc cần cứu máy. Xóa dữ liệu trong máy, giữ thẻ SD."
    : "Cài đầy đủ phiên bản cũ trên máy có bố cục phân vùng tương thích.";
  document.getElementById("mode-app-description").textContent = automatic
    ? "Dành cho máy đã dùng dpReader 1.4.2 trở lên với NVS 256 KiB. Giữ cài đặt và dữ liệu."
    : "Chỉ nạp ứng dụng trên máy đang dùng bố cục phân vùng cũ tương thích.";
  if (automatic)
    document.getElementById("release-installer-download").href = `./${selectedReleaseFolder()}/dpReader-metalio47-${version}-USB.zip`;
}
function updateFlashReadiness() {
  flashBtn.disabled = !esploader || isFlashing || !riskAccepted.checked || ((automaticLayoutSelected() || !radioCustom.checked) && !editionReady);
  riskAccepted.disabled = isFlashing;
  flashReadiness.textContent = isFlashing ? "Đang nạp, hãy giữ kết nối USB."
    : !esploader ? "Kết nối thiết bị và tích chấp nhận rủi ro để bắt đầu."
    : (automaticLayoutSelected() || !radioCustom.checked) && !editionReady ? "Chưa có gói firmware hợp lệ cho lựa chọn này."
    : !riskAccepted.checked ? "Đã kết nối. Tích chấp nhận rủi ro để bật nút Bắt đầu nạp."
    : "Đã kết nối và chấp nhận rủi ro. Sẵn sàng bắt đầu nạp.";
}
function resetRiskAgreement() {
  riskAccepted.checked = false;
  updateFlashReadiness();
}
riskAccepted.addEventListener("change", updateFlashReadiness);
updateFlashReadiness();

// Kiểm tra Web Serial API
if (!("serial" in navigator)) {
  browserWarning.style.display = "block";
  document.getElementById("browser-ok").style.display = "none";
  connectBtn.disabled = true;
}

// Custom Terminal Logger cho esptool-js
const terminal = {
  clean() {
    terminalEl.textContent = "";
  },
  writeLine(data) {
    terminalEl.textContent += data + "\n";
    terminalEl.scrollTop = terminalEl.scrollHeight;
  },
  write(data) {
    terminalEl.textContent += data;
    terminalEl.scrollTop = terminalEl.scrollHeight;
  }
};

clearTerminalBtn.addEventListener("click", () => terminal.clean());

// Changing a write target invalidates the previous risk acknowledgement.
document.querySelectorAll("input[name='flash-mode']").forEach(input => {
  input.addEventListener("change", () => {
    document.querySelectorAll(".radio-card").forEach(card => {
      card.classList.toggle("active", card.querySelector("input[type='radio']").checked);
    });
    customFileContainer.style.display = radioCustom.checked ? "block" : "none";
    resetRiskAgreement();
  });
});
customFileInput.addEventListener("change", resetRiskAgreement);
customOffsetInput.addEventListener("input", resetRiskAgreement);

// Kết nối USB Serial
connectBtn.addEventListener("click", async () => {
  try {
    terminal.clean();
    terminal.writeLine("[*] Đang mở hộp thoại chọn cổng Serial...");
    device = await navigator.serial.requestPort({});
    terminal.writeLine("[+] Đã chọn cổng USB!");

    transport = new Transport(device, true);
    const baudRate = parseInt(baudRateSelect.value, 10);

    esploader = new ESPLoader({
      transport: transport,
      baudrate: baudRate,
      terminal: terminal,
      romBaudrate: 115200
    });

    terminal.writeLine("[*] Đang đồng bộ hóa với Bootloader thiết bị...");
    const chip = await esploader.main();
    terminal.writeLine(`[+] Kết nối thành công! Loại chip: ${chip}`);

    let mac = "Không xác định";
    try {
      mac = await esploader.chip.readMac(esploader);
    } catch (e) {
      console.warn("Không đọc được MAC:", e);
    }

    chipNameEl.textContent = chip || "ESP32-S31";
    macAddrEl.textContent = mac;
    deviceInfo.style.display = "grid";

    connectBtn.style.display = "none";
    disconnectBtn.style.display = "inline-flex";
    updateFlashReadiness();

  } catch (error) {
    terminal.writeLine(`[!] Lỗi kết nối: ${error.message || error}`);
    console.error(error);
    alert(`Không thể kết nối với thiết bị: ${error.message || error}\n\nHướng dẫn: Hãy thử giữ nút Nguồn + AI khoảng 3 giây rồi nhả ra để vào chế độ BOOT và kết nối lại.`);
  }
});

// Ngắt kết nối
disconnectBtn.addEventListener("click", async () => {
  if (isFlashing) {
    if (!confirm("Quá trình nạp đang diễn ra. Bạn có chắc chắn muốn ngắt kết nối không?")) return;
  }
  await disconnect();
});

async function disconnect() {
  try {
    if (transport) {
      await transport.disconnect();
    }
  } catch (e) {
    console.warn("Lỗi ngắt kết nối:", e);
  } finally {
    device = null;
    transport = null;
    esploader = null;
    isFlashing = false;
    deviceInfo.style.display = "none";
    connectBtn.style.display = "inline-flex";
    disconnectBtn.style.display = "none";
    resetRiskAgreement();
    progressContainer.style.display = "none";
    terminal.writeLine("[*] Đã ngắt kết nối thiết bị.");
  }
}

// Tự động kiểm tra và đồng bộ phiên bản mới nhất từ máy chủ
let currentReleaseInfo = null;

// 中文：只允许已归档版本入口，保持当前版与上一版可独立安装。
// English: Allow only archived versions so current and previous firmware install independently.
function selectedReleaseFolder() {
  const version = document.getElementById("release-version")?.value || "";
  const base = editionSelect?.value === "love-book" ? "releases/love-book" : "releases";
  return /^\d+\.\d+\.\d+(?:-love)?-vn$/.test(version) ? `${base}/${version}` : base;
}
const releaseVersionSelect = document.getElementById("release-version");
let previousEdition = editionSelect?.value || "full";
const releaseOptions = new Map();
function snapshotReleaseOptions() {
  return Array.from(releaseVersionSelect?.options || [], option => ({value: option.value, text: option.textContent}));
}
releaseOptions.set(previousEdition, snapshotReleaseOptions());
releaseOptions.set("love-book", [
  {value: "", text: "Mới nhất"},
  {value: "1.4.4-love-vn", text: "1.4.4-love-vn — bản chính thức trước"},
  {value: "1.4.3-love-vn", text: "1.4.3-love-vn"},
  {value: "1.4.2-love-vn", text: "1.4.2-love-vn"},
]);
function releaseSelectionChanged() {
  syncInstallationGuide();
  resetRiskAgreement();
  const folder = selectedReleaseFolder();
  document.getElementById("release-tf-download").href = `./${folder}/dpReader_metalio47_firmware.bin`;
  document.getElementById("release-usb-download").href = `./${folder}/dpReader_metalio47_full.bin`;
  editionReady = false;
  updateFlashReadiness();
  fetchLatestRelease();
}
releaseVersionSelect?.addEventListener("change", releaseSelectionChanged);
editionSelect?.addEventListener("change", () => {
  releaseOptions.set(previousEdition, snapshotReleaseOptions());
  previousEdition = editionSelect.value;
  if (releaseVersionSelect) {
    const options = releaseOptions.get(previousEdition) || [{value:"", text:"Mới nhất"}];
    releaseVersionSelect.replaceChildren(...options.map(item => {
      const option = document.createElement("option");
      option.value = item.value; option.textContent = item.text; return option;
    }));
    releaseVersionSelect.value = "";
  }
  releaseSelectionChanged();
});

async function fetchLatestRelease() {
  const requestedFolder = selectedReleaseFolder();
  const requestedVersion = document.getElementById("release-version")?.value || "";
  const latestVerName = document.getElementById("latest-version-name");
  const latestVerSize = document.getElementById("latest-version-size");
  const latestVerNotes = document.getElementById("latest-version-notes");

  try {
    let data = null;
    try {
      const res = await fetch(`./${requestedFolder}/metalio-s31-update.json?t=${Date.now()}`, { cache: "no-store" });
      if (res.ok) data = await res.json();
    } catch (_) {}

    if (!data || !data.version) {
      try {
        const rawRes = await fetch(`https://raw.githubusercontent.com/dephia31/dpReader31/main/${requestedFolder}/metalio-s31-update.json?t=${Date.now()}`, { cache: "no-store" });
        if (rawRes.ok) data = await rawRes.json();
      } catch (_) {}
    }

    if ((!data || !data.version) && editionSelect?.value !== "love-book") {
      try {
        const selectedVer = document.getElementById("release-version")?.value;
        const targetTag = selectedReleaseFolder() === "releases" ? "latest" : `tags/v${selectedVer}`;
        const ghRes = await fetch(`https://api.github.com/repos/dephia31/dpReader31/releases/${targetTag}`);
        if (ghRes.ok) {
          const ghData = await ghRes.json();
          data = {
            version: (ghData.tag_name || "").replace(/^v/, ""),
            notes: ghData.body || "Bản phát hành mới nhất từ GitHub Releases.",
            size: ghData.assets && ghData.assets[0] ? ghData.assets[0].size : 0,
          };
        }
      } catch (_) {}
    }

    if (requestedFolder !== selectedReleaseFolder()) return;
    if (data && (!requestedVersion || data.version === requestedVersion) && typeof data.version === "string" && /^\d+\.\d+\.\d+(?:-love)?-vn$/.test(data.version) && Number.isSafeInteger(data.size) && data.size > 0 && /^[a-f0-9]{64}$/i.test(data.sha256 || "") && ((data.version.includes("-love-vn")) === (editionSelect?.value === "love-book"))) {
      if (automaticLayoutSelected() && !data.usb_parts) throw new Error("Thiếu thông tin cài đặt NVS 256 KiB");
      editionReady = true;
      currentReleaseInfo = data;
      if (latestVerName) latestVerName.textContent = `v${data.version}`;
      if (latestVerNotes && data.notes) {
        latestVerNotes.textContent = data.notes.split("\n")[0] || data.notes;
      }
      if (latestVerSize && data.size) {
        latestVerSize.textContent = `${(data.size / 1024 / 1024).toFixed(2)} MB`;
      }

      // Tự động đồng bộ và chọn đúng bản mới nhất trong dropdown
      if ((requestedFolder === "releases" || requestedFolder === "releases/love-book") && releaseVersionSelect) {
        const firstOpt = releaseVersionSelect.options[0];
        if (firstOpt && firstOpt.value === "") {
          firstOpt.textContent = `${data.version} — mới nhất`;
        }
        if (data.previous_version) {
          if (releaseVersionSelect.options.length < 2) releaseVersionSelect.appendChild(document.createElement("option"));
          releaseVersionSelect.options[1].value = data.previous_version;
          releaseVersionSelect.options[1].textContent = `${data.previous_version} — bản chính thức trước`;
        }
        // Đảm bảo option đầu tiên (mới nhất) được chọn khi vào trang
        if (!releaseVersionSelect.value) {
          releaseVersionSelect.selectedIndex = 0;
        }
      }
    } else {
      editionReady = false;
      currentReleaseInfo = null;
      if (latestVerName) latestVerName.textContent = "Chưa phát hành";
      if (latestVerSize) latestVerSize.textContent = "";
      if (latestVerNotes) latestVerNotes.textContent = "Chưa có gói cho phiên bản đã chọn.";
    }
    for (const id of ["release-tf-download", "release-usb-download"]) {
      const link = document.getElementById(id);
      link.hidden = !editionReady;
    }
    document.getElementById("release-installer-download").hidden = !editionReady || !automaticLayoutSelected();
    updateFlashReadiness();
  } catch (err) {
    if (requestedFolder !== selectedReleaseFolder()) return;
    editionReady = false;
    currentReleaseInfo = null;
    for (const id of ["release-tf-download", "release-usb-download", "release-installer-download"])
      document.getElementById(id).hidden = true;
    updateFlashReadiness();
    console.warn("Không thể tải thông tin release mới nhất:", err);
  }
}

fetchLatestRelease();

function sleepMs(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// Reset chip sau khi nạp. Hai lỗi riêng biệt gây ra "nạp xong không tự khởi động lại":
// 1) esptool-js KHÔNG có ESPLoader.hardReset() -> gọi hàm không tồn tại, TypeError bị nuốt vào catch.
// 2) HardReset của esptool-js chỉ nhả RTS mà không kéo xuống trước, nên khi RTS vốn đã ở mức nhả
//    (trạng thái USB-Serial/JTAG sau khi kết nối) thì không có xung EN nào: chip nằm lại trong stub.
// Phải kéo RTS xuống (EN thấp), giữ một nhịp, rồi để esptool-js nhả ra theo đúng thứ tự của chip.
// Two separate bugs cause "flashes fine but never reboots":
// 1) esptool-js has NO ESPLoader.hardReset() -> the call throws TypeError, swallowed by catch.
// 2) Its HardReset only releases RTS without asserting it first, so when RTS is already released
//    (USB-Serial/JTAG after connecting) no EN pulse happens and the chip stays in the flasher stub.
// Assert RTS (EN low), hold, then let esptool-js release it in the chip-specific order.
async function resetIntoApp() {
  // DTR cao = IO0 cao, để chip khởi động ứng dụng thay vì quay lại download mode.
  // DTR high keeps IO0 high so the chip boots the app instead of re-entering download mode.
  try {
    await transport.setDTR(false);
  } catch (e) {
    console.warn("Không đặt được DTR:", e);
  }

  // Xung EN giống esptool bản Python khi chip chạy trên USB nội bộ: kéo RTS xuống 200 ms rồi nhả,
  // chờ 800 ms cho thiết bị liệt kê lại trên USB.
  // Same EN pulse as the Python esptool on chips using the internal USB peripheral: assert RTS for
  // 200 ms, release, then wait 800 ms for the device to re-enumerate on USB.
  try {
    await transport.setRTS(true);
    await sleepMs(200);
    await transport.setRTS(false);
    await sleepMs(800);
    return true;
  } catch (e) {
    terminal.writeLine(`[!] Xung reset qua RTS lỗi: ${e.message || e}`);
    console.warn("Reset pulse failed:", e);
  }

  // Dự phòng: để esptool-js nhả theo thứ tự riêng của chip đã nhận diện.
  // Fallback: let esptool-js release in the detected chip's own order.
  try {
    await esploader.after("hard_reset");
    await sleepMs(800);
    return true;
  } catch (e) {
    console.warn("Fallback reset failed:", e);
    return false;
  }
}

// Sau reset, thiết bị USB-Serial/JTAG liệt kê lại nên phải nhả cổng; giữ nguyên log và thanh tiến trình.
// The device re-enumerates on USB-Serial/JTAG after reset, so release the port while keeping log and progress.
async function releasePortAfterFlash() {
  try {
    if (transport) await transport.disconnect();
  } catch (e) {
    console.warn("Không đóng được cổng sau khi nạp:", e);
  }
  transport = null;
  esploader = null;
  device = null;
  connectBtn.style.display = "inline-flex";
  disconnectBtn.style.display = "none";
  disconnectBtn.disabled = false;
  flashBtn.disabled = true;
  deviceInfo.style.display = "none";
}

// Convert ArrayBuffer sang Binary String cho esptool-js
function arrayBufferToBinaryString(buffer) {
  const bytes = new Uint8Array(buffer);
  const chunkSize = 0x8000;
  let binary = "";
  for (let i = 0; i < bytes.length; i += chunkSize) {
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunkSize));
  }
  return binary;
}

// Tải file firmware từ đường dẫn URL (kèm fallback GitHub raw)
async function fetchBinary(primaryUrl, fallbackUrl) {
  terminal.writeLine(`[*] Đang tải firmware từ: ${primaryUrl}`);
  try {
    const res = await fetch(primaryUrl);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.arrayBuffer();
  } catch (err) {
    terminal.writeLine(`[!] Tải từ nguồn chính thất bại. Thử nguồn dự phòng: ${fallbackUrl}`);
    const res2 = await fetch(fallbackUrl);
    if (!res2.ok) throw new Error(`HTTP ${res2.status}`);
    return await res2.arrayBuffer();
  }
}

// 核对实际应用头和哈希，防止选择正确但下载了另一分支。/ Verify actual app identity/hash before any writes.
async function verifySelectedFirmware(buffer, appOffset, release, edition) {
  if (!release || !/^\d+\.\d+\.\d+(?:-love)?-vn$/.test(release.version)) throw new Error("Web Flash chỉ nhận bản chính thức, không nạp RC.");
  const bytes = new Uint8Array(buffer);
  const app = bytes.subarray(appOffset, appOffset + release.size);
  const view = new DataView(app.buffer, app.byteOffset, app.byteLength);
  const field = offset => new TextDecoder().decode(app.subarray(offset, offset + 32)).split("\0")[0];
  if (app.length !== release.size || app.length < 288 || app[0] !== 0xe9 ||
      view.getUint16(12, true) !== 32 || view.getUint32(32, true) !== 0xabcd5432 ||
      field(48) !== release.version || field(80) !== "Kiikoread_Metalio_S31" ||
      field(48).includes("-love-vn") !== (edition === "love-book")) {
    throw new Error("Firmware không khớp bản Full/Love Book, phiên bản hoặc chip đã chọn.");
  }
  const digest = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", app)), value => value.toString(16).padStart(2, "0")).join("");
  if (digest !== release.sha256.toLowerCase()) throw new Error("SHA-256 firmware không khớp. Đã dừng trước khi ghi.");
}

// Validate exact partition geometry before any write, including archived releases.
function classifyS31Partitions(raw) {
  const data = new Uint8Array(raw);
  if (data.length !== 0x1000) throw new Error("Không đọc đủ bảng phân vùng.");
  if (data.every(value => value === 255)) return "blank";
  const table = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const found = new Map();
  let offset = 0;
  for (; offset < data.length && table.getUint16(offset, true) === 0x50aa; offset += 32) {
    const label = String.fromCharCode(...data.subarray(offset + 12, offset + 28)).split("\0")[0];
    if (found.has(label) || table.getUint32(offset + 28, true) !== 0) return "unknown";
    found.set(label, [data[offset+2], data[offset+3], table.getUint32(offset+4,true), table.getUint32(offset+8,true)].join(","));
  }
  if (offset + 32 <= data.length && table.getUint16(offset,true) === 0xebeb) offset += 32;
  if (!data.subarray(offset).every(value => value === 255)) return "unknown";
  const common = {otadata:[1,0,0xd000,0x2000], phy_init:[1,1,0xf000,0x1000],
    ota_0:[0,16,0x10000,0x500000], ota_1:[0,17,0x510000,0x500000], coredump:[1,3,0xff0000,0x10000]};
  if (Object.entries(common).some(([name, shape]) => found.get(name) !== shape.join(","))) return "unknown";
  const legacyNvs = [1,2,0x9000,0x4000].join(",");
  if (found.size === 7 && found.get("nvs") === legacyNvs && found.get("storage") === [1,129,0xa10000,0x5e0000].join(",")) return "legacy";
  if (found.get("nvs") === [1,2,0xfb0000,0x40000].join(",") && found.get("storage") === [1,129,0xa10000,0x5a0000].join(",")) {
    if (found.size === 7) return "new";
    if (found.size === 8 && found.get("nvs_old") === legacyNvs) return "rc";
  }
  return "unknown";
}
async function fetchVerifiedPart(name, part) {
  if (!part || !/^[a-f0-9]{32}$/i.test(part.md5 || "") || !Number.isSafeInteger(part.size) || part.size <= 0 || !/^[a-f0-9]{64}$/i.test(part.sha256))
    throw new Error(`Thiếu thông tin xác minh ${name}.`);
  const folder = selectedReleaseFolder();
  const bytes = new Uint8Array(await fetchBinary(`./${folder}/${name}`, `https://raw.githubusercontent.com/dephia31/dpReader31/main/${folder}/${name}`));
  const sha = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), n => n.toString(16).padStart(2,"0")).join("");
  if (bytes.length !== part.size || sha !== part.sha256.toLowerCase()) throw new Error(`Sai checksum ${name}.`);
  return bytes;
}
async function verifyFlashRegion({data,address,md5}) {
  if (!/^[a-f0-9]{32}$/i.test(md5 || "") ||
      (await esploader.flashMd5sum(address, data.length)).toLowerCase() !== md5.toLowerCase())
    throw new Error(`Kiểm tra lại sau khi ghi thất bại tại 0x${address.toString(16)}.`);
}
// Match the entire sector against verified dpReader and CrossPoint tables.
// The stub returns a small checksum response instead of streaming flash data.
const S31_PARTITION_LAYOUTS = new Map([
  ["6ae59e64850377ee5470c854761551ea", "blank"],
  ["c2c89587cec31dfd42c20bf837b3eb29", "crosspoint"],
  ["9f56f6078cbd6aac217fb84f364246de", "legacy"],
  ["c8d9169209a600d053b83659b33e0bb2", "rc"],
  ["98fd72a61b5952a549dd2a6f48b7c6fa", "new"],
]);
async function probeS31Layout() {
  terminal.writeLine("[*] Đang kiểm tra bảng phân vùng trên thiết bị...");
  let checksum;
  try {
    checksum = (await esploader.flashMd5sum(0x8000, 0x1000)).toLowerCase();
  } catch (error) {
    throw new Error(`Không đọc được checksum bảng phân vùng (${error.message || error}). Kết nối lại ở 115200 baud hoặc dùng bộ cài USB.`);
  }
  terminal.writeLine(`[*] Checksum bảng phân vùng: ${checksum}`);
  return S31_PARTITION_LAYOUTS.get(checksum) || "unknown";
}
// Digests of the 16 MiB images assembled from the official 1.4.2 USB components.
const CLEAN_IMAGE_DIGESTS = new Map([
  ["1.4.2-vn", {sha256:"9a72c8876c144c7b516e0441aee9ec5e68a92e0af880d6b86d948c71bade8f4f", md5:"931de619005c692837839cb3de85fb98"}],
  ["1.4.2-love-vn", {sha256:"42b805d93ce4b9e78486afbe8ead7392b6c888021cf3c6b9a2429e7c0d26d4ea", md5:"ab7f46d09ee6e2f35c2891e26a59624a"}],
]);
async function prepareNvs256Install(release) {
  if (esploader.chip.CHIP_NAME !== "ESP32-S31" || !esploader.IS_STUB || await esploader.detectFlashSize() !== "16MB")
    throw new Error("Gói này chỉ dành cho Metalio 4.7 ESP32-S31 16 MB.");
  const fullInstall = release.usb_only === true || radioFull.checked;
  if (!fullInstall && await probeS31Layout() !== "new")
    throw new Error("Máy chưa có bố cục NVS 256 KiB. Hãy chọn ô Nạp toàn phần.");
  if (fullInstall && !confirm("Nạp toàn phần dpReader sẽ ghi lại toàn bộ 16 MB và xóa firmware cùng dữ liệu trong máy. Thẻ SD giữ nguyên. Bạn muốn tiếp tục?")) return null;

  const app = await fetchVerifiedPart(release.usb_only ? "web-firmware.bin" : "dpReader_metalio47_firmware.bin", release);
  await verifySelectedFirmware(app, 0, release, editionSelect?.value || "full");
  if (!fullInstall) {
    terminal.writeLine("[*] Đã nhận diện NVS 256 KiB. Chỉ nạp ứng dụng và giữ cài đặt.");
    return {data:app, address:0x10000, md5:release.md5};
  }
  const storage = await fetchVerifiedPart("storage-empty.bin", release.usb_parts?.["storage-empty.bin"]);
  const partitions = await fetchVerifiedPart("partitions.bin", release.usb_parts?.["partitions.bin"]);
  const boot = await fetchVerifiedPart("bootloader.bin", release.usb_parts?.["bootloader.bin"]);
  const digests = release.usb_only ? CLEAN_IMAGE_DIGESTS.get(release.version) : release.clean_image;
  const table = new Uint8Array(0x1000).fill(255);
  if (partitions.length > table.length || storage.length !== 0x5a0000) throw new Error("Sai kích thước phân vùng.");
  table.set(partitions);
  if (!/^[a-f0-9]{32}$/.test(digests?.md5 || "") || classifyS31Partitions(table) !== "new" || boot.length > 0x6000 ||
      boot[0] !== 0xe9 || boot[12] !== 32 || boot[13] !== 0 || app.length > 0x500000)
    throw new Error("Các thành phần của ảnh cài toàn phần không hợp lệ.");

  const data = new Uint8Array(0x1000000).fill(255);
  data.set(boot, 0x2000);
  data.set(partitions, 0x8000);
  data.set(app, 0x10000);
  data.set(storage, 0xa10000);
  const sha256 = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", data)), n => n.toString(16).padStart(2, "0")).join("");
  if (sha256 !== digests.sha256) throw new Error("Sai checksum ảnh cài toàn phần.");
  terminal.writeLine("[*] Nạp một ảnh cài sạch 16 MB từ 0x0.");
  return {data, address:0, md5:digests.md5};
}

// Bắt đầu nạp Flash
flashBtn.addEventListener("click", async () => {
  if (!esploader) {
    alert("Vui lòng kết nối máy đọc sách trước khi nạp!");
    return;
  }

  if (isFlashing || ((automaticLayoutSelected() || !radioCustom.checked) && !editionReady)) return;
  if (!riskAccepted.checked) {
    updateFlashReadiness();
    riskAccepted.focus();
    return;
  }
  try {
    isFlashing = true;
    updateFlashReadiness();
    document.querySelectorAll("input[name='flash-mode']").forEach(input => input.disabled = true);
    customFileInput.disabled = true;
    customOffsetInput.disabled = true;
    if (releaseVersionSelect) releaseVersionSelect.disabled = true;
    if (editionSelect) editionSelect.disabled = true;
    flashBtn.disabled = true;
    disconnectBtn.disabled = true;
    progressContainer.style.display = "block";
    progressBarFill.style.width = "0%";
    progressBarFill.style.backgroundColor = "var(--accent-color)";
    progressPercent.textContent = "0%";
    progressStatus.textContent = "Đang chuẩn bị tệp firmware...";

    let binaryData = null;
    let offset = 0x10000;
    let fileName = "";
    // Chế độ "Ứng dụng" phải đưa otadata về trạng thái trống để bootloader chọn ota_0 vừa ghi.
    // Nếu máy từng cập nhật OTA qua Wi-Fi, otadata có thể đang trỏ ota_1 và thiết bị sẽ khởi động lại
    // đúng bản cũ dù đã ghi xong bản mới vào ota_0.
    // App-only mode must clear otadata so the bootloader selects the freshly written ota_0.
    // A device that OTA'd over Wi-Fi may have otadata pointing at ota_1 and would boot the old image
    // even though the new one was written to ota_0.
    let clearOtadata = false;
    const timestamp = Date.now();

    let nvsImage = null;
    if (automaticLayoutSelected()) {
      if (!currentReleaseInfo?.usb_parts) throw new Error("Chưa có gói cài USB đã xác minh.");
      nvsImage = await prepareNvs256Install(currentReleaseInfo);
      if (!nvsImage) { progressStatus.textContent = "Đã hủy cài sạch."; return; }
      binaryData = nvsImage.data;
      offset = nvsImage.address;
      clearOtadata = offset === 0x10000;
    } else if (radioApp.checked) {
      fileName = "dpReader_metalio47_firmware.bin";
      offset = 0x10000;
      clearOtadata = true;
      progressStatus.textContent = "Đang tải firmware ứng dụng mới nhất...";
      binaryData = await fetchBinary(
        `./${selectedReleaseFolder()}/${fileName}?t=${timestamp}`,
        `https://raw.githubusercontent.com/dephia31/dpReader31/main/${selectedReleaseFolder()}/${fileName}?t=${timestamp}`
      );
    } else if (radioFull.checked) {
      fileName = "dpReader_metalio47_full.bin";
      offset = 0x0;
      progressStatus.textContent = "Đang tải firmware toàn diện (Full Flash)...";
      binaryData = await fetchBinary(
        `./${selectedReleaseFolder()}/${fileName}?t=${timestamp}`,
        `https://raw.githubusercontent.com/dephia31/dpReader31/main/${selectedReleaseFolder()}/${fileName}?t=${timestamp}`
      );
    } else if (radioCustom.checked) {
      const file = customFileInput.files[0];
      if (!file) {
        alert("Vui lòng chọn một tệp .bin từ máy tính của bạn!");
        isFlashing = false;
        updateFlashReadiness();
        disconnectBtn.disabled = false;
        return;
      }
      fileName = file.name;
      offset = parseInt(customOffsetInput.value, 16) || 0x10000;
      // 自选应用与默认应用共用启动槽处理；其他偏移不改OTA选择。
      // Custom apps share the default app's boot-slot handling; other offsets retain OTA selection.
      clearOtadata = offset === 0x10000;
      progressStatus.textContent = `Đang đọc tệp ${fileName}...`;
      binaryData = await file.arrayBuffer();
    }

    if (!nvsImage && !radioCustom.checked) await verifySelectedFirmware(binaryData, radioFull.checked ? 0x10000 : 0, currentReleaseInfo, editionSelect?.value || "full");
    if (!nvsImage) {
      if (esploader.chip.CHIP_NAME !== "ESP32-S31") throw new Error("Sai dòng thiết bị.");
      const layout = await probeS31Layout();
      if (layout !== "legacy" && !(layout === "blank" && radioFull.checked && !radioCustom.checked))
        throw new Error("Bản cũ chỉ cài trên bố cục cũ. Chọn bản mới nhất để cập nhật máy dùng NVS 256 KiB.");
      if (radioCustom.checked) throw new Error("Chọn gói chính thức để xác minh bố cục và phiên bản.");
    }
    const payloadSize = binaryData.byteLength;
    terminal.writeLine(`[+] Kích thước tệp: ${payloadSize.toLocaleString()} bytes`);
    terminal.writeLine(`[*] Vị trí ghi (Offset): 0x${offset.toString(16).toUpperCase()}`);
    terminal.writeLine("[*] Đang chuẩn bị dữ liệu và bắt đầu nạp...");

    const fileData = binaryData instanceof Uint8Array ? binaryData : new Uint8Array(binaryData);
    const fileArray = [nvsImage ?? { data: fileData, address: offset, md5: currentReleaseInfo?.md5 }];

    // Xoá otadata (ghi 0xFF) để bootloader chọn ota_0 vừa nạp. Nếu otadata còn trỏ ota_1 từ một lần
    // cập nhật Wi-Fi trước đó, máy sẽ khởi động lại đúng bản cũ dù bản mới đã ghi xong.
    // Clear otadata (write 0xFF) so the bootloader picks the freshly written ota_0. If otadata still
    // points at ota_1 from an earlier Wi-Fi update, the device reboots into the old image.
    if (clearOtadata) {
      terminal.writeLine("[*] Xoá otadata (0xD000) để khởi động vào phân vùng vừa nạp (ota_0)...");
      fileArray.push({ data: new Uint8Array(0x2000).fill(0xff), address: 0xd000, md5: "84d04c9d6cc8ef35bf825d51a5277699" });
    }

    const startTime = Date.now();

    await esploader.writeFlash({
      fileArray: fileArray,
      flashSize: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const percent = Math.floor((written / total) * 100);
        const elapsedSec = (Date.now() - startTime) / 1000;
        const speedKB = elapsedSec > 0 ? (written / 1024 / elapsedSec).toFixed(1) : 0;
        
        progressBarFill.style.width = `${percent}%`;
        progressPercent.textContent = `${percent}%`;
        progressStatus.textContent = `Đang nạp: ${(written / 1024 / 1024).toFixed(2)} MB / ${(total / 1024 / 1024).toFixed(2)} MB (${speedKB} KB/s)`;
      }
    });

    if (nvsImage) {
      progressStatus.textContent = "Đang xác minh dữ liệu đã ghi...";
      for (const region of fileArray) await verifyFlashRegion(region);
    }
    terminal.writeLine("=======================================================");
    terminal.writeLine("🎉 NẠP FIRMWARE THÀNH CÔNG RỰC RỠ!");
    terminal.writeLine("=======================================================");
    terminal.writeLine("[*] Đang khởi động lại thiết bị (xung reset EN qua RTS)...");

    progressBarFill.style.backgroundColor = "var(--success-color)";
    progressPercent.textContent = "100%";
    progressStatus.textContent = "Đã ghi xong. Kiểm tra phiên bản sau khi thiết bị khởi động.";

    let resetOk = false;
    try {
      resetOk = await resetIntoApp();
      if (resetOk) terminal.writeLine("[+] Đã phát xung reset, thiết bị đang khởi động lại.");
    } catch (e) {
      terminal.writeLine(`[!] Không phát được xung reset: ${e.message || e}`);
      console.warn("Reset note:", e);
    }

    // Nhả cổng để thiết bị USB-Serial/JTAG liệt kê lại sau khi reset.
    await releasePortAfterFlash();

    if (resetOk) {
      alert("Đã ghi firmware. Kiểm tra phiên bản sau khi máy khởi động. Nếu máy còn ở BOOT, rút cáp và nhấn giữ nút Nguồn để khởi động lại.");
    } else {
      alert("🎉 Nạp firmware thành công!\n\nKhông tự reset được thiết bị. Hãy rút cáp USB và nhấn giữ nút Nguồn khoảng 3 giây để khởi động lại.");
    }

  } catch (err) {
    terminal.writeLine(`[!] LỖI TRONG QUÁ TRÌNH NẠP: ${err.message || err}`);
    console.error(err);
    progressBarFill.style.backgroundColor = "var(--danger-color)";
    progressStatus.textContent = `Lỗi: ${err.message || err}`;
    alert(`Nạp thất bại: ${err.message || err}`);
  } finally {
    isFlashing = false;
    document.querySelectorAll("input[name='flash-mode']").forEach(input => input.disabled = input === radioCustom);
    customFileInput.disabled = false;
    customOffsetInput.disabled = false;
    resetRiskAgreement();
    if (releaseVersionSelect) releaseVersionSelect.disabled = false;
    if (editionSelect) editionSelect.disabled = false;
    // Sau khi nạp thành công cổng đã được nhả; chỉ bật lại nút khi còn kết nối.
    // After a successful flash the port is released; only re-enable the button while still connected.
    if (esploader) {
      updateFlashReadiness();
      disconnectBtn.disabled = false;
    }
  }
});

syncInstallationGuide();
