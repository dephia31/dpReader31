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

    if (radioApp.checked) {
      fileName = "dpReader_metalio47_firmware.bin";
      offset = 0x10000;
      progressStatus.textContent = "Đang tải firmware ứng dụng mới nhất...";
      binaryData = await fetchBinary(
        `./releases/${fileName}`,
        `https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/${fileName}`
      );
    } else if (radioFull.checked) {
      fileName = "dpReader_metalio47_full.bin";
      offset = 0x0;
      progressStatus.textContent = "Đang tải firmware toàn diện (Full Flash)...";
      binaryData = await fetchBinary(
        `./releases/${fileName}`,
        `https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/${fileName}`
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
      progressStatus.textContent = `Đang đọc tệp ${fileName}...`;
      binaryData = await file.arrayBuffer();
    }

    terminal.writeLine(`[+] Kích thước tệp: ${binaryData.byteLength.toLocaleString()} bytes`);
    terminal.writeLine(`[*] Vị trí ghi (Offset): 0x${offset.toString(16).toUpperCase()}`);
    terminal.writeLine("[*] Đang chuyển đổi định dạng và bắt đầu nạp...");

    const binaryString = arrayBufferToBinaryString(binaryData);
    const fileArray = [{ data: binaryString, address: offset }];

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
    terminal.writeLine("[*] Đang khởi động lại thiết bị (Hard reset)...");

    progressBarFill.style.backgroundColor = "var(--success-color)";
    progressPercent.textContent = "100%";
    progressStatus.textContent = "Hoàn tất 100%! Thiết bị đang khởi động lại vào hệ điều hành mới.";

    try {
      await esploader.hardReset();
    } catch (e) {
      console.warn("Reset note:", e);
    }

    alert("🎉 Nạp firmware thành công rực rỡ!\nThiết bị đang tự động khởi động lại.");

  } catch (err) {
    terminal.writeLine(`[!] LỖI TRONG QUÁ TRÌNH NẠP: ${err.message || err}`);
    console.error(err);
    progressBarFill.style.backgroundColor = "var(--danger-color)";
    progressStatus.textContent = `Lỗi: ${err.message || err}`;
    alert(`Nạp thất bại: ${err.message || err}`);
  } finally {
    isFlashing = false;
    flashBtn.disabled = false;
    disconnectBtn.disabled = false;
  }
});
