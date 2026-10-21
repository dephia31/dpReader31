/* SPDX-License-Identifier: Apache-2.0
 * 中文：刷机版本选择仅使用已发布清单；每次选择固定一个独立安装入口。
 * English: Select published manifests and bind each choice to an independent installer.
 */
(() => {
  const selector = document.getElementById("firmware-choice");
  const version = document.getElementById("firmware-version");
  const updated = document.getElementById("firmware-updated");
  const download = document.getElementById("bin-download");
  const help = document.getElementById("flash-help");
  const title = document.getElementById("flash-help-title");
  const hint = document.getElementById("firmware-choice-hint");
  let installer = document.getElementById("flash-installer");
  let serialReady = false, manifestReady = false, request = 0;
  const latest = {manifest: "./manifest.json", bin: "./Pico-update.bin"};
  const releases = new Map([[latest.manifest, latest]]);
  const showInstaller = () => {
    installer.hidden = !(serialReady && manifestReady);
    help.hidden = serialReady && manifestReady;
  };
  async function readJSON(path) {
    const response = await fetch(path, {cache: "no-store"});
    if (!response.ok) throw new Error("Release unavailable");
    return response.json();
  }
  async function selectRelease() {
    const current = ++request;
    const choice = releases.get(selector.value);
    manifestReady = false;
    installer.hidden = true;
    download.hidden = true;
    download.removeAttribute("href");
    version.textContent = "读取中…";
    document.dispatchEvent(new CustomEvent("kiikoread:release-loading"));
    updated.textContent = "读取中…";
    updated.removeAttribute("datetime");
    if (serialReady) title.textContent = "正在读取所选版本…";
    showInstaller();
    try {
      if (!choice) throw new Error("Unknown release");
      const manifestURL = new URL(choice.manifest, document.baseURI).href;
      const manifest = await readJSON(manifestURL);
      if (current !== request) return;
      if (!/^\d+\.\d+\.\d+(?:-rc\d+)?$/.test(manifest.version) ||
          (choice.version && manifest.version !== choice.version) ||
          manifest.new_install_prompt_erase !== true) throw new Error("Invalid release");
      version.textContent = manifest.version;
      if (choice === latest) selector.options[0].textContent = `最新正式版 · ${manifest.version}`;
      const published = new Date(manifest.updated_at);
      if (Number.isFinite(published.getTime())) {
        updated.dateTime = published.toISOString();
        updated.textContent = new Intl.DateTimeFormat("zh-CN", {
          timeZone: "Asia/Shanghai", year: "numeric", month: "2-digit", day: "2-digit",
          hour: "2-digit", minute: "2-digit", hourCycle: "h23"
        }).format(published).replace(/\//g, "-");
      } else updated.textContent = "暂未提供";
      // 串口选择尚未结束时切换版本，旧点击仍持有其原清单，不会改变待刷固件。
      // A pending serial picker retains its clicked element's manifest when a new choice replaces it.
      const next = installer.cloneNode(true);
      next.manifest = manifestURL;
      next.setAttribute("manifest", manifestURL);
      next.prepareManifest = () => window.KiikoreadFlasher.prepare(manifest, manifestURL, choice.files);
      const shortVersion = manifest.version.split("-").pop();
      next.querySelector("button").textContent = `连接墨水屏，刷入 ${shortVersion}`;
      installer.replaceWith(next);
      installer = next;
      download.href = new URL(choice === latest ? `./Pico-update-${manifest.version}.bin` : choice.bin,
                              document.baseURI).href;
      download.textContent = `下载 ${shortVersion} TF 卡升级 Bin ↓`;
      download.hidden = false;
      manifestReady = true;
      showInstaller();
      document.dispatchEvent(new CustomEvent("kiikoread:release", {detail: {manifest, latest: choice === latest}}));
    } catch (error) {
      if (current !== request) return;
      version.textContent = "读取失败，请重选版本或刷新页面";
      document.dispatchEvent(new CustomEvent("kiikoread:release-unavailable"));
      updated.textContent = "暂不可用";
      if (serialReady) title.textContent = "所选版本暂不可用，请重选版本或刷新页面。";
      showInstaller();
    }
  }
  if (location.protocol === "file:") {
    version.textContent = updated.textContent = "请在线上刷机页查看";
    title.textContent = "本地预览无法刷机。";
    return;
  }
  if (!window.isSecureContext) title.textContent = "请使用 HTTPS 刷机页。";
  else if (!("serial" in navigator)) title.textContent = "当前浏览器无法连接串口。";
  else {
    title.textContent = "正在加载刷机按钮…";
    const timer = setTimeout(() => {
      if (!serialReady) title.textContent = "刷机按钮未能加载，请刷新页面后重试。";
    }, 5000);
    customElements.whenDefined("esp-web-install-button").then(() => {
      clearTimeout(timer);
      serialReady = true;
      if (!manifestReady) title.textContent = "正在读取所选版本…";
      showInstaller();
    });
  }
  // 安装对话框打开时冻结版本选择；关闭后恢复，避免已开始的操作与页面文案混淆。
  // Freeze the selector while an install dialog is open, restoring it when the dialog closes.
  let catalogReady = false;
  const syncSelector = () => {
    selector.disabled = !catalogReady || !!document.querySelector("ewt-install-dialog");
  };
  new MutationObserver(syncSelector).observe(document.body, {childList: true});
  selector.addEventListener("change", selectRelease);
  (async () => {
    try {
      const catalog = await readJSON("./releases.json");
      if (catalog.schema !== 1 || !Array.isArray(catalog.releases)) throw new Error("Invalid catalog");
      for (const release of catalog.releases) {
        if (!/^\d+\.\d+\.\d+(?:-rc\d+)?$/.test(release.version) ||
            release.manifest !== `releases/${release.version}/manifest.json` ||
            release.bin !== `Pico-update-${release.version}.bin` || releases.has(release.manifest))
          throw new Error("Invalid archived release");
      }
      for (const release of catalog.releases) {
        releases.set(release.manifest, release);
        const option = document.createElement("option");
        option.value = release.manifest;
        option.textContent = `${release.version} · 历史正式版`;
        selector.appendChild(option);
      }
      const wanted = new URLSearchParams(location.search).get("version");
      const choice = catalog.releases.find(item => item.version === wanted || item.version.split("-").pop() === wanted);
      if (choice) selector.value = choice.manifest;
    } catch (error) {
      hint.textContent = "历史版本列表暂不可用，可选择最新正式版或刷新页面重试。";
    }
    catalogReady = true;
    syncSelector();
    await selectRelease();
  })();
})();
