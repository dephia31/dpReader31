# kiikoread · Metalio S31

**Metalio E-INK4 Plus / ESP32-S31、V1.5、34Pin E0470A14-AF** 向けの独立した kiikoread ファームウェアです。ソースを公開しています。Metalio の公式リリースではありません。

[简体中文](README.zh-CN.md) · [English](README.md)

## 正式版 rc103

- [公式 Web インストーラー](https://kiikoread.com/?device=metalio&version=rc103#flash-action)
- [ファームウェア、TF/USB パッケージ、SHA-256](https://github.com/wegooo-cell/read-pico-reader/releases/tag/metalio-s31-v0.3.3-rc103)
- [変更履歴](docs/CHANGELOG.md) · [ボードとビルド](docs/BOARDS.md)

本リポジトリは S31 ソースを管理し、正式バイナリは共通の Release に保存します。ここに残る test Release は以前のプレビュー版です。Pico と S31 のファームウェアは交換できません。

EPUB/TXT、画像、128件までの TTF/OTF フォント、暖色/寒色フロントライト、振動、AI キー、BLE ページ送り、Wi-Fi/USB 転送、OTA と複数のロック画面を備えます。rc103 の電源設定は「浅いスリープから10分後に深いスリープ」と「完全電源オフ」の2種類です。実際の待機時間と復帰動作は引き続き実機確認が必要です。

初回は **Metalio 公式ファームウェアに戻して、そこから BOOT に入ってください**。対応する kiikoread が導入済みなら直接更新できます。通常は設定を保持し、TF ファイルも保持します。アプリケーション Bin を `0x0` に直接書き込まないでください。

ビルドと使用方法は[中国語ガイド](README.zh-CN.md)をご覧ください。許諾範囲は [LICENSE](LICENSE)、[第三者の告知](THIRD_PARTY_NOTICES.md)と各ファイルの表示に従います。既存の Apache-2.0 等の権利は維持されます。
