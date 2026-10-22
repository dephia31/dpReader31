# 官网图文说明书与检索

官网入口为 `https://kiikoread.com/manual/`，首页和说明书右下角的「问说明书」共用同一份操作索引。原 `manual.html` 与已公开的章节链接仍跳到对应新章节，PDF 地址保持 `manual/kiikoread-manual.pdf`。

## 内容与生成

- `docs/user-manual-detailed.zh-CN.json`：章节、操作路径、步骤、结果、提醒的唯一正文来源。
- `tools/build_detailed_manual.py`：生成网页、图解标注、检索索引及可选 PDF。
- `flash/manual/assets/`：使用当前固件绘图函数生成的公开界面示意；说明书蓝色编号不是设备新增控件。
- `flash/assets/manual-search.js`：排序、输入法等操作的同义说法与检索规则。
- `flash/assets/manual-helper.js`：官网提问框、说明书搜索框和跳转。

更新正文后，用现有图解重新生成网页：

```sh
python3 tools/build_detailed_manual.py --assets flash/manual/assets --output flash/manual --website
node tools/manual_search_test.js
```

新增操作必须有唯一且稳定的 `id`，不能破坏已分享的定位链接；更新相应原生图解后再替换图片。PDF 使用生成工具的 `--pdf --font <中文字体>` 选项另行生成，确认页面与字形后复制到固定公开 PDF 地址。网站打包仍走 `tools/stage_public_flash.py`；它核对说明书本地资源，同时保持原固件清单、Release 来源、SHA-256 与 OTA 兼容检查。

## 检索行为

支持关键词、口语问句、同义表达与有限错字容错，例如「最近看的书怎么排前面」「英文怎么打大写」「Wi-Fi 密码输错了」。最相关结果展示文档中的前三个步骤；完整步骤、提醒和图解通过带操作 ID 的链接打开。说明书正文搜索与提问框使用相同索引和排序。

索引在使用搜索时加载，后续问题复用内存中的索引。检索在浏览器本地执行，不调用外部问答接口，不编写文档之外的答案，不保存提问记录。无相关结果时显示无结果及完整说明书入口。网络加载失败可重试；快速更换问题时忽略旧请求结果。

`tools/manual_search_test.js` 验证关键问法、错字、无关问题与全部操作定位链接。发布前还需在真实浏览器检查提问框、检索、跨页跳转、手机布局与 PDF 下载；此项网站更新不改变固件版本或二进制。
