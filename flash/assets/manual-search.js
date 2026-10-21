/* SPDX-License-Identifier: Apache-2.0
 * 中文：说明书检索：同义说法、中文片段及少量错字；结果只引用实际操作。
 * English: Rank documented tasks using aliases, character fragments and limited typo tolerance.
 */
(function (root) {
  'use strict';
  const aliases = {
    'recent-sort': '最近排序 最近看的书排前面 最近打开 按时间排序 最近读过的书 最近阅读 按最近',
    'shelf-style': '列表书架 封面书架 亚克力书架 深色书架 更换书架样式',
    'shelf-search': '搜书 找书 书名 拼音 首字母',
    'shelf-pages': '书架下一页 书架上一页 书架翻页',
    'favorite-book': '收藏书 取消收藏 收藏优先 星标',
    'batch-select': '多选书 批量操作 跨页选择 全选书',
    'delete-books': '删除书 批量删书 删除所选',
    'unshelf': '移出书架 隐藏书 保留文件',
    'rescan': '书架刷新 重新扫描 找不到新书',
    'rename-file': '文件改名 书籍改名 修改书名 重命名文件',
    'rename-folder': '文件夹改名 重命名目录',
    'copy-file': '文件复制 副本 重复文件',
    'move-file': '移动文件 换目录 移到文件夹',
    'connect-wifi': '联网 连接无线 wifi连接 家里网络',
    'password-fix': 'wifi密码 无线密码 密码输错 看密码 显示密码 英文密码',
    'disconnect-wifi': '断开wifi 关闭wifi 断网',
    'forget-wifi': '遗忘网络 忘记网络 删除wifi密码',
    'wifi-transfer': 'wifi传书 无线传书 电脑传书 同一网络',
    'hotspot-transfer': '热点传书 手机传书 设备热点',
    'usb-transfer': 'usb传书 数据线传书 读卡器 电脑读卡',
    'web-any-file': '任意文件 任意格式 根目录上传 添加文件 上传bin',
    'web-signature': '网页设置签名 手机改签名',
    'web-fonts': '上传字体 网页字体 手机字体',
    'ime-case': '大小写 英文大写 英文小写 字母大写 shift caps lock uppercase lowercase 大小谢 大写锁定',
    'ime-english': '中文英文 中英切换 输入英文 字母输入',
    'ime-nine': '九宫格 9键 拼音九键',
    'ime-full-pinyin': '全键盘 24键 26键 qwerty 全拼',
    'ime-phrases': '词组 整句 联想 多字 连续拼音 一句话 一段话 连续打 连续输入',
    'ime-candidates': '候选翻页 找字 找不到字 下一组候选',
    'ime-digits': '数字 123 数字键盘 输入数字',
    'ime-symbols': '标点 符号 问号 引号 叹号',
    'ime-caret': '光标 插入文字 修改中间的字 移动光标',
    'ime-delete': '退格 连续删除 长按删除 删除文字',
    'ime-retype-clear': '清空 重输 重新输入',
    'fullscreen': '全屏 非全屏 双击中间 退出全屏 隐藏状态栏',
    'reader-tools': '阅读工具栏 呼出工具栏 打开阅读设置',
    'toc-jump': '章节目录 目录跳转 按百分比跳转',
    'bookmarks': '书签 添加书签 删除书签 标记页面',
    'tap-horizontal': '左右翻页 左半边 右半边',
    'tap-vertical': '上下翻页 上三分之一 下三分之二',
    'swipe-pages': '滑动翻页 左右滑动 划页 手势翻页',
    'shake-turn': '晃动翻页 摇一摇 灵敏度 方向',
    'reading-font': '阅读字体 正文字体 更换读书字体',
    'system-font': '系统字体 导入字体 界面字体',
    'reading-size': '阅读字号 字太小 正文放大',
    'system-size': '系统字号 界面放大 200%',
    'indent-fine': '首行缩进 首行锁紧 缩进微调 两格 对不齐',
    'layout-margins': '页边距 行距 字距 排版',
    'paragraph-spacing': '段距 段落间距',
    'manual-refresh': '阅读全刷 清理阅读残影 手动刷新',
    'auto-refresh': '自动全刷 每几页全刷 全刷间隔',
    'reader-effect': '翻页效果 阅读水波纹',
    'main-refresh': '主页刷新模式 快刷 普通模式 水波纹模式',
    'hide-images': '关闭书内图片 不显示插图 跳过图片',
    'power-turn': '电源键翻页 关机键翻页',
    'key-presets': '按键推荐 设置1 设置2',
    'custom-short': '自定义按键 左键 中键 右键 短按',
    'custom-hold': '中键长按 长按中间键 长按返回主页 自定义长按',
    'keep-tools': '工具栏不见了 无法呼出工具栏 按键保底',
    'ble-pair': '蓝牙配对 连接翻页器 扫描翻页器',
    'ble-learn': '按键学习 蓝牙按键映射 识别翻页器',
    'ble-shelf': '蓝牙书架翻页 上下键书架',
    'auto-lock': '自动休眠 自动锁屏 关闭自动锁屏 一分钟 五分钟 十分钟',
    'sleep-resume': '深度睡眠 回到最后阅读 恢复阅读 唤醒首页',
    'pin-enable': '锁屏密码 开启密码 四位密码 解锁密码',
    'pin-change': '修改锁屏密码 关闭锁屏密码 解除锁屏密码 取消锁屏密码 关闭密码',
    'pin-unlock': '解锁 输入密码 密码错误',
    'lock-style': '锁屏样式 切换锁屏 壁纸 票根 拼贴',
    'wallpaper-lock': '壁纸锁屏 换壁纸',
    'ticket-lock': '票根锁屏 阅读票根',
    'collage-lock': '拼贴锁屏 封面拼贴',
    'profile-name': '资料卡姓名 资料卡名字 改名字',
    'profile-avatar': '头像 修改头像',
    'device-signature': '个性签名 状态栏签名 顶部签名',
    'time-sync': '时间不准 对时 时钟 日期 时间设置',
    'card-format': 'fat32 exfat 存储卡格式 tf格式',
    'device-card-format': '格式化内存卡 设备格式化 清空卡',
    'faq-sd': '卡不识别 读不出卡 插卡失败 ESP_ERR_INVALID_STATE 256g',
    'faq-empty-shelf': '书架空了 没有书 找不到书籍',
    'boot-recovery': '反复重启 开机循环 书打不开重启',
    'faq-ghost': '残影 黑闪 刷新不干净',
    'faq-open-epub': 'epub打不开 打开书失败',
    'faq-web-flash': '刷机连接失败 failed to initialize 找不到串口',
    'ota-check': '检查更新 联网更新 找新版本',
    'ota-install': '安装更新 ota升级 下载新版本',
    'tf-update': 'tf卡升级 bin放根目录 Pico-update.bin',
    'downgrade': '降级 老版本 回退版本 rc88',
    'boot-mode': 'boot刷机 进入boot 下载模式',
    'web-install': '首次安装 网页刷机 重新刷机',
    'save-config': '备份配置 保存配置 保存进度 Pico-settings.backup',
    'restore-config': '恢复配置 恢复设置 恢复进度 恢复阅读进度 刷机后恢复 wifi恢复',
    'restore-paths': '恢复后没有进度 恢复失败 路径不一致'
  };
  const normalize = value => String(value).normalize('NFKC').toLowerCase()
    .replace(/wi[\s-]?fi/g, 'wifi')
    .replace(/内存卡|存储卡|sd卡/g, 'tf卡').replace(/密吗|密马/g, '密码')
    .replace(/[\s\p{P}\p{S}]/gu, '');
  const strip = q => q.replace(/请问|能不能|可以吗|有没有|我想知道|我想|想要|怎样|怎么|如何|一下|这里|那个|这个|操作|方法|功能|我|的|了|呢|吗|啊|呀/g, '');
  const grams = value => {
    const out = new Set();
    const parts = value.match(/[a-z0-9]+|[\u3400-\u9fff]+/g) || [];
    for (const part of parts) {
      if (/^[a-z0-9]+$/.test(part) || part.length === 1) out.add(part);
      else for (let i = 0; i < part.length - 1; i++) out.add(part.slice(i, i + 2));
    }
    return [...out];
  };
  function oneEdit(a, b) {
    if (Math.abs(a.length - b.length) > 1 || Math.min(a.length, b.length) < 3) return false;
    let i = 0, j = 0, edits = 0;
    while (i < a.length && j < b.length) {
      if (a[i] === b[j]) { i++; j++; continue; }
      if (++edits > 1) return false;
      if (a.length >= b.length) i++;
      if (b.length >= a.length) j++;
    }
    return edits + (i < a.length || j < b.length ? 1 : 0) === 1;
  }
  function create(tasks) {
    const df = new Map();
    const records = tasks.map(task => {
      const phrases = (aliases[task.id] || '').split(' ').filter(Boolean).map(normalize);
      const title = normalize(task.title), path = normalize(task.path);
      const body = normalize([...task.steps, task.result || '', task.tip || ''].join(' '));
      const primary = title + phrases.join('');
      const vocabulary = new Set(grams(primary + path + body));
      for (const word of vocabulary) df.set(word, (df.get(word) || 0) + 1);
      return { task, title, path, body, phrases, primary, vocabulary };
    });
    return function search(query, limit = 8) {
      const q = strip(normalize(query).slice(0, 180));
      if (!q) return [];
      const terms = grams(q), hits = [];
      for (const r of records) {
        let score = 0, covered = 0, strong = false;
        if (r.title.includes(q)) { score += 35; strong = true; }
        if (r.path.includes(q)) { score += 12; strong = true; }
        for (const phrase of r.phrases) {
          if (phrase === q) { score += 45; strong = true; }
          else if (phrase.includes(q)) { score += 20; strong = true; }
          else if (phrase.length >= 3 && q.includes(phrase)) { score += 20 + phrase.length; strong = true; }
          else if (q.length <= 9 && oneEdit(q, phrase)) { score += 18; strong = true; }
        }
        for (const word of terms) {
          const weight = Math.log(1 + tasks.length / (1 + (df.get(word) || 0)));
          if (r.vocabulary.has(word)) {
            covered++;
            score += weight * (r.primary.includes(word) ? 4 : r.path.includes(word) ? 2 : 0.75);
          }
        }
        const coverage = covered / Math.max(1, terms.length);
        score *= 0.25 + 0.75 * coverage;
        // 中文：无相关证据时不编造答案；单个常用片段不足以触发结果。
        // English: Require evidence; one common fragment is not enough to invent an answer.
        if ((strong || coverage >= 0.42) && score >= 5) hits.push({ ...r.task, score });
      }
      return hits.sort((a, b) => b.score - a.score).slice(0, limit);
    };
  }
  const api = { create, normalize };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.KiikoManualSearch = api;
})(typeof window !== 'undefined' ? window : globalThis);
