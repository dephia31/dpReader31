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

// Kiểm tra Web Serial API
if (!("serial" in navigator)) {
  browserWarning.style.display = "block";
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

// Chuyển đổi Radio Cards
document.querySelectorAll(".radio-card").forEach(card => {
  card.addEventListener("click", (e) => {
    if (isFlashing) return;
    const input = card.querySelector("input[type='radio']");
    if (input) {
      input.checked = true;
      document.querySelectorAll(".radio-card").forEach(c => c.classList.remove("active"));
      card.classList.add("active");
      
      if (input.id === "mode-custom") {
        customFileContainer.style.display = "block";
      } else {
        customFileContainer.style.display = "none";
      }
    }
  });
});

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
    flashBtn.disabled = false;

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
    flashBtn.disabled = true;
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
  return version === "1.1.6-vn" ? `releases/${version}` : "releases";
}
const releaseVersionSelect = document.getElementById("release-version");
releaseVersionSelect?.addEventListener("change", () => {
  const folder = selectedReleaseFolder();
  document.getElementById("release-tf-download").href = `./${folder}/dpReader_metalio47_firmware.bin`;
  document.getElementById("release-usb-download").href = `./${folder}/dpReader_metalio47_full.bin`;
  fetchLatestRelease();
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
      const res = await fetch(`./${requestedFolder}/metalio-s31-update.json?t=${Date.now()}`);
      if (res.ok) data = await res.json();
    } catch (_) {}

    if (!data || !data.version) {
      try {
        const ghRes = await fetch(`https://api.github.com/repos/dephia31/dpReader31/releases/${selectedReleaseFolder() === "releases" ? "latest" : "tags/v1.1.6-vn"}`);
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
    if (data && data.version) {
      currentReleaseInfo = data;
      if (latestVerName) latestVerName.textContent = `v${data.version}`;
      if (releaseTagApp) releaseTagApp.textContent = `Khuyên dùng · v${data.version}`;
      if (latestVerNotes && data.notes) {
        latestVerNotes.textContent = data.notes.split("\n")[0] || data.notes;
      }
      if (latestVerSize && data.size) {
        latestVerSize.textContent = `${(data.size / 1024 / 1024).toFixed(2)} MB`;
      }
    }
  } catch (err) {
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

// Bắt đầu nạp Flash
flashBtn.addEventListener("click", async () => {
  if (!esploader) {
    alert("Vui lòng kết nối máy đọc sách trước khi nạp!");
    return;
  }

  try {
    isFlashing = true;
    if (releaseVersionSelect) releaseVersionSelect.disabled = true;
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
        flashBtn.disabled = false;
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
    if (releaseVersionSelect) releaseVersionSelect.disabled = false;
    // Sau khi nạp thành công cổng đã được nhả; chỉ bật lại nút khi còn kết nối.
    // After a successful flash the port is released; only re-enable the button while still connected.
    if (esploader) {
      flashBtn.disabled = false;
      disconnectBtn.disabled = false;
    }
  }
});
