# 官网 OTA 维护 / Official OTA maintenance

官网刷机入口为 `https://kiikoread.com`；OTA默认清单及应用来源为 `https://wegooo-cell.github.io/read-pico-reader/`，正式包与历史版本归档GitHub Release。用户已确认此默认来源，rc97实机下载并校验原rc98成功。新固件优先GitHub Pages清单、官网清单备用；原已发布固件的内置顺序不会随服务器变更。只接受固件原有的两个HTTPS前缀，不向旧OTA提供Release重定向URL。浏览器能下载不代表旧固件能验证服务器证书。
Web flashing uses `https://kiikoread.com`; OTA metadata/images default to the existing GitHub Pages site, with original packages archived in Releases. A real rc97 device downloaded and verified rc98. New builds prefer Pages metadata; published clients retain their baked-in order. Keep the two exact HTTPS origins and avoid Release redirects for legacy OTA. Browser success alone does not establish legacy trust compatibility.

发布前及证书续期后运行（Python 需要 `cryptography`，curl 需要支持 `%{certs}`）：
Run before publication and after certificate renewal (Python `cryptography` and curl `%{certs}` support required):

```sh
python3 tools/ota_tls_host_test.py
python3 tools/check_official_ota.py --output build/ota-health.json
```

可用 `--trust-image /path/to/original-rc97.bin` 复用本地原正式版应用，脚本会检查固定 SHA；默认从官网获取原 rc97 应用。报告保存在被忽略的构建目录，不记录账户令牌。
Use `--trust-image /path/to/original-rc97.bin` to reuse a local original image; its fixed SHA is verified. Otherwise the original rc97 application is fetched from the official site. Reports stay in the ignored build directory and contain no account tokens.

检查 TLS 1.2/HTTP 1.1、证书有效期与签名、原 rc96/97/98 信任包的末端签发者兼容性、清单、实际应用版本/大小/SHA，以及 206 Range 续传。旧客户端未启用交叉签名链重建，不能因为链中间出现受信任的同名根就判为通过。此检查是宿主保守预检，发布验收仍需已发布固件真机完成“检查更新→下载→校验”。
Checks cover TLS 1.2/HTTP 1.1, validity/signatures, terminal-issuer compatibility with the original rc96/97/98 bundle, metadata, embedded app version/size/SHA and 206 Range resumes. Legacy clients lack cross-signed path rebuilding: a trusted root name elsewhere in the chain is insufficient. This conservative host precheck does not replace a released device completing discovery, download and verification.

每次发布核对GitHub Pages实际TLS1.2链与旧固件根包、清单、完整Bin大小/SHA、Range字节一致性及已发布固件实机下载；网络可用性不能做绝对保证。`check_official_ota.py`仅检查官网来源，GitHub OTA恢复不代表官网旧根兼容问题已修复。未经用户指示，不把默认应用来源改回官网。证书操作保留现有HTTPS，不关闭验证、降低HTTP、禁用Universal SSL或覆盖已发布字节；临时证书令牌使用完成后清理。
Before publication verify the actual Pages TLS1.2 chain against released roots, metadata, full image size/SHA, exact Range bytes and a released device download. Network availability cannot be guaranteed. The official-origin checker remains useful for site-certificate repair; healthy Pages OTA does not fix that separate issue. Do not silently restore the site image origin. Retain HTTPS validation and immutable binaries and clean up temporary certificate credentials.

参考 / References: [Espressif certificate bundle](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_crt_bundle.html), [Cloudflare Universal SSL API](https://developers.cloudflare.com/api/resources/ssl/subresources/universal/subresources/settings/methods/edit/).
