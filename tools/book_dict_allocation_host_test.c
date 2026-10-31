/* SPDX-License-Identifier: Apache-2.0 */
#include "book_dict_file.h"
#include "book_dictionary.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned live;
static int countdown=-1;
void *dict_test_calloc(size_t n,size_t size) {
    if(countdown==0)return NULL;
    if(countdown>0)--countdown;
    void *p=calloc(n,size);if(p)++live;return p;
}
void dict_test_free(void *p) {if(p){assert(live);--live;}free(p);}
int main(int argc,char **argv) {
    assert(argc==4);
    for(int fault=0;fault<20;fault++) {
        countdown=fault;
        book_dict_file_t *f=book_dict_file_open(argv[1],true,true);
        if(f) {
            unsigned char buf[1200];
            if(book_dict_file_begin(f,11,buf,sizeof(buf))) {
                int result;unsigned ticks=0;
                do {result=book_dict_file_step(f);assert(++ticks<1000);}while(result==0);
            }
            book_dict_file_close(f);
        }
        assert(!live);
    }
    for(int fault=0;fault<20;fault++) {
        countdown=fault;
        book_dict_file_t *f=book_dict_file_open(argv[2],true,false);
        if(f){unsigned char buf[1200];book_dict_file_read(f,buf,sizeof(buf));book_dict_file_close(f);}
        assert(!live);
    }
    for(int fault=0;fault<24;fault++) {
        countdown=fault;book_dictionary_t d={0};
        if(book_dictionary_open(&d,argv[3],"water")) {
            int result;unsigned ticks=0;
            do {result=book_dictionary_step(&d);assert(++ticks<1000);}while(result>0&&result!=4);
        }
        book_dictionary_close(&d);assert(!live&&!d.index&&!d.data&&!d.syn);
    }
    puts("dictzip/gzip allocation failure and partial initialization cleanup: PASS");return 0;
}
