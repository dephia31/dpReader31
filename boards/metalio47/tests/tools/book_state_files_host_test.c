/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#define main legacy_storage_main
#include "book_storage_host_test.c"
#undef main
#include "../main/book/book_state_store.c"
extern esp_err_t nvs_progress_save(const char *, const book_progress_t *);
static unsigned visits;
static bool visit(const char *p,const book_progress_t *v,void *ctx){(void)p;(void)ctx;assert(v->px==32);++visits;return true;}
int main(int argc,char **argv) {
    (void)argv;
    const char *a="/sdcard/books/a.epub",*b="/sdcard/books/b.epub",*c="/flash/books/c.epub";
    book_progress_t v={.file_size=900,.chapter=3,.byte_off=420,.px=32,.pct=40},out;
    char last_path[288];
    if(argc>1) {
        assert(book_progress_load(a,900,&out)&&out.byte_off==420);
        assert(book_progress_load(b,900,&out)&&out.byte_off==840);
        assert(book_progress_last_path(last_path,sizeof(last_path))&&!strcmp(last_path,b));
        assert(book_progress_reset_files()==ESP_OK);
        assert(!book_progress_load(a,900,&out));
        visits=0;assert(book_progress_list(visit,NULL)==ESP_OK&&visits==0);
        puts("PASS persisted per-book progress and reset across process restart");return 0;
    }
    assert(mkdir(BOOK_STATE_SD_ROOT,0775)==0);assert(mkdir(BOOK_STATE_FLASH_ROOT,0775)==0);
    assert(nvs_progress_save(a,&v)==ESP_OK);
    assert(book_progress_load(a,900,&out)&&out.byte_off==420);
    blob_failure=seq_failure=last_failure=ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    int before=commits;
    assert(book_progress_save(a,&v)==ESP_OK);
    v.byte_off=840;assert(book_progress_save(a,&v)==ESP_OK);
    /* Truncate newest slot: previous verified progress survives. */
    char base[376],file[384];assert(record_base(BOOK_STATE_SD_ROOT,a,"progress",base,sizeof(base)));
    snprintf(file,sizeof(file),"%s.0",base);FILE *f=fopen(file,"wb");assert(f);fputs("torn",f);fclose(f);
    assert(book_progress_load(a,900,&out)&&out.byte_off==420);
    assert(book_progress_save(b,&v)==ESP_OK);
    assert(book_progress_save(c,&v)==ESP_OK);
    assert(book_progress_set_last_path(c)==ESP_OK);
    assert(book_progress_last_path(last_path,sizeof(last_path))&&!strcmp(last_path,c));
    assert(book_progress_set_last_path(b)==ESP_OK);
    assert(book_progress_last_path(last_path,sizeof(last_path))&&!strcmp(last_path,b));
    visits=0;assert(book_progress_list(visit,NULL)==ESP_OK&&visits==3);
    assert(commits==before);
    assert(book_progress_forget(c)==ESP_OK&&!book_progress_load(c,900,&out));
    assert(rename(BOOK_STATE_SD_ROOT,"removed-card")==0);
    assert(book_progress_save(b,&v)!=ESP_OK);
    assert(rename("removed-card",BOOK_STATE_SD_ROOT)==0);
    assert(book_progress_load(b,900,&out)&&out.byte_off==840);
    assert(!book_progress_load(b,901,&out));
    /* A valid hash collision must never be overwritten. */
    assert(book_state_write(BOOK_STATE_SD_ROOT,a,"profile","old",4)==ESP_OK);
    assert(record_base(BOOK_STATE_SD_ROOT,a,"profile",base,sizeof(base)));
    snprintf(file,sizeof(file),"%s.1",base);f=fopen(file,"r+b");assert(f);
    state_header_t h;assert(fread(&h,1,sizeof(h),f)==sizeof(h));strcpy(h.key,b);h.checksum=0;
    h.checksum=hash_bytes(hash_bytes(2166136261u,&h,sizeof(h)),"old",4);
    rewind(f);assert(fwrite(&h,1,sizeof(h),f)==sizeof(h));fclose(f);
    assert(book_state_write(BOOK_STATE_SD_ROOT,a,"profile","new",4)==ESP_ERR_INVALID_STATE);
    assert(remove(file)==0);
    puts("PASS legacy migration, NVS-full isolation, torn-write recovery, media loss, collision and per-book list");
    return 0;
}
