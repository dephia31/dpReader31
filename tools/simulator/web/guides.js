// 演示只提交固件已有的点按和手势，不直接设置页面或伪造成功。/ Submit existing firmware input only; never forge a successful result.
const step=(text,target,action,expect)=>({text,target,action,expect});
const home=()=>step('先回到首页，找到底部的四个页面入口。',[85,1140],'page 0',{page:'首页'});
const open=(epub=false)=>step('打开内置演示书，实际进入阅读正文。',[330,410],`open /sdcard/books/${epub?'山间来信.epub':'海边的信.txt'}`,{page:'图书',book:1});
const tools=()=>step('从屏幕底部上滑，或短按设备中键，呼出阅读工具栏。',[342,1170],'key 1',{panel:1});
const fonts=()=>step('点工具栏最右侧的「字体设置」。',[616,1140],'tap 616 1140',{panel:2});
const settings=()=>step('点底栏的「设置」，进入设置主页面。',[598,1140],'page 3',{settings:0,page:'设置'});
const setting=(name,index,view)=>step(`向下找到「${name}」，点进对应设置。`,`setting:${index}`,{setting:index},{settings:view});
const done=()=>step('点「完成」，回到正文查看排版变化。',[342,1170],'tap 342 1170',{panel:0});
const view=(text,expected)=>step(text,null,null,expected);
const fontBase=()=>[open(),tools(),fonts()];
const readerCard=(name,index,panel)=>step(`点「${name}」，进入对应阅读设置。`,`reader:${index}`,{reader:index},{panel});
export const tours={
  navigation:{title:'认识四个主页面',ids:['navigation'],steps:[home(),step('点「书架」，查看内置演示书。',[256,1140],'tap 256 1140',{page:'图书',book:0}),step('点「文件管理」，浏览虚拟 TF 卡。',[428,1140],'tap 428 1140',{page:'文件管理'}),step('点「设置」，查看显示和个性化选项。',[598,1140],'tap 598 1140',{page:'设置'}),view('四个页面已依次展示。退出演示后可以自由点按。')]},
  'quick-menu':{title:'下拉快捷菜单',ids:['quick-menu','status-bar','manual-refresh'],steps:[home(),step('从屏幕最顶部向下滑动，展开玻璃快捷面板。',[342,22],{swipe:[342,10,342,190]},{quick:true}),view('四个圆键依次是 Wi-Fi、蓝牙、全刷、锁屏。网络与蓝牙在体验中为模拟状态。'),step('点面板外部，收起快捷菜单。',[342,450],'tap 342 450',{quick:false})]},
  'reader-tools':{title:'打开阅读工具栏',ids:['reader-tools','three-keys','keep-tools'],steps:[open(),tools(),view('下方依次是目录、书签、阅读统计、阅读设置和字体设置。')]},
  'reading-size':{title:'调整正文字号',ids:['reading-size'],steps:[...fontBase(),step('拖动 A 滑块，增大字号。松开后由固件重新排版。',[420,719],{swipe:[330,719,465,719]}),done(),view('正文已经按新的字号重排，阅读位置由固件保留。')]},
  'reading-font':{title:'选择阅读字体',ids:['reading-font'],steps:[...fontBase(),readerCard('阅读字体',0,6),view('这里列出浏览器虚拟 TF 卡中的字体。退出演示后，可用「导入书籍 / 字体」导入自己的 TTF / OTF；文件不会上传。')]},
  'layout-margins':{title:'调整阅读排版',ids:['layout-margins','paragraph-spacing','indent-fine'],steps:[...fontBase(),readerCard('排版设置',3,3),view('可以调整边距、行距、段距、字距和首行缩进。rc97 的字距支持 -15 到 +15 像素。'),step('完成查看后，点下方按钮返回。',[342,1168],'tap 342 1168',{panel:2})]},
  bookmarks:{title:'添加阅读书签',ids:['bookmarks'],steps:[open(),step('先翻到下一页，选择要标记的位置。',[540,600],'key 2'),tools(),step('点「书签」，保存当前阅读位置。',[205,1140],'tap 205 1140'),view('书签和阅读进度由固件代码保存。演示结束后不会覆盖你自己的体验记录。')]},
  'toc-jump':{title:'打开章节目录',ids:['toc-jump'],steps:[open(true),tools(),step('点「目录」，查看示例 EPUB 的三个章节。',[68,1140],'tap 68 1140',{book:2}),view('目录中可以选择章节。章节较多时，还可以使用分页和目录位置跳转。')]},
  'swipe-pages':{title:'用手势翻页',ids:['swipe-pages','tap-horizontal','tap-vertical'],steps:[open(),step('在正文从右向左滑动，翻到下一页。',[500,620],{swipe:[540,620,140,620]}),view('这里只演示实际页面变化。墨水屏的物理刷新速度和残影需以设备为准。')]},
  'shelf-style':{title:'切换书架样式',ids:['shelf-style'],steps:[settings(),setting('书架样式',4,4),view('rc97 将书架样式放在「个性化功能」分组，可选择深色书轨、亚克力书架和列表书架。')]},
  'shelf-manage':{title:'书架管理与排序',ids:['recent-sort','batch-select','shelf-search','delete-books','unshelf'],steps:[step('点底栏「书架」。',[256,1140],'page 1',{book:0}),step('点右上角「管理」，查看来源、排序和搜索。',[490,120],'tap 490 120',{book:4}),view('排序按钮用于按名称或最近阅读排列；搜索支持书名、拼音和首字母。演示不执行删除。')]},
  'main-refresh':{title:'主页刷新模式',ids:['main-refresh'],steps:[settings(),setting('主页刷新模式',2,8),view('这里可以选择普通、快刷和水波纹模式。浏览器展示布局与操作，物理波形效果以实机为准。')]},
  'system-font':{title:'设置系统字体',ids:['system-font','system-size'],steps:[settings(),setting('系统字体',1,5),view('系统字体与阅读字体分别设置。可以导入自己的字体，再在这个页面选择。')]},
  'auto-lock':{title:'自动休眠锁屏',ids:['auto-lock','sleep-resume'],steps:[settings(),setting('自动休眠锁屏',3,16),view('按需要选择自动锁屏时间；这里展示配置入口，浏览器不会控制你的电脑睡眠。')]},
  'lock-style':{title:'选择锁屏样式',ids:['lock-style','wallpaper-lock','ticket-lock','collage-lock'],steps:[settings(),setting('锁屏样式',5,9),view('可查看票根、壁纸、书架拼贴和透明书封入口。选择所需样式后，短按电源键即可查看锁屏。')]},
  'pin-enable':{title:'设置四位锁屏密码',ids:['pin-enable','pin-change','pin-unlock'],steps:[settings(),setting('锁屏密码',6,21),step('点「设置密码」，进入四位数字密码界面。',[342,440],'tap 342 440',{settings:22}),view('设置时需要输入两次相同密码。这里停在输入界面；退出演示后可以在自己的体验中设置，不会写入真实设备。')]},
  profile:{title:'查看资料卡设置',ids:['profile-name','profile-avatar'],steps:[settings(),setting('资料卡',0,17),view('资料卡可设置姓名、头像和签名。头像文件需放在虚拟 TF 卡中；此演示不改写你的资料。')]},
  signature:{title:'修改状态栏签名',ids:['device-signature','web-signature','ime-case','ime-english','ime-nine','ime-full-pinyin','ime-phrases','ime-candidates','ime-digits','ime-symbols','ime-caret','ime-delete','ime-retype-clear'],steps:[settings(),setting('状态栏签名',7,23),view('这里使用固件原有的输入法。可切换九宫格与全键盘、中英文、大小写、数字和符号；完成后确认保存。')]},
  'time-sync':{title:'日期与时间入口',ids:['time-sync'],steps:[settings(),setting('日期与时间',8,2),view('可查看手动日期与时间设置。真实设备的联网对时需要设备连接 Wi-Fi，网页不执行联网对时。')]},
  bluetooth:{title:'蓝牙翻页器入口',ids:['ble-pair','ble-learn','ble-shelf'],scope:'入口',steps:[settings(),setting('蓝牙翻页器',9,19),view('这里展示开关、配对与按键学习入口。浏览器不会扫描或配对真实蓝牙设备，请按说明书在墨水屏上完成。')]},
  backup:{title:'保存与恢复入口',ids:['config-save','config-restore','backup','restore-config','restore-progress'],scope:'入口',steps:[settings(),setting('保存与恢复',10,12),view('这里展示配置备份入口。请在真实设备上操作自己的 TF 卡；演示不会恢复或覆盖你的配置。')]},
  files:{title:'导入与管理文件',ids:['wifi-transfer','hotspot-transfer','usb-transfer','web-fonts','web-any-file','rescan'],scope:'入口',steps:[step('点「文件管理」，查看虚拟 TF 卡。',[428,1140],'page 2',{page:'文件管理'}),view('浏览器体验可通过下方「导入书籍 / 字体」导入本地副本。Wi-Fi、热点和 USB 传书请在真实设备上完成。')]},
  'home-component':{title:'切换首页组件',ids:['home-component','home-clock'],steps:[home(),step('长按首页左下方组件，进入样式选择。',[180,800],{hold:[180,800]},{home:1}),step('点右箭头，浏览下一个组件样式。',[616,504],'tap 616 504',{home:1}),view('左右滑动或用箭头选择，点下方按钮确认。支持最近阅读书夹和多种时钟。')]},
  'reading-goal':{title:'阅读记录与每日目标',ids:['reading-goal','home-goal','reading-records'],steps:[home(),step('点月阅读记录，进入目标与记录页面。',[500,800],'tap 500 800',{home:2}),view('上方显示月阅读记录，下方用加减控件设置每日阅读分钟数，确认保存后返回。')]}
};
export const byTopic=Object.fromEntries(Object.entries(tours).flatMap(([key,tour])=>tour.ids.map(id=>[id,key])));
