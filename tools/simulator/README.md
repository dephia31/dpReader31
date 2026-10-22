# kiikoread 源码模拟器 / Source simulator

使用 v0.3.3-rc97 发布源码绘制 684×1216 界面，运行同一套 TXT/EPUB 排版、书签、阅读记录和设置代码。浏览器与 macOS 共用宿主适配；不执行 ESP32 BIN。WiFi、蓝牙、OTA、USB 和物理波形仍需实机验证。

The native and browser builds share host adapters around released firmware UI and reader code. This is source simulation, not ESP32 BIN emulation. Hardware connections and panel waveforms require device testing.

## 网页构建 / Browser build

安装官方 Emscripten **6.0.13** 并激活其环境，Node 22 或更新版本、Python 3.10 或更新版本。在本目录执行：

Install and activate Emscripten 6.0.13, Node 22+ and Python 3.10+. Run from this directory:

```sh
PICO_SIM_FIRMWARE=../.. EMCC="$(command -v emcc)" NODE="$(command -v node)" python3 web/build.py
python3 web/dev_server.py 8880
```

使用与 source-manifest.json 相同的发布 commit 检出仓库，再编译。环境变量指向该仓库根目录；本机已有 firmware/ 快照时可省略 PICO_SIM_FIRMWARE。构建不改固件源码；异步预取调整和测试目标元数据只生成到宿主副本。web/dist 的成品复制到 flash/simulator/ 发布。

Build against the release commit in source-manifest.json. PICO_SIM_FIRMWARE points to that checkout; omit it when using a prepared firmware/ snapshot. Generated host copies disable threaded prefetch and expose read-only tutorial targets; the firmware tree is unchanged. Publish web/dist as flash/simulator/.

## macOS

Apple 芯片、macOS 13+，需要 Xcode Command Line Tools。在本目录执行：

Apple Silicon, macOS 13+ and Xcode Command Line Tools:

```sh
PICO_SIM_FIRMWARE=../.. python3 build.py
python3 package.py
```

运行 Pico Simulator.app；Data/ 与 app 同目录，首次运行会创建。模拟器目录中的 prepare.py 可从 PICO_RELEASE_SOURCE 指定的同版发布源码创建不可覆盖快照。

Run Pico Simulator.app. Data/ is created beside the app. prepare.py can create a non-overwriting snapshot from PICO_RELEASE_SOURCE.

## 问说明书演示 / Manual tours

- web/guides.js 定义主题、步骤、手势和预期状态。每条流程只提交固件已有输入，不伪造完成。
- web/tour-controller.js 管理暂停、上一步、下一步和独立演示实例；关闭后恢复原实例。
- 问说明书保留本地检索，在匹配主题下展示演示入口；硬件功能只展示入口和实机操作说明。
- 个人设置、密码摘要与阅读记录留在浏览器 IndexedDB；演示不持久化。导入书籍和字体仅本次会话使用，不上传。

Topics, gestures and expected states live in web/guides.js. Tours run in a separate ephemeral firmware worker and restore the personal worker when closed. Hardware topics show entry points only. Personal settings and reading records use local IndexedDB; imported files remain session-local and are never uploaded.

## 第三方 / Third party

- stb_image.h：保留上游 MIT / public-domain 双许可原文。
- zlib 1.3.2：保留 web/vendor/zlib/LICENSE。
- sha256.c、sha256.h：Brad Conte 的 crypto-algorithms 公有领域实现，保留原作者声明（https://github.com/B-Con/crypto-algorithms）。
- 固件和其字体、Hanzi、图标等资产继续适用各自原有许可；原创宿主适配遵循仓库根 LICENSE。无私人书库、个人字体或真实账号资料。

Original upstream licenses and attributions are retained. Firmware and bundled assets retain their respective licenses; original host adapters follow the repository LICENSE. No personal library, fonts or account data are included.

## 浏览器验证 / Browser checks

启动上述 8880 端口后，在 web 目录安装 Playwright 并运行 `npm run test:tours`、`npm run test:storage`。默认使用 Playwright Chromium；CHROME_PATH 可指定本机 Chrome。输出留在被忽略的 build/web 中。

Serve on port 8880, install the web package and Playwright Chromium, then run the tour and storage scripts. CHROME_PATH may select an existing Chrome binary. Reports stay in ignored build/web.

Safari 启动：数据包预压缩 gzip，内容哈希资源长期缓存；使用 web/dev_server.py 提供一致的本地响应头。阅读记录恢复超过 2.5 秒时启动不写盘的临时实例，保留原浏览器记录。仅在画面变化时转换像素，后台和屏幕外停止轮询。

Safari startup uses precompressed gzip data and fingerprinted cached assets. The dev server sends matching headers. A timed-out history restore restarts in a non-persistent instance without modifying stored records. Only changed screen areas are decoded; hidden and off-screen views stop polling.
