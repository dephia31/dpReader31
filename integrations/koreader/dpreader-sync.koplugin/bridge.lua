-- SPDX-License-Identifier: Apache-2.0
-- Independent bridge: CRE positions are resolved by KOReader, never synthesized
-- from firmware byte offsets. Unresolved/conflicting records remain in transport.
local Bridge = {}
function Bridge.exchangePath(book)
    -- Search nearest ancestor first; the mirrored relative book path includes
    -- extension and parent folders, matching the firmware's central layout.
    local ancestor=book:match("^(.*)/[^/]+$") or "."
    while ancestor and ancestor~="" do
        local relative=book:sub(ancestor=="/" and 2 or #ancestor+2)
        local candidate=(ancestor=="/" and "" or ancestor).."/dpReader_note/"..relative.."/dpReader.annotations.json"
        local f=io.open(candidate,"rb") or io.open(candidate..".old","rb")
        if f then f:close();return candidate end
        if ancestor=="/" then break end
        ancestor=ancestor:match("^(.*)/[^/]+$")
        if ancestor=="" then ancestor="/" end
    end
    return (book:gsub("%.[^./]+$",""))..".sdr/kiikoread.annotations.json"
end
local function find_owned(items, id)
    for i, a in ipairs(items) do if a.kiikoread_sync_id == id then return a, i end end
end
local function note(a) return a.note or "" end
local function same_text(document, a, text)
    if type(a.pos0)~="string" or type(a.pos1)~="string" then return false end
    if not document:isXPointerInDocument(a.pos0) or not document:isXPointerInDocument(a.pos1) then return false end
    return document:getTextFromXPointers(a.pos0,a.pos1)==text
end
local function chapter_index(catalog, title)
    if #(catalog or {})==1 and (title==nil or title=="") then return 0 end
    local index
    for i, name in ipairs(catalog or {}) do
        if name == title then if index then return nil end; index=i-1 end
    end
    return index
end
local function resolve(ui, e)
    local d=ui.document
    if e.koreader and same_text(d,e.koreader,e.text) then return e.koreader.pos0,e.koreader.pos1 end
    -- Cap hits and reject ambiguous matches; never pick an arbitrary occurrence.
    local hits=d:findAllText(e.text,false,0,2,false,1) or {}
    if #hits~=1 or not hits[1].start or not hits[1]["end"] then return end
    local a={pos0=hits[1].start,pos1=hits[1]["end"]}
    if same_text(d,a,e.text) then return a.pos0,a.pos1 end
end
function Bridge.sync(ui, doc)
    assert(doc.schema=="kiikoread-koreader-annotations" and doc.version==1,"Unsupported exchange schema")
    assert(type(doc.annotations)=="table" and #doc.annotations<=128,"Exchange exceeds 128 records")
    assert(ui.rolling and ui.annotation and ui.document.findAllText,"EPUB/TXT CRE documents only")
    local items=ui.annotation.annotations
    local stats={imported=0,exported=0,deleted=0,unresolved=0,conflicts=0}
    local seen={}
    for _,e in ipairs(doc.annotations) do
        assert(type(e.id)=="string" and type(e.text)=="string" and type(e.note)=="string","Invalid exchange record")
        assert(not seen[e.id],"Duplicate annotation identifier");seen[e.id]=true
    end
    for _, e in ipairs(doc.annotations) do
        assert(type(e.id)=="string" and type(e.text)=="string" and type(e.note)=="string","Invalid exchange record")
        local a,index=find_owned(items,e.id)
        seen[e.id]=true
        if e.conflict and a and note(a)==e.note and a.text==e.text and not e.deleted then e.conflict=nil end
        if e.conflict then
            stats.conflicts=stats.conflicts+1
        elseif a then
            local local_changed=note(a)~=(a.kiikoread_sync_note or "") or a.text~=(a.kiikoread_sync_text or a.text)
            local remote_changed=e.note~=(a.kiikoread_sync_note or "") or e.text~=(a.kiikoread_sync_text or a.text)
            if local_changed and (e.deleted or (remote_changed and (note(a)~=e.note or a.text~=e.text))) then
                e.conflict={koreader_note=note(a),koreader_text=a.text};stats.conflicts=stats.conflicts+1
            elseif e.deleted then
                table.remove(items,index);stats.deleted=stats.deleted+1
            elseif local_changed then
                -- Keep the user's CRE selection if it still matches its quoted text.
                if same_text(ui.document,a,a.text) then
                    e.note=note(a);e.text=a.text;e.source="koreader";e.conflict=nil
                    a.kiikoread_sync_note=e.note;a.kiikoread_sync_text=e.text;stats.exported=stats.exported+1
                else stats.unresolved=stats.unresolved+1 end
            else
                local p0,p1=resolve(ui,e)
                if p0 then
                    a.page=p0;a.pos0=p0;a.pos1=p1;a.text=e.text;a.note=e.note~="" and e.note or nil
                    a.drawer=e.drawer or "underscore";a.datetime_updated=e.datetime_updated or os.date("%Y-%m-%d %H:%M:%S")
                    a.kiikoread_sync_note=e.note;a.kiikoread_sync_text=e.text;e.conflict=nil;stats.imported=stats.imported+1
                else stats.unresolved=stats.unresolved+1 end
            end
        elseif e.koreader then
            -- A previously linked record absent from KOReader is a deletion.
            if not e.deleted and e.note~=(e.koreader.last_note or "") then
                e.conflict={koreader_deleted=true};stats.conflicts=stats.conflicts+1
            else e.deleted=true;e.source="koreader" end
        elseif not e.deleted then
            local p0,p1=resolve(ui,e)
            if p0 then
                a={page=p0,pos0=p0,pos1=p1,text=e.text,note=e.note~="" and e.note or nil,
                    drawer=e.drawer or "underscore",chapter=ui.toc:getTocTitleByPage(p0),
                    datetime=e.datetime or os.date("%Y-%m-%d %H:%M:%S"),kiikoread_sync_id=e.id,
                    kiikoread_sync_note=e.note,kiikoread_sync_text=e.text}
                ui.annotation:addItem(a);stats.imported=stats.imported+1
            else stats.unresolved=stats.unresolved+1 end
        end
        a=find_owned(items,e.id)
        if a and not e.conflict and not e.deleted then
            e.koreader={page=a.page,pos0=a.pos0,pos1=a.pos1,chapter=a.chapter,last_note=note(a)}
            e.datetime=a.datetime;e.datetime_updated=a.datetime_updated
        end
    end
    -- Export ordinary native KOReader annotations, without touching unrelated ones.
    for _, a in ipairs(items) do
        if a.drawer and a.text and a.text~="" and not a.kiikoread_sync_id then
            local chapter=chapter_index(doc.chapters,a.chapter)
            if chapter==nil or not same_text(ui.document,a,a.text) or #doc.annotations>=128 then
                stats.unresolved=stats.unresolved+1
            else
                local id="ko-"..tostring(a.datetime or os.time()).."-"..a.pos0.."-"..a.pos1
                a.kiikoread_sync_id=id;a.kiikoread_sync_note=note(a);a.kiikoread_sync_text=a.text
                table.insert(doc.annotations,{id=id,text=a.text,note=note(a),drawer=a.drawer,
                    datetime=a.datetime,datetime_updated=a.datetime_updated,chapter=a.chapter,
                    source="koreader",deleted=false,kiikoread={chapter=chapter},
                    koreader={page=a.page,pos0=a.pos0,pos1=a.pos1,chapter=a.chapter,last_note=note(a)}})
                stats.exported=stats.exported+1
            end
        end
    end
    ui.annotation:sortItems(items)
    return stats
end
return Bridge
