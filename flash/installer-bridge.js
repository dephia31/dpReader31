
    // attachShadow 不在父树上留下 mutation 记录，父树的观察器因此看不到后挂上的影子根。
    // 安装器的对话框正是这样长出来的：元素先插入，观察器触发一次时还没有影子根，根随后才
    // 挂上，于是整个对话框都翻不到，包括 "Erase device" 这个标题和它旁边的勾选框标签。
    // 这里挂个钩子，新的影子根一出现就登记下来，交给下面的翻译层处理。
    // attachShadow leaves no mutation record on the parent tree, so a parent observer cannot see a
    // shadow root attached after the element was inserted. That is exactly how the installer's
    // dialog grows: the element goes in, the observer fires once and finds no shadow root, and the
    // root is attached afterwards -- so the whole dialog stayed untranslated, "Erase device" as its
    // heading and as the checkbox label beside it included. Hook it here and register every new
    // root for the translation layer below.
    window.__flasherShadowRoots = [];
    (() => {
      const attachShadow = Element.prototype.attachShadow;
      Element.prototype.attachShadow = function (init) {
        const root = attachShadow.call(this, init);
        try { window.__flasherShadowRoots.push(root); } catch (e) { /* 忽略 / ignore */ }
        return root;
      };
    })();
    // 打包好的安装器直接调 navigator.serial.requestPort()，不带任何过滤，于是浏览器能看到的
    // 串口都会列出来，包括刷不了机的蓝牙虚拟串口。这里在它之前补一层 Espressif 厂商过滤：
    // Pico 的 USB Serial/JTAG（VID 303A）会列出来，蓝牙那些不会。放在独立脚本里是因为它必须
    // 早于下面那个 deferred 模块执行，而页面末尾那段脚本在本地 file:// 预览时会提前返回。
    // The bundled installer calls navigator.serial.requestPort() with no filters, so every port the
    // browser can see is offered, Bluetooth virtual COM ports included, and those cannot flash
    // anything. Add an Espressif vendor filter ahead of it: Pico's USB Serial/JTAG (VID 303A) is
    // listed and the Bluetooth entries are not. This lives in its own script because it must run
    // before the deferred module below, and the script at the end of the page returns early when
    // previewing over file://.
    if (navigator.serial && navigator.serial.requestPort) {
      const requestPort = navigator.serial.requestPort.bind(navigator.serial);
      navigator.serial.requestPort = options => {
        const next = Object.assign({}, options);
        if (!next.filters) next.filters = [{ usbVendorId: 0x303a }];
        return requestPort(next);
      };
    }
  


    // 安装器只带英文，打包产物里没有任何本地化钩子（localize / lang / translations 都是 0 处），
    // 所以由页面补一层翻译。lit 的插值会把整句拆成若干个文本节点，因此下面按片段映射，
    // 片段之间拼起来仍然是通顺的中文。作用范围只限安装器元素自己的树，翻过的节点会打标记，
    // 重渲染后重新翻，但不会重复翻。
    // The installer ships English only and its build carries no localisation hook at all (no
    // localize, lang or translations), so the page translates it. lit interpolates a sentence into
    // several text nodes, so the map below is built from fragments that still concatenate into
    // readable Chinese. It is scoped to the installer element's own tree, and translated nodes are
    // marked so a re-render re-translates without applying anything twice.
    (() => {
      const FRAGMENTS = [
        // 整句的两个半边，顺序拼接后成立。
        // The two halves of a sentence; concatenating them in order reads correctly.
        ["Do you want to erase the device before installing ", "安装 "],
        ["? All data on the device will be lost.", " 前是否擦除设备？设备上的所有数据都会丢失。"],
        ["Your device is running ", "设备当前运行 "],
        ["Do you want to ", "是否"],
        ["All data on the device will be erased.", "设备上的所有数据都会被擦除。"],
        // 对话框标题与按钮。
        // Dialog headings and buttons.
        ["Erase device", "擦除设备"],
        ["Confirm Installation", "确认安装"],
        ["Installation complete!", "安装完成！"],
        ["Installation failed", "安装失败"],
        ["Failed to initialize. Try resetting your device or holding the BOOT button while clicking INSTALL.", "未连接到刷写模式，尚未开始写入。请关闭此窗口，在墨水屏上打开「设置 → 升级和恢复 → BOOT 刷机」，点击「进入 BOOT 模式」，再重新连接安装。"],
        ["Preparing installation", "正在准备安装"],
        ["Erasing device...", "正在擦除设备…"],
        ["Device erased", "设备已擦除"],
        ["Erasing", "正在擦除"],
        ["Installing", "正在安装"],
        ["Trying to connect", "正在连接"],
        ["Writing progress:", "写入进度："],
        ["Back", "返回"],
        ["Next", "下一步"],
        ["Install", "安装"],
        ["update to", "更新到"],
        ["install", "安装"],
        ["Close", "关闭"],
        ["Connect", "连接"],
        ["Skip", "跳过"],
        ["Erase User Data", "擦除用户数据"],
        ["Visit Device", "访问设备"],
        ["Logs & Console", "日志与控制台"],
        ["Fund Development", "支持开发"],
        ["Add to Home Assistant", "添加到 Home Assistant"],
        ["Keep this page visible to prevent slow down", "请保持本页可见，以免变慢"],
        ["a minute", "一分钟"],
        ["2 minutes", "两分钟"],
      ];
      // 精确匹配优先，短的英文词不做子串替换，免得把固件名或日志翻坏。
      // Exact matches win; short English words are never replaced as substrings, so a firmware
      // name or a log line cannot be damaged.
      const EXACT = new Map(FRAGMENTS);
      // 前后缀只用于已知的整句，且要求原文以它开头或结尾。
      // Prefixes and suffixes apply only to the known sentences and must anchor the whole text.
      const PREFIX = [
        ["Install ", "安装 "],
        ["Update ", "更新 "],
      ];

      function translateText(node) {
        const raw = node.nodeValue;
        if (!raw) return;
        // 先保留片段尾部的拼接空格；未匹配时再忽略模板尾部的缩进，兼容按钮和错误提示。
        // Preserve fragment join spaces first, then ignore trailing template indentation for
        // buttons and error messages when the first exact match fails.
        const key = raw.replace(/^\s+/, "");
        if (!key.trim()) return;
        let next = raw;
        let matched = key;
        let exact = EXACT.get(matched);
        if (exact === undefined) {
          matched = key.trimEnd();
          exact = EXACT.get(matched);
        }
        if (exact !== undefined) next = raw.replace(matched, exact);
        else {
          let done = false;
          for (const [from, to] of PREFIX) {
            if (key.startsWith(from)) { next = raw.replace(from, to); done = true; break; }
          }
          if (!done) for (const [from, to] of FRAGMENTS) {
            if (from.length > 6 && key.endsWith(from)) { next = raw.replace(from, to); break; }
          }
        }
        // 句末的问号与句号按中文全角走；只碰整句就是标点的那种文本节点。
        // A sentence-final question mark or full stop becomes full width; only a node that is
        // nothing but that punctuation is touched.
        next = next.replace(/\?(?=\s*$)/, "？");
        if (/^\.\s*$/.test(next)) next = next.replace(".", "。");
        if (next !== raw) node.nodeValue = next;
      }

      function translateAttrs(root) {
        for (const el of root.querySelectorAll("*")) {
          for (const name of ["label", "heading"]) {
            const value = el.getAttribute && el.getAttribute(name);
            if (value === null || value === undefined) continue;
            const hit = EXACT.get(value.trim());
            if (hit !== undefined) el.setAttribute(name, hit);
            else for (const [from, to] of PREFIX) {
              if (value.trim().startsWith(from)) { el.setAttribute(name, value.replace(from, to)); break; }
            }
          }
        }
      }

      // 影子树是分开的文档树，观察器不会跨过去，所以每发现一个新的影子根都要挂一次。
      // A shadow tree is a separate tree, so an observer does not cross into it; every newly seen
      // shadow root gets its own observer.
      const watched = new WeakSet();

      function walk(root) {
        if (!root) return;
        const nodes = root.ownerDocument
          ? root.ownerDocument.createTreeWalker(root, NodeFilter.SHOW_TEXT)
          : document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        for (let n = nodes.nextNode(); n; n = nodes.nextNode()) translateText(n);
        translateAttrs(root);
        const elements = root.querySelectorAll ? root.querySelectorAll("*") : [];
        for (const el of elements) if (el.shadowRoot) walk(el.shadowRoot);
        if (root.host && root.host.shadowRoot !== root) walk(root.host.shadowRoot);
      }

      function watch(root) {
        if (!root || watched.has(root)) return;
        watched.add(root);
        new MutationObserver(() => {
          walk(root);
          for (const el of root.querySelectorAll("*")) if (el.shadowRoot) watch(el.shadowRoot);
        }).observe(root, {childList: true, subtree: true, characterData: true});
        for (const el of root.querySelectorAll("*")) if (el.shadowRoot) watch(el.shadowRoot);
      }

      // 处理钩子登记到的每个影子根，并递归进它内部的影子根。
      // Handle every root the hook registered, recursing into the roots inside it.
      function drain() {
        const roots = window.__flasherShadowRoots;
        if (!roots) return;
        for (let i = 0; i < roots.length; i++) {
          const root = roots[i];
          if (watched.has(root)) continue;
          walk(root);
          watch(root);
        }
      }
      const installer = () => document.getElementById("flash-installer");
      function start() {
        const el = installer();
        if (!el || !el.shadowRoot) return false;
        walk(el.shadowRoot);
        watch(el.shadowRoot);
        return true;
      }
      start();
      drain();
      // 钩子负责及时性，慢扫描负责兜底：还有别的路径能长出影子根（自定义元素自行 upgrade
      // 等），漏掉一个就是整块对话框没翻。树很小，扫一遍没有成本。
      // The hook handles promptness and the slow sweep is the safety net: other paths can create a
      // shadow root too (a custom element upgrading on its own), and missing one means a whole
      // dialog stays in English. The tree is tiny, so a sweep costs nothing.
      setInterval(() => { start(); drain(); }, 400);
    })();
  