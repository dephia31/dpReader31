/* SPDX-License-Identifier: Apache-2.0
 * 中文：将选中的刷机包校验为独立快照，安装器只读取已验证的本地 Blob，避免旧缓存或发布切换混入其他版本。
 * English: Verify each selected bundle as an independent snapshot; the installer reads validated local blobs instead of stale caches or a changing release.
 */
(() => {
  const names = new Map([[0, "bootloader.bin"], [0x8000, "partitions.bin"],
    [0x10000, "firmware.bin"], [0xd10000, "ota_data_initial.bin"]]);
  const digest = async bytes => [...new Uint8Array(await crypto.subtle.digest("SHA-256", bytes))]
    .map(value => value.toString(16).padStart(2, "0")).join("");
  function appVersion(bytes) {
    if (bytes.byteLength < 112 || bytes[0] !== 0xe9 ||
        new DataView(bytes.buffer, bytes.byteOffset).getUint32(32, true) !== 0xabcd5432)
      throw new Error("下载文件不是有效的墨水屏固件，请刷新后重试。");
    const field = offset => new TextDecoder().decode(bytes.slice(offset, offset + 32)).split("\0")[0];
    if (field(80) !== "Read_Pico") throw new Error("下载固件的设备型号不匹配，已停止安装。");
    return field(48);
  }
  async function prepare(manifest, manifestURL, archiveFiles) {
    if (!/^\d+\.\d+\.\d+(?:-rc\d+)?$/.test(manifest.version) ||
        manifest.new_install_prompt_erase !== true || manifest.builds?.length !== 1 ||
        manifest.builds[0].chipFamily !== "ESP32-S3" || manifest.builds[0].parts?.length !== 4)
      throw new Error("刷机清单无效，请刷新页面后重试。");
    const snapshot = JSON.parse(JSON.stringify(manifest));
    const urls = [], seen = new Set();
    let disposed = false;
    const dispose = () => {
      if (disposed) return;
      disposed = true;
      for (const url of urls) URL.revokeObjectURL(url);
    };
    try {
      const requests = snapshot.builds[0].parts.map(part => {
        const name = names.get(part.offset), info = part.sha256 ? part : archiveFiles?.[name];
        if (!name || seen.has(name) || !info || !/^[a-f0-9]{64}$/.test(info.sha256) ||
            !Number.isSafeInteger(info.size) || info.size <= 0)
          throw new Error("刷机包缺少完整性信息，请刷新页面后重试。");
        seen.add(name);
        // 应用使用固定版本名；每个基础文件也绑定内容哈希，绕过所有通用地址的旧缓存。
        // Pin the app by version and every base part by content hash, bypassing cached mutable aliases.
        const target = name === "firmware.bin" && !archiveFiles
          ? `Pico-update-${manifest.version}.bin` : part.path;
        const url = new URL(target, manifestURL);
        if (url.origin !== new URL(manifestURL).origin)
          throw new Error("刷机文件来源与所选清单不一致，已停止安装。");
        url.searchParams.set("sha256", info.sha256);
        return {part, name, info, url: url.href};
      });
      const results = await Promise.allSettled(requests.map(async ({part, name, info, url}) => {
        const response = await fetch(url, {cache: "no-store"});
        if (!response.ok) throw new Error(`刷机文件下载失败（${response.status}），尚未写入设备。`);
        const data = await response.arrayBuffer();
        if (data.byteLength !== info.size || await digest(data) !== info.sha256)
          throw new Error(`${name} 校验失败，可能收到了旧缓存；已停止安装，请刷新后重试。`);
        if (name === "firmware.bin" && appVersion(new Uint8Array(data)) !== manifest.version)
          throw new Error("固件内部版本与所选版本不一致，已停止安装。");
        const blobURL = URL.createObjectURL(new Blob([data], {type: "application/octet-stream"}));
        urls.push(blobURL);
        part.path = blobURL;
      }));
      const failure = results.find(result => result.status === "rejected");
      if (failure) throw failure.reason;
      const url = URL.createObjectURL(new Blob([JSON.stringify(snapshot)], {type: "application/json"}));
      urls.push(url);
      return {url, dispose};
    } catch (error) {
      dispose();
      throw error;
    }
  }
  window.KiikoreadFlasher = Object.freeze({prepare});
})();
