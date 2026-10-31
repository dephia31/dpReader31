-- SPDX-License-Identifier: Apache-2.0
local WidgetContainer=require("ui/widget/container/widgetcontainer")
local InfoMessage=require("ui/widget/infomessage")
local UIManager=require("ui/uimanager")
local JSON=require("json")
local util=require("util")
local Bridge=require("bridge")
local Sync=WidgetContainer:extend{name="kiikoread_sync",is_doc_only=true}
function Sync:init() self.ui.menu:registerToMainMenu(self) end
function Sync:addToMainMenu(menu)
    menu.kiikoread_sync={text="dpReader: đồng bộ ghi chú",callback=function() self:sync() end}
end
function Sync:sync()
    local function run()
        local book=self.ui.document.file
        local path=Bridge.exchangePath(book)
        local function read(file)
            local f=io.open(file,"rb");if not f then return end
            local raw=f:read(131073);f:close();assert(#raw<=131072,"Tệp vượt 128 KiB")
            local ok,value=pcall(JSON.decode,raw);if ok then return value end
        end
        local doc=read(path);local primary_valid=doc~=nil
        if not doc then doc=read(path..".old") end
        assert(doc,"Mở sách trên dpReader trước, rồi chép thư mục dpReader_note ở gốc thẻ cùng sách")
        assert(doc.schema=="kiikoread-koreader-annotations" and doc.version==1,"Định dạng chưa hỗ trợ; giữ nguyên tệp")
        local filename=book:match("[^/]+$")
        assert(doc.book==filename,"Tệp trao đổi thuộc sách khác")
        -- Work on copies: no in-memory annotation changes before transport is durable.
        local original=self.ui.annotation.annotations
        local working=util.tableDeepCopy(original)
        self.ui.annotation.annotations=working
        local ok,stats=pcall(Bridge.sync,self.ui,doc)
        self.ui.annotation.annotations=original
        assert(ok,stats)
        local encoded=JSON.encode(doc);assert(#encoded<=131072,"Tệp vượt 128 KiB")
        local tmp=path..".tmp";local out=assert(io.open(tmp,"wb"))
        local wrote,err=out:write(encoded);local flushed=out:flush();local closed=out:close()
        assert(wrote and flushed and closed,err or "Không ghi được tệp trao đổi")
        -- POSIX rename replaces atomically; on FAT keep the previous copy first.
        local old=path..".old"
        if primary_valid then
            local removed,why,code=os.remove(old);assert(removed or code==2,why)
            assert(os.rename(path,old))
        else os.remove(path) end
        assert(os.rename(tmp,path))
        self.ui.annotation.annotations=working
        local saved,why=pcall(function()
            self.ui.annotation:onSaveSettings()
            assert(self.ui.doc_settings:flush(),"KOReader không ghi được metadata")
        end)
        if not saved then
            self.ui.annotation.annotations=original
            self.ui.doc_settings:saveSetting("annotations",original)
            local removed=os.remove(path)
            local restored=removed and os.rename(old,path)
            error(tostring(why)..(restored and "; đã phục hồi tệp trao đổi cũ" or "; giữ bản .old để phục hồi"))
        end
        UIManager:setDirty(self.ui.dialog,"ui")
        return string.format("Nhập %d · Xuất %d · Xóa %d\nChưa gắn vị trí: %d · Xung đột: %d",stats.imported,stats.exported,stats.deleted,stats.unresolved,stats.conflicts)
    end
    local ok,result=pcall(run)
    UIManager:show(InfoMessage:new{text=ok and result or ("Chưa đồng bộ: "..tostring(result))})
end
return Sync
