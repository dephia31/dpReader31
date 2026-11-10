// SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
// 中文：使用真实SDK NVS存储与RAM模拟NOR验证扩容。/ English: Test real SDK NVS storage against RAM-backed NOR.
#include "nvs_storage.hpp"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <fstream>
#include <zlib.h>
extern "C" uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t *data, uint32_t len) { return crc32(crc, data, len); }
class MemoryPartition : public nvs::Partition {
public:
    std::vector<uint8_t> bytes;
    explicit MemoryPartition(size_t size) : bytes(size, 255) {}
    const char *get_partition_name() override { return "nvs"; }
    esp_err_t read_raw(size_t at, void *dst, size_t size) override { assert(at + size <= bytes.size()); memcpy(dst, bytes.data()+at, size); return ESP_OK; }
    esp_err_t read(size_t at, void *dst, size_t size) override { return read_raw(at,dst,size); }
    esp_err_t write_raw(size_t at, const void *src, size_t size) override {
        assert(at + size <= bytes.size());
        for(size_t i=0;i<size;i++) bytes[at+i] &= static_cast<const uint8_t*>(src)[i];
        return ESP_OK;
    }
    esp_err_t write(size_t at, const void *src, size_t size) override { return write_raw(at,src,size); }
    esp_err_t erase_range(size_t at,size_t size) override { assert(at+size<=bytes.size()); memset(bytes.data()+at,255,size); return ESP_OK; }
    uint32_t get_address() override { return 0xfb0000; }
    uint32_t get_size() override { return bytes.size(); }
    bool get_readonly() override { return false; }
};
static void ok(esp_err_t err) { if(err != ESP_OK) { fprintf(stderr,"NVS err=%x\n",err); abort(); } }
int main(int argc, char **argv) {
    MemoryPartition p(0x4000);
    const char* names[] = {"metalio_s31","nvs.net80211","nimble_bond"};
    uint8_t ns[3];
    {
        nvs::Storage s(&p); ok(s.init(0,4));
        for(unsigned i=0;i<3;i++) { ok(s.createOrOpenNamespace(names[i],true,ns[i])); uint32_t v=100+i; ok(s.writeItem(ns[i],"value",v,false)); }
        std::vector<uint8_t> blob(4500,0x73);
        ok(s.writeItem(ns[2],nvs::ItemType::BLOB,"bond",blob.data(),blob.size(),false));
    }
    if(argc>1) { std::ofstream f(argv[1],std::ios::binary); f.write((char*)p.bytes.data(),p.bytes.size()); }
    p.bytes.resize(0x40000,255);
    {
        nvs::Storage s(&p); ok(s.init(0,64));
        for(unsigned i=0;i<3;i++) { uint8_t actual; ok(s.createOrOpenNamespace(names[i],false,actual)); assert(actual==ns[i]); uint32_t v=0;ok(s.readItem(ns[i],"value",v));assert(v==100+i); }
        std::vector<uint8_t> blob(4500);ok(s.readItem(ns[2],nvs::ItemType::BLOB,"bond",blob.data(),blob.size())); assert(blob==std::vector<uint8_t>(4500,0x73));
        uint8_t stress;ok(s.createOrOpenNamespace("stress",true,stress));
        unsigned count=0; char key[16];
        for(;;count++) { snprintf(key,sizeof(key),"k%u",count); uint32_t v=count; auto err=s.writeItem(stress,key,v,false); if(err==ESP_ERR_NVS_NOT_ENOUGH_SPACE)break;ok(err);assert(count<10000); }
        assert(count>7000);
        nvs_stats_t stats={};ok(s.fillStats(stats));
        printf("SDK NVS expanded 16->256KiB; %u scalar keys until full; used=%zu free=%zu total=%zu\n",count,stats.used_entries,stats.free_entries,stats.total_entries);
        for(unsigned i=0;i<count;i+=2) {snprintf(key,sizeof(key),"k%u",i);ok(s.eraseItem(stress,key,false));}
        for(unsigned round=0;round<4;round++) for(unsigned i=0;i<1000;i++) {snprintf(key,sizeof(key),"k%u",i*2);uint32_t v=round*1000+i;ok(s.writeItem(stress,key,v,false));}
        uint32_t v;ok(s.readItem(ns[0],"value",v));assert(v==100);
        puts("erase/rewrite/GC passed; original namespaces retained");
    }
    {nvs::Storage s(&p);ok(s.init(0,64));uint32_t v;ok(s.readItem(ns[1],"value",v));assert(v==101);puts("reopen after stress passed");}
}
