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
function updateFlashReadiness() {
  flashBtn.disabled = !esploader || isFlashing || !riskAccepted.checked || (!radioCustom.checked && !editionReady);
  riskAccepted.disabled = isFlashing;
  flashReadiness.textContent = isFlashing ? "Đang nạp, hãy giữ kết nối USB."
    : !esploader ? "Kết nối thiết bị và tích chấp nhận rủi ro để bắt đầu."
    : !radioCustom.checked && !editionReady ? "Chưa có gói firmware hợp lệ cho lựa chọn này."
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
function releaseSelectionChanged() {
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
  const latestVerName = document.getElementById("latest-version-name");
  const latestVerSize = document.getElementById("latest-version-size");
  const latestVerNotes = document.getElementById("latest-version-notes");
  const releaseTagApp = document.getElementById("release-tag-app");

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
    if (data && typeof data.version === "string" && /^\d+\.\d+\.\d+(?:-love)?-vn$/.test(data.version) && Number.isSafeInteger(data.size) && data.size > 0 && /^[a-f0-9]{64}$/i.test(data.sha256 || "") && ((data.version.includes("-love-vn")) === (editionSelect?.value === "love-book"))) {
      editionReady = true;
      currentReleaseInfo = data;
      if (latestVerName) latestVerName.textContent = `v${data.version}`;
      if (releaseTagApp) releaseTagApp.textContent = `Dành cho máy đã cài dpReader · v${data.version}`;
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
    updateFlashReadiness();
  } catch (err) {
    editionReady = false;
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

// Bắt đầu nạp Flash
flashBtn.addEventListener("click", async () => {
  if (!esploader) {
    alert("Vui lòng kết nối máy đọc sách trước khi nạp!");
    return;
  }

  if (isFlashing || (!radioCustom.checked && !editionReady)) return;
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

    if (radioApp.checked) {
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

    if (!radioCustom.checked) await verifySelectedFirmware(binaryData, radioFull.checked ? 0x10000 : 0, currentReleaseInfo, editionSelect?.value || "full");
    terminal.writeLine(`[+] Kích thước tệp: ${binaryData.byteLength.toLocaleString()} bytes`);
    terminal.writeLine(`[*] Vị trí ghi (Offset): 0x${offset.toString(16).toUpperCase()}`);
    terminal.writeLine("[*] Đang chuẩn bị dữ liệu và bắt đầu nạp...");

    const fileData = binaryData instanceof Uint8Array ? binaryData : new Uint8Array(binaryData);
    const fileArray = [{ data: fileData, address: offset }];

    // Xoá otadata (ghi 0xFF) để bootloader chọn ota_0 vừa nạp. Nếu otadata còn trỏ ota_1 từ một lần
    // cập nhật Wi-Fi trước đó, máy sẽ khởi động lại đúng bản cũ dù bản mới đã ghi xong.
    // Clear otadata (write 0xFF) so the bootloader picks the freshly written ota_0. If otadata still
    // points at ota_1 from an earlier Wi-Fi update, the device reboots into the old image.
    if (clearOtadata) {
      terminal.writeLine("[*] Xoá otadata (0xD000) để khởi động vào phân vùng vừa nạp (ota_0)...");
      fileArray.push({ data: new Uint8Array(0x2000).fill(0xff), address: 0xd000 });
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

    terminal.writeLine("=======================================================");
    terminal.writeLine("🎉 NẠP FIRMWARE THÀNH CÔNG RỰC RỠ!");
    terminal.writeLine("=======================================================");
    terminal.writeLine("[*] Đang khởi động lại thiết bị (xung reset EN qua RTS)...");

    progressBarFill.style.backgroundColor = "var(--success-color)";
    progressPercent.textContent = "100%";
    progressStatus.textContent = "Hoàn tất 100%! Thiết bị đang khởi động lại vào hệ điều hành mới.";

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
      alert("🎉 Nạp firmware thành công rực rỡ!\nThiết bị đang tự động khởi động lại.");
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
    document.querySelectorAll("input[name='flash-mode']").forEach(input => input.disabled = false);
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
