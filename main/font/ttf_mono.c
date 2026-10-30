/* SPDX-License-Identifier: Apache-2.0
 * TrueType monochrome hinting from original streams; all engine allocations are
 * bounded PSRAM on ESP. The caller serializes access with existing font drawing.
 */
#include "ttf_mono.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_OUTLINE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#define mono_alloc(n) heap_caps_malloc((n),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)
#define mono_free(p) heap_caps_free(p)
#else
#define mono_alloc(n) malloc(n)
#define mono_free(p) free(p)
#endif
#define MONO_BUDGET (512u*1024u)
#define MONO_AXES 16
static FT_Library library;
static unsigned faces;
static size_t used,peak;
typedef union {max_align_t alignment;size_t bytes;} allocation_t;
static void *allocate(FT_Memory memory,long bytes) {
    (void)memory;
    if(bytes<=0 || (size_t)bytes>MONO_BUDGET-used || sizeof(allocation_t)>MONO_BUDGET-used-(size_t)bytes)return NULL;
#ifdef ESP_PLATFORM
    if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)<(size_t)bytes+2560u*1024u)return NULL;
#endif
    allocation_t *p=mono_alloc(sizeof(*p)+(size_t)bytes);
    if(!p)return NULL;
    p->bytes=sizeof(*p)+(size_t)bytes;used+=p->bytes;if(used>peak)peak=used;
    return p+1;
}
static void release(FT_Memory memory,void *block) {
    (void)memory;if(!block)return;
    allocation_t *p=(allocation_t*)block-1;used-=p->bytes;mono_free(p);
}
static void *resize(FT_Memory memory,long old_size,long new_size,void *block) {
    if(new_size<=0){release(memory,block);return NULL;}
    void *p=allocate(memory,new_size);
    if(!p)return NULL;
    if(block){memcpy(p,block,(size_t)(old_size<new_size?old_size:new_size));release(memory,block);}
    return p;
}
static struct FT_MemoryRec_ memory={NULL,allocate,release,resize};
struct ttf_mono_face {
    FT_Face face;
    FT_StreamRec stream;
    ttf_mono_read_t read;
    void *context;
    FT_Fixed coordinates[MONO_AXES];
    unsigned axes;
    int weight_axis,last_weight;
    FT_F26Dot6 size;
    bool force_auto;
};
static unsigned long stream_read(FT_Stream stream,unsigned long offset,unsigned char *buffer,unsigned long bytes) {
    ttf_mono_face_t *f=stream->descriptor.pointer;
    if(offset>stream->size || bytes>stream->size-offset)return bytes?0:1;
    if(!bytes)return 0;
    return f->read(f->context,(uint32_t)offset,buffer,(size_t)bytes)?bytes:0;
}
static void finish_library(void) {if(library&&!faces){FT_Done_Library(library);library=NULL;}}
ttf_mono_face_t *ttf_mono_open(ttf_mono_read_t read,void *context,uint32_t length) {
    if(!read || !length || length>16u*1024u*1024u)return NULL;
    if(!library){if(FT_New_Library(&memory,&library)){library=NULL;return NULL;}FT_Add_Default_Modules(library);}
    ttf_mono_face_t *f=allocate(&memory,sizeof(*f));
    if(!f){finish_library();return NULL;}
    memset(f,0,sizeof(*f));f->read=read;f->context=context;f->weight_axis=-1;f->last_weight=INT_MIN;
    f->stream.size=length;f->stream.read=stream_read;f->stream.descriptor.pointer=f;
    FT_Open_Args args={.flags=FT_OPEN_STREAM,.stream=&f->stream};
    if(FT_Open_Face(library,&args,0,&f->face))goto fail;
    if(!FT_IS_SCALABLE(f->face) || FT_Select_Charmap(f->face,FT_ENCODING_UNICODE))goto fail;
    if(FT_HAS_MULTIPLE_MASTERS(f->face)) {
        FT_MM_Var *variation=NULL;
        if(FT_Get_MM_Var(f->face,&variation))goto fail;
        if(variation->num_axis>MONO_AXES){FT_Done_MM_Var(library,variation);goto fail;}
        f->axes=variation->num_axis;
        for(unsigned i=0;i<f->axes;++i){f->coordinates[i]=variation->axis[i].def;if(variation->axis[i].tag==FT_MAKE_TAG('w','g','h','t'))f->weight_axis=(int)i;}
        FT_Done_MM_Var(library,variation);
    }
    // Fonts without native hint programs use the monochrome auto-hinter.
    FT_ULong prep=0,fpgm=0;
    (void)FT_Load_Sfnt_Table(f->face,FT_MAKE_TAG('p','r','e','p'),0,NULL,&prep);
    (void)FT_Load_Sfnt_Table(f->face,FT_MAKE_TAG('f','p','g','m'),0,NULL,&fpgm);
    f->force_auto=!prep&&!fpgm;
    ++faces;return f;
fail:
    if(f->face)FT_Done_Face(f->face);
    release(&memory,f);finish_library();return NULL;
}
void ttf_mono_close(ttf_mono_face_t *f) {
    if(!f)return;
    FT_Done_Face(f->face);release(&memory,f);--faces;finish_library();
}
bool ttf_mono_render_stroke(ttf_mono_face_t *f,uint32_t cp,float scale,int weight,int steps,ttf_mono_bitmap_t *out) {
    if(!f||!out||!isfinite(scale)||scale<=0)return false;
    double em=scale*f->face->units_per_EM*64.0;
    if(em<64 || em>256*64)return false;
    FT_F26Dot6 size=(FT_F26Dot6)lround(em);
    if(size!=f->size){if(FT_Set_Char_Size(f->face,0,size,72,72))return false;f->size=size;}
    if(f->weight_axis>=0 && weight>=0 && weight!=f->last_weight){
        f->coordinates[f->weight_axis]=(FT_Fixed)weight*65536L;
        if(FT_Set_Var_Design_Coordinates(f->face,f->axes,f->coordinates))return false;
        f->last_weight=weight;
    }
    FT_UInt glyph=FT_Get_Char_Index(f->face,cp);
    if(!glyph)return false;
    FT_Int32 flags=FT_LOAD_NO_BITMAP|FT_LOAD_TARGET_MONO|(f->force_auto?FT_LOAD_FORCE_AUTOHINT:0);
    if(FT_Load_Glyph(f->face,glyph,flags))return false;
    if(steps < -32)steps=-32;
    if(steps > 32)steps=32;
    if(steps && f->face->glyph->format==FT_GLYPH_FORMAT_OUTLINE) {
        /* Embolden changes total width; center it for a signed contour offset. */
        FT_Pos strength=steps*8;
        if(FT_Outline_EmboldenXY(&f->face->glyph->outline,strength,strength))return false;
        FT_Outline_Translate(&f->face->glyph->outline,-strength/2,-strength/2);
    }
    if(FT_Render_Glyph(f->face->glyph,FT_RENDER_MODE_MONO))return false;
    FT_GlyphSlot slot=f->face->glyph;
    if(slot->bitmap.pixel_mode!=FT_PIXEL_MODE_MONO || slot->bitmap.pitch<0 || slot->bitmap.width>1024 || slot->bitmap.rows>1024)return false;
    *out=(ttf_mono_bitmap_t){slot->bitmap.buffer,(int)slot->bitmap.width,(int)slot->bitmap.rows,slot->bitmap_left,slot->bitmap_top,slot->bitmap.pitch};
    return true;
}
bool ttf_mono_render(ttf_mono_face_t *f,uint32_t cp,float scale,int weight,ttf_mono_bitmap_t *out) {
    return ttf_mono_render_stroke(f,cp,scale,weight,0,out);
}
size_t ttf_mono_bytes(void){return used;}
size_t ttf_mono_peak_bytes(void){return peak;}
