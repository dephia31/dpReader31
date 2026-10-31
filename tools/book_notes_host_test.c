/* SPDX-License-Identifier: Apache-2.0 */
#include "book_annotations.h"
#include "book_dictionary.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static book_annotations_t a,b;
static book_dictionary_t d;
static void put32(FILE *f,unsigned n){unsigned char x[4]={n>>24,n>>16,n>>8,n};assert(fwrite(x,1,4,f)==4);}
static void fixture(const char *root) {
    char path[640];snprintf(path,sizeof(path),"%s/test.dict",root);FILE *f=fopen(path,"wb");assert(f);fputs("A fruitQuả táo",f);fclose(f);
    snprintf(path,sizeof(path),"%s/test.idx",root);f=fopen(path,"wb");assert(f);fwrite("apple",1,6,f);put32(f,0);put32(f,7);fwrite("táo",1,5,f);put32(f,7);put32(f,10);long size=ftell(f);fclose(f);
    snprintf(path,sizeof(path),"%s/test.ifo",root);f=fopen(path,"wb");assert(f);fprintf(f,"StarDict's dict ifo file\nversion=2.4.2\nbookname=Fixture\nwordcount=2\nidxfilesize=%ld\nsametypesequence=m\n",size);fclose(f);
    snprintf(path,sizeof(path),"%s/test.syn",root);f=fopen(path,"wb");assert(f);fwrite("apples",1,7,f);put32(f,0);fclose(f);
}
static void single(const char *root,const char *types,const char *payload,size_t size) {
    char path[640];snprintf(path,sizeof(path),"%s/test.dict",root);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(payload,1,size,f)==size);fclose(f);
    snprintf(path,sizeof(path),"%s/test.idx",root);f=fopen(path,"wb");assert(f);fwrite("word",1,5,f);put32(f,0);put32(f,(unsigned)size);fclose(f);
    snprintf(path,sizeof(path),"%s/test.ifo",root);f=fopen(path,"wb");assert(f);fprintf(f,"StarDict's dict ifo file\nversion=3.0.0\nwordcount=1\nidxfilesize=13\nsametypesequence=%s\n",types);fclose(f);
}
static int lookup(const char *ifo,const char *query) {
    assert(book_dictionary_open(&d,ifo,query));for(unsigned i=0;i<100;i++){int n=book_dictionary_step(&d);if(n==4||n<0)return n;}assert(!"lookup did not terminate");return 0;
}
int main(void) {
    char root[]="/tmp/kiiko-notes-XXXXXX";assert(mkdtemp(root));
    const char text[]="Một đoạn văn tiếng Việt. Another sentence.";
    assert(book_annotations_load(&a,root,"/sdcard/book.epub",3,text,strlen(text)));
    assert(!a.count);assert(book_annotations_put(&a,text,0,strlen("Một đoạn"),"Ghi chú"));
    assert(!book_annotations_put(&a,text,2,8,"bad boundary"));
    assert(book_annotations_save(&a,root));
    assert(book_annotations_load(&b,root,"/sdcard/book.epub",3,text,strlen(text)));
    assert(b.count==1&&!strcmp(b.items[0].note,"Ghi chú"));
    assert(book_annotations_put(&b,text,0,strlen("Một đoạn"),"Edited"));assert(b.count==1);assert(book_annotations_save(&b,root));
    assert(book_annotations_load(&a,root,"/sdcard/book.epub",3,text,strlen(text)));assert(!strcmp(a.items[0].note,"Edited"));
    assert(book_annotations_load(&b,root,"/sdcard/book.epub",4,text,strlen(text))&&!b.count);
    assert(book_annotations_load(&b,root,"/sdcard/book.epub",3,"changed",7)&&!b.count);
    assert(!book_annotations_save(&a,"/missing/path/notes"));
    /* Exercise destination reuse: POSIX-only tests previously missed save #3. */
    book_annotations_t repeats;
    assert(book_annotations_load(&repeats,root,"repeat.txt",0,text,strlen(text)));
    for(unsigned attempt=0;attempt<8;attempt++) {
        char note[32];snprintf(note,sizeof(note),"save-%u",attempt);
        assert(book_annotations_put(&repeats,text,0,strlen("Một"),note));
        assert(book_annotations_save(&repeats,root));
        book_annotations_t reload;
        assert(book_annotations_load(&reload,root,"repeat.txt",0,text,strlen(text)));
        assert(reload.count==1&&!strcmp(reload.items[0].note,note));
    }
    /* A torn newest snapshot must recover the previous complete generation. */
    char snapshot[640];snprintf(snapshot,sizeof(snapshot),"%s/%016llx-%u-%016llx.%u.ann",root,
       (unsigned long long)a.path_hash,a.chapter,(unsigned long long)a.text_hash,a.sequence&1);
    assert(truncate(snapshot,4)==0);
    assert(book_annotations_load(&b,root,"/sdcard/book.epub",3,text,strlen(text)));
    assert(b.count==1&&!strcmp(b.items[0].note,"Ghi chú"));
    snprintf(snapshot,sizeof(snapshot),"%s/%016llx-%u-%016llx.%u.ann",root,
       (unsigned long long)b.path_hash,b.chapter,(unsigned long long)b.text_hash,b.sequence&1);
    assert(truncate(snapshot,4)==0);
    assert(!book_annotations_load(&b,root,"/sdcard/book.epub",3,text,strlen(text)));
    fixture(root);char ifo[640],path[640];snprintf(ifo,sizeof(ifo),"%s/test.ifo",root);
    assert(lookup(ifo,"apple")==4&&!strcmp(d.text,"A fruit"));
    assert(lookup(ifo,"APPLE")==4);assert(lookup(ifo,"táo")==4&&!strcmp(d.text,"Quả táo"));
    assert(lookup(ifo,"apples")==4&&!strcmp(d.text,"A fruit"));assert(lookup(ifo,"missing")==-2);
    assert(book_dictionary_open(&d,ifo,"apple"));book_dictionary_close(&d);assert(!d.index&&!d.data&&!d.syn);
    snprintf(path,sizeof(path),"%s/test.dict",root);assert(truncate(path,1)==0);
    assert(lookup(ifo,"táo")==-1);
    snprintf(path,sizeof(path),"%s/test.idx",root);assert(truncate(path,2)==0);assert(!book_dictionary_open(&d,ifo,"apple"));
    single(root,"tm","IPA\0meaning",11);
    assert(lookup(ifo,"word")==4&&!strcmp(d.text,"IPA\nmeaning"));
    single(root,"h","<b>word</b> &amp; text",22);
    assert(lookup(ifo,"word")==4&&strstr(d.text,"& text")&&!strchr(d.text,'<'));
    single(root,"m","\xc0\xaf",2);assert(lookup(ifo,"word")==-1);
    single(root,"tm","missing separator",17);assert(lookup(ifo,"word")==-1);
    single(root,"P","binary",6);assert(!book_dictionary_open(&d,ifo,"word"));
    char long_text[9000];memset(long_text,'a',sizeof(long_text));
    single(root,"m",long_text,sizeof(long_text));assert(lookup(ifo,"word")==4&&d.truncated&&strlen(d.text)==8191);
    puts("notes storage and StarDict: PASS");return 0;
}
