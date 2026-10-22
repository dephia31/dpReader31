import Cocoa
import UniformTypeIdentifiers

final class ScreenView: NSView {
    var image: NSImage?
    var action: ((String) -> Void)?
    var dragging = false
    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }
    override func draw(_ dirtyRect: NSRect) {
        NSColor(calibratedWhite: 0.94, alpha: 1).setFill(); bounds.fill()
        NSGraphicsContext.current?.imageInterpolation = .high
        image?.draw(in: bounds, from: .zero, operation: .copy, fraction: 1, respectFlipped: true, hints: nil)
    }
    private func point(_ e: NSEvent) -> (Int, Int) {
        let p = convert(e.locationInWindow, from: nil)
        return (max(0,min(683,Int(p.x/bounds.width*684))),max(0,min(1215,Int(p.y/bounds.height*1216))))
    }
    override func mouseDown(with e: NSEvent) { window?.makeFirstResponder(self); dragging=true; let (x,y)=point(e);action?("down \(x) \(y)") }
    override func mouseDragged(with e: NSEvent) { let (x,y)=point(e);action?("move \(x) \(y)") }
    override func mouseUp(with e: NSEvent) { dragging=false;let (x,y)=point(e);action?("up \(x) \(y)") }
    override func scrollWheel(with e: NSEvent) {
        let (x,y)=point(e); let end = max(30,min(1180,y+(e.scrollingDeltaY>0 ? -180:180)))
        action?("down \(x) \(y)"); action?("move \(x) \(end)"); action?("up \(x) \(end)")
    }
    override func keyDown(with e: NSEvent) {
        switch e.keyCode { case 123: action?("key 0");case 124:action?("key 2");case 49,36:action?("key 1");case 53:action?("page 0");default:super.keyDown(with:e) }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    var window: NSWindow!
    let screen = ScreenView()
    let status = NSTextField(labelWithString:"正在启动…")
    let booksStack = NSStackView()
    let homeStyle = NSSegmentedControl(labels:["rc97 正式首页"],trackingMode:.selectOne,target:nil,action:nil)
    let process = Process()
    let input = Pipe(), output = Pipe()
    let ioQueue = DispatchQueue(label:"pico.engine.io")
    var buffer = Data()
    var timer: Timer?
    var framePath: String?
    var dataURL: URL!
    var library:[URL]=[]
    var launchRoot: URL { Bundle.main.bundleURL.deletingLastPathComponent() }

    func label(_ text: String, _ size: CGFloat = 13, _ weight: NSFont.Weight = .regular) -> NSTextField {
        let f=NSTextField(labelWithString:text);f.font=NSFont.systemFont(ofSize:size,weight:weight);f.textColor = .labelColor;return f
    }
    func button(_ text:String,_ selector:Selector,_ tag:Int = 0) -> NSButton {
        let b=NSButton(title:text,target:self,action:selector);b.bezelStyle = .rounded;b.tag=tag;b.controlSize = .large;return b
    }
    func row(_ views:[NSView],spacing:CGFloat = 8) -> NSStackView {
        let s=NSStackView(views:views);s.orientation = .horizontal;s.spacing=spacing;s.alignment = .centerY;return s
    }
    func line() -> NSBox { let b=NSBox();b.boxType = .separator;return b }
    func applicationDidFinishLaunching(_ note: Notification) {
        dataURL=launchRoot.appendingPathComponent("Data",isDirectory:true)
        for p in ["card/books","card/fonts","card/pictures","flash","nvs"] { try? FileManager.default.createDirectory(at:dataURL.appendingPathComponent(p),withIntermediateDirectories:true) }
        NSApp.setActivationPolicy(.regular)
        createMenu();createWindow();loadLibrary();startEngine()
        window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
        if CommandLine.arguments.contains("--bookshelf") { DispatchQueue.main.asyncAfter(deadline:.now()+1){[weak self] in self?.send("page 1") } }
        timer=Timer.scheduledTimer(withTimeInterval:0.7,repeats:true){[weak self] _ in if self?.screen.dragging != true { self?.send("tick") } }
        if CommandLine.arguments.contains("--snapshot") { DispatchQueue.main.asyncAfter(deadline:.now()+5){[weak self] in self?.snapshotWindow() } }
    }
    func createMenu() {
        let menu=NSMenu();let item=NSMenuItem();menu.addItem(item);let appMenu=NSMenu();item.submenu=appMenu
        appMenu.addItem(withTitle:"关于 Pico 模拟器",action:#selector(about),keyEquivalent:"")
        appMenu.addItem(.separator());appMenu.addItem(withTitle:"退出 Pico 模拟器",action:#selector(NSApplication.terminate(_:)),keyEquivalent:"q")
        let edit=NSMenuItem();edit.title="编辑";let editMenu=NSMenu(title:"编辑");edit.submenu=editMenu;menu.addItem(edit)
        for (title,selector,key) in [("复制",#selector(NSText.copy(_:)),"c"),("粘贴",#selector(NSText.paste(_:)),"v"),("全选",#selector(NSText.selectAll(_:)),"a")] { editMenu.addItem(withTitle:title,action:selector,keyEquivalent:key) }
        NSApp.mainMenu=menu
    }
    func createWindow() {
        window=NSWindow(contentRect:NSRect(x:0,y:0,width:920,height:830),styleMask:[.titled,.closable,.miniaturizable],backing:.buffered,defer:false)
        window.title="Pico · Mac 模拟器";window.center();window.isReleasedWhenClosed=false
        let root=NSView();root.wantsLayer=true;root.layer?.backgroundColor=NSColor.windowBackgroundColor.cgColor;window.contentView=root
        let columns=NSStackView();columns.orientation = .horizontal;columns.alignment = .top;columns.spacing=30;columns.translatesAutoresizingMaskIntoConstraints=false;root.addSubview(columns)
        NSLayoutConstraint.activate([columns.leadingAnchor.constraint(equalTo:root.leadingAnchor,constant:28),columns.trailingAnchor.constraint(equalTo:root.trailingAnchor,constant:-28),columns.topAnchor.constraint(equalTo:root.topAnchor,constant:24),columns.bottomAnchor.constraint(equalTo:root.bottomAnchor,constant:-24)])
        let left=NSStackView();left.orientation = .vertical;left.alignment = .centerX;left.spacing=12
        let device=NSView();device.wantsLayer=true;device.layer?.backgroundColor=NSColor(calibratedWhite:0.80,alpha:1).cgColor;device.layer?.cornerRadius=18;device.layer?.borderWidth=1;device.layer?.borderColor=NSColor(calibratedWhite:0.65,alpha:1).cgColor
        screen.translatesAutoresizingMaskIntoConstraints=false;screen.wantsLayer=true;screen.layer?.borderWidth=1;screen.layer?.borderColor=NSColor(calibratedWhite:0.55,alpha:1).cgColor;device.addSubview(screen)
        screen.action = {[weak self] command in self?.send(command)}
        NSLayoutConstraint.activate([device.widthAnchor.constraint(equalToConstant:418),device.heightAnchor.constraint(equalToConstant:732),screen.widthAnchor.constraint(equalToConstant:380),screen.heightAnchor.constraint(equalToConstant:380*1216/684),screen.topAnchor.constraint(equalTo:device.topAnchor,constant:16),screen.centerXAnchor.constraint(equalTo:device.centerXAnchor)])
        let keys=row([button("‹",#selector(key),0),button("○",#selector(key),1),button("›",#selector(key),2)],spacing:42);keys.translatesAutoresizingMaskIntoConstraints=false;device.addSubview(keys)
        NSLayoutConstraint.activate([keys.centerXAnchor.constraint(equalTo:device.centerXAnchor),keys.bottomAnchor.constraint(equalTo:device.bottomAnchor,constant:-10)])
        left.addArrangedSubview(device)
        status.font=NSFont.systemFont(ofSize:12);status.textColor = .secondaryLabelColor;left.addArrangedSubview(status)
        columns.addArrangedSubview(left)
        let right=NSStackView();right.orientation = .vertical;right.alignment = .leading;right.spacing=14;right.widthAnchor.constraint(equalToConstant:400).isActive=true;columns.addArrangedSubview(right)
        right.addArrangedSubview(label("Pico",30,.semibold));right.addArrangedSubview(label("固件源码模拟 · 0.3.3-rc97",13));right.addArrangedSubview(label("684 × 1216 · 16 级灰阶",12))
        right.addArrangedSubview(line())
        let tabs=row([button("首页",#selector(tab),0),button("书架",#selector(tab),1),button("文件",#selector(tab),2),button("设置",#selector(tab),3)],spacing:6);right.addArrangedSubview(tabs)
        homeStyle.target=self;homeStyle.action=#selector(homeCompare);homeStyle.selectedSegment=0;right.addArrangedSubview(homeStyle)
        right.addArrangedSubview(row([button("电源 / 锁屏",#selector(power)),button("下拉菜单",#selector(quick)),button("全刷",#selector(refresh))],spacing:6))
        right.addArrangedSubview(line())
        right.addArrangedSubview(label("本地书库",16,.semibold))
        right.addArrangedSubview(row([button("＋ 导入书籍",#selector(importBooks)),button("＋ 导入字体",#selector(importFonts))]))
        let scroll=NSScrollView();scroll.hasVerticalScroller=true;scroll.drawsBackground=false;scroll.borderType = .noBorder;scroll.heightAnchor.constraint(equalToConstant:220).isActive=true;scroll.widthAnchor.constraint(equalToConstant:395).isActive=true
        booksStack.orientation = .vertical;booksStack.alignment = .leading;booksStack.spacing=7;booksStack.translatesAutoresizingMaskIntoConstraints=false
        let document=NSView();document.translatesAutoresizingMaskIntoConstraints=false;document.addSubview(booksStack);scroll.documentView=document
        NSLayoutConstraint.activate([document.widthAnchor.constraint(equalTo:scroll.contentView.widthAnchor),booksStack.leadingAnchor.constraint(equalTo:document.leadingAnchor),booksStack.trailingAnchor.constraint(equalTo:document.trailingAnchor),booksStack.topAnchor.constraint(equalTo:document.topAnchor),booksStack.bottomAnchor.constraint(equalTo:document.bottomAnchor)])
        right.addArrangedSubview(scroll)
        right.addArrangedSubview(line())
        right.addArrangedSubview(row([button("保存当前画面",#selector(exportFrame)),button("打开数据目录",#selector(showData))]))
        let help=label("在屏幕内点击或拖动，操作真实页面。\n← / → 翻页，空格打开阅读工具栏。\n从屏幕顶边向下拖动，可打开快捷菜单。",12);help.maximumNumberOfLines=4;help.textColor = .secondaryLabelColor;right.addArrangedSubview(help)
        let boundary=label("本版使用固件界面与阅读代码。\n电池、Wi-Fi、蓝牙为模拟状态。\n设备 BIN 刷入及真实屏幕波形尚未接入。",12);boundary.maximumNumberOfLines=4;boundary.textColor = .secondaryLabelColor;right.addArrangedSubview(boundary)
    }
    func startEngine() {
        process.executableURL=Bundle.main.url(forResource:"pico-engine",withExtension:nil)
        process.currentDirectoryURL=launchRoot
        var env=ProcessInfo.processInfo.environment;env["PICO_SIM_DATA"]=dataURL.path;process.environment=env
        process.standardInput=input;process.standardOutput=output
        let logURL=dataURL.appendingPathComponent("engine.log");FileManager.default.createFile(atPath:logURL.path,contents:nil);process.standardError=try? FileHandle(forWritingTo:logURL)
        output.fileHandleForReading.readabilityHandler={[weak self] handle in
            let data=handle.availableData;if data.isEmpty { return }
            self?.ioQueue.async { [weak self] in
                guard let self=self else{return};self.buffer.append(data)
                while let end=self.buffer.firstIndex(of:10){let line=self.buffer.prefix(upTo:end);self.buffer.removeSubrange(...end)
                    guard let json=try? JSONSerialization.jsonObject(with:line) as? [String:Any],let path=json["frame"] as? String else{continue}
                    DispatchQueue.main.async { self.framePath=path;self.screen.image=NSImage(contentsOfFile:path);self.screen.needsDisplay=true
                        self.homeStyle.selectedSegment=0
                        let page=json["page"] as? String ?? "Pico";let locked=json["locked"] as? Bool ?? false;self.status.stringValue="\(page) · \(locked ? "已锁屏":"运行中")"
                    }
                }
            }
        }
        process.terminationHandler={[weak self] _ in DispatchQueue.main.async{self?.status.stringValue="模拟器已停止，请重新打开"}}
        do {try process.run()}catch{alert("启动失败",error.localizedDescription)}
    }
    func send(_ command:String){guard process.isRunning else{return};ioQueue.async{[weak self] in guard let self=self else{return};try? self.input.fileHandleForWriting.write(contentsOf:Data((command+"\n").utf8))}}
    func loadLibrary(){library=(try? FileManager.default.contentsOfDirectory(at:dataURL.appendingPathComponent("card/books"),includingPropertiesForKeys:nil))?.filter{["txt","epub"].contains($0.pathExtension.lowercased())}.sorted{$0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending} ?? []
        for v in booksStack.arrangedSubviews{booksStack.removeArrangedSubview(v);v.removeFromSuperview()}
        for (i,url) in library.enumerated(){let name=url.deletingPathExtension().lastPathComponent;let b=NSButton(title:name,target:self,action:#selector(openBook));b.tag=i;b.bezelStyle = .recessed;b.contentTintColor = .labelColor;b.alignment = .left;b.lineBreakMode = .byTruncatingTail;b.font = .systemFont(ofSize:13);b.toolTip=name;b.widthAnchor.constraint(equalToConstant:380).isActive=true;booksStack.addArrangedSubview(b)}
    }
    @objc func homeCompare(_ control:NSSegmentedControl){send("home \(control.selectedSegment)")}
    @objc func tab(_ b:NSButton){send("page \(b.tag)")}
    @objc func key(_ b:NSButton){send("key \(b.tag)")}
    @objc func power(){send("power")}
    @objc func quick(){send("quick")}
    @objc func refresh(){send("refresh")}
    @objc func openBook(_ b:NSButton){guard b.tag<library.count else{return};send("open /sdcard/books/\(library[b.tag].lastPathComponent)")}
    @objc func importBooks(){importFiles(fonts:false)}
    @objc func importFonts(){importFiles(fonts:true)}
    func importFiles(fonts:Bool){let panel=NSOpenPanel();panel.allowsMultipleSelection=true;panel.canChooseDirectories=false;panel.allowedContentTypes=(fonts ? ["ttf","otf"]:["epub","txt"]).compactMap{UTType(filenameExtension:$0)};panel.message=fonts ? "选择要在模拟器中使用的字体":"导入书籍副本到模拟器";panel.beginSheetModal(for:window){[weak self] result in guard result == .OK,let self=self else{return};var first:String?
        for url in panel.urls{let destination=self.dataURL.appendingPathComponent(fonts ? "card/fonts":"card/books").appendingPathComponent(url.lastPathComponent)
            do{if !FileManager.default.fileExists(atPath:destination.path){try FileManager.default.copyItem(at:url,to:destination)};if first==nil{first=url.lastPathComponent}}catch{self.alert("导入失败",error.localizedDescription)}
        }
        if fonts,let first=first{self.send("font /sdcard/fonts/\(first)")}else{self.loadLibrary();self.send("rescan")}
    }}
    @objc func showData(){NSWorkspace.shared.open(dataURL)}
    @objc func exportFrame(){guard let frame=framePath else{return};let panel=NSSavePanel();panel.nameFieldStringValue="Pico-\(Int(Date().timeIntervalSince1970)).png";panel.allowedContentTypes=[.png];panel.beginSheetModal(for:window){[weak self] result in if result == .OK,let url=panel.url{do{let data=try Data(contentsOf:URL(fileURLWithPath:frame));try data.write(to:url,options:.atomic)}catch{self?.alert("保存失败",error.localizedDescription)}}}}
    @objc func about(){alert("Pico Mac 模拟器","基于 0.3.3-rc97 正式版源码的本地模拟版。\n界面、字体、阅读排版与书签使用固件代码。\n不会刷写真实设备；本版不执行 ESP32 BIN。")}
    func alert(_ title:String,_ message:String){let a=NSAlert();a.messageText=title;a.informativeText=message;a.beginSheetModal(for:window)}
    func snapshotWindow(){guard let view=window.contentView,let rep=view.bitmapImageRepForCachingDisplay(in:view.bounds) else{return};view.cacheDisplay(in:view.bounds,to:rep);if let png=rep.representation(using:.png,properties:[:]){try? png.write(to:dataURL.appendingPathComponent("mac-preview.png"))}}
    func applicationShouldTerminateAfterLastWindowClosed(_ sender:NSApplication)->Bool{true}
    func applicationWillTerminate(_ notification:Notification){timer?.invalidate();output.fileHandleForReading.readabilityHandler=nil;if process.isRunning{try? input.fileHandleForWriting.write(contentsOf:Data("quit\n".utf8));DispatchQueue.global().asyncAfter(deadline:.now()+1){[weak self] in if self?.process.isRunning == true{self?.process.terminate()}}}}
}
let application=NSApplication.shared
let delegate=AppDelegate()
application.delegate=delegate
application.run()
