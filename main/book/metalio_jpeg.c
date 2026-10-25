/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：有界JPEG硬解与完整渐进式软解；最终灰阶与现有缓存/波形保持独立。
 * English: Bounded hardware JPEG and full progressive software decoding; preserve grayscale/cache/waveform policy.
 */
#include "metalio_jpeg.h"
#include "book_cover.h"
#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include "jpeglib.h"
#include "jerror.h"
#ifdef ESP_PLATFORM
#include "driver/jpeg_decode.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#else
#include "metalio_jpeg_host.h"
#endif

#define JPEG_RESERVE (2u*1024u*1024u)
#define JPEG_WORK_MAX (12u*1024u*1024u)
static const char *TAG="s31_jpeg";

// 强缩小时用面积平均保留细线；放大保留双线性，不加锐化或改灰阶。
// Area averaging retains fine lines on reduction; enlargement remains bilinear with no sharpening/quantization.
static bool jpeg_scale(const uint8_t *src,unsigned sw,unsigned sh,unsigned stride,
                       unsigned dw,unsigned dh,uint8_t *out) {
    if(!src||!out||!sw||!sh||sw>8192||sh>8192||stride<sw||!dw||!dh||dw>1216||dh>1216)return false;
    book_crop_t c=book_cover_crop(sw,sh,dw,dh);
    if(!c.width||!c.height||c.x+c.width>sw||c.y+c.height>sh)return false;
    for(unsigned y=0;y<dh;++y) {
        if(c.width>=dw && c.height>=dh) {
            unsigned top=(uint64_t)y*c.height*256/dh,bottom=(uint64_t)(y+1)*c.height*256/dh;
            for(unsigned x=0;x<dw;++x) {
                unsigned left=(uint64_t)x*c.width*256/dw,right=(uint64_t)(x+1)*c.width*256/dw;
                uint64_t sum=0,total=(uint64_t)(right-left)*(bottom-top);
                for(unsigned sy=top/256;sy<(bottom+255)/256;++sy) {
                    unsigned a=top>sy*256?top:sy*256,b=bottom<(sy+1)*256?bottom:(sy+1)*256;
                    const uint8_t *row=src+(size_t)(c.y+sy)*stride+c.x;
                    for(unsigned sx=left/256;sx<(right+255)/256;++sx) {
                        unsigned l=left>sx*256?left:sx*256,r=right<(sx+1)*256?right:(sx+1)*256;
                        sum+=(uint64_t)row[sx]*(r-l)*(b-a);
                    }
                }
                out[(size_t)y*dw+x]=(uint8_t)((sum+total/2)/total);
            }
        } else {
            int py=(int)(((uint64_t)(2*y+1)*c.height*128)/dh)-128;
            if(py<0)py=0;
            unsigned sy=(unsigned)py/256,fy=(unsigned)py%256,sy1=sy+1<c.height?sy+1:sy;
            for(unsigned x=0;x<dw;++x) {
                int px=(int)(((uint64_t)(2*x+1)*c.width*128)/dw)-128;
                if(px<0)px=0;
                unsigned sx=(unsigned)px/256,fx=(unsigned)px%256,sx1=sx+1<c.width?sx+1:sx;
                const uint8_t *a=src+(size_t)(c.y+sy)*stride+c.x,*b=src+(size_t)(c.y+sy1)*stride+c.x;
                unsigned n=(a[sx]*(256-fx)+a[sx1]*fx)*(256-fy)+(b[sx]*(256-fx)+b[sx1]*fx)*fy;
                out[(size_t)y*dw+x]=(uint8_t)((n+32768)/65536);
            }
        }
        if((y&63u)==0)vTaskDelay(1);
    }
    return true;
}

static size_t jpeg_budget(void) {
    size_t n=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    return n>JPEG_RESERVE?n-JPEG_RESERVE:0;
}
static bool jpeg_budget_ok(size_t bytes) {
    return bytes<=JPEG_WORK_MAX && bytes<=jpeg_budget() &&
        bytes<=heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

typedef struct {
    struct jpeg_error_mgr error;
    struct jpeg_progress_mgr progress;
    struct jpeg_decompress_struct jpeg;
    jmp_buf escape;
    uint8_t *gray;
    int64_t start,yield_at;
} jpeg_soft_t;
static void jpeg_soft_error(j_common_ptr p) { longjmp(((jpeg_soft_t *)p->client_data)->escape,1); }
static void jpeg_soft_progress(j_common_ptr p) {
    jpeg_soft_t *s=p->client_data;
    if(esp_timer_get_time()-s->start>INT64_C(2500000))ERREXIT(p,JERR_BAD_STATE);
    if(esp_timer_get_time()-s->yield_at>=20000){s->yield_at=esp_timer_get_time();vTaskDelay(1);}
}
static void jpeg_soft_quiet(j_common_ptr p) { (void)p; }
static bool jpeg_progressive(const uint8_t *data,size_t size,FILE *file,unsigned dw,unsigned dh,uint8_t *out) {
    // setjmp后的资源只存堆对象，避免longjmp使栈上的修改值失效。
    // Keep mutable cleanup state on the heap, not indeterminate automatic variables after longjmp.
    jpeg_soft_t *s=calloc(1,sizeof(*s));if(!s)return false;
    volatile bool ok=false;
    s->start=esp_timer_get_time();
    s->jpeg.err=jpeg_std_error(&s->error);s->error.error_exit=jpeg_soft_error;s->error.output_message=jpeg_soft_quiet;
    s->jpeg.client_data=s;
    if(setjmp(s->escape))goto done;
    jpeg_create_decompress(&s->jpeg);
    s->jpeg.client_data=s;
    if(file)jpeg_stdio_src(&s->jpeg,file);else jpeg_mem_src(&s->jpeg,data,(unsigned long)size);
    if(jpeg_read_header(&s->jpeg,TRUE)!=JPEG_HEADER_OK||!s->jpeg.progressive_mode)goto done;
    unsigned w=s->jpeg.image_width,h=s->jpeg.image_height;
    if(!w||!h||w>8192||h>8192||s->jpeg.num_components<1||s->jpeg.num_components>3)goto done;
    // 系数数组按实际采样块计费；保留2MiB给字体、供屏和主循环。
    // Budget actual sampled coefficient blocks, retaining 2MiB for fonts, display and the event loop.
    uint64_t coeff=0;
    for(int i=0;i<s->jpeg.num_components;++i){
        jpeg_component_info *c=&s->jpeg.comp_info[i];
        unsigned bw=(c->width_in_blocks+c->h_samp_factor-1)/c->h_samp_factor*c->h_samp_factor;
        unsigned bh=(c->height_in_blocks+c->v_samp_factor-1)/c->v_samp_factor*c->v_samp_factor;
        coeff+=(uint64_t)bw*bh*sizeof(JBLOCK);
    }
    unsigned scale=1;
    for(unsigned n=2;n<=8;n*=2) {
        book_crop_t c=book_cover_crop((w+n-1)/n,(h+n-1)/n,dw,dh);
        if(c.width<dw||c.height<dh)break;
        scale=n;
    }
    s->jpeg.scale_num=1;s->jpeg.scale_denom=scale;s->jpeg.out_color_space=JCS_GRAYSCALE;
    jpeg_calc_output_dimensions(&s->jpeg);
    size_t plane=(size_t)s->jpeg.output_width*s->jpeg.output_height;
    if(coeff+plane+512u*1024u>jpeg_budget()||coeff+plane>JPEG_WORK_MAX||!jpeg_budget_ok(plane))goto done;
    s->gray=heap_caps_malloc(plane,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!s->gray)goto done;
    s->jpeg.mem->max_memory_to_use=(long)(coeff+512u*1024u);
    s->progress.progress_monitor=jpeg_soft_progress;s->jpeg.progress=&s->progress;
    if(!jpeg_start_decompress(&s->jpeg)||s->jpeg.output_components!=1)goto done;
    while(s->jpeg.output_scanline<s->jpeg.output_height) {
        JSAMPROW row=s->gray+(size_t)s->jpeg.output_scanline*s->jpeg.output_width;
        if(jpeg_read_scanlines(&s->jpeg,&row,1)!=1)goto done;
    }
    if(!jpeg_finish_decompress(&s->jpeg))goto done;
    ok=jpeg_scale(s->gray,s->jpeg.output_width,s->jpeg.output_height,s->jpeg.output_width,dw,dh,out);
    if(ok)ESP_LOGI(TAG,"progressive full %ux%u scale=1/%u ms=%lld",w,h,scale,(long long)((esp_timer_get_time()-s->start)/1000));
done:
    if(s->jpeg.mem)jpeg_destroy_decompress(&s->jpeg);
    free(s->gray);free(s);return ok;
}

// 限制为原生支持的基线/采样；按MCU尺寸计算实际步长，不按可见宽度读填充行。
// Restrict to native baseline/sampling and use MCU-padded stride, never visible width for padded rows.
static bool jpeg_hw_shape(const uint8_t *data,size_t size,unsigned *w,unsigned *h,unsigned *pw,unsigned *ph,bool *gray) {
    if(!data||size<4||data[0]!=255||data[1]!=216)return false;
    for(size_t at=2;at+4<=size;) {
        if(data[at++]!=255)return false;
        while(at<size&&data[at]==255)++at;
        if(at>=size)return false;
        unsigned marker=data[at++];
        if(marker==0xda||marker==0xd9)return false;
        if(marker==1||(marker>=0xd0&&marker<=0xd8))continue;
        if(at+2>size)return false;
        size_t n=(unsigned)data[at]*256+data[at+1];if(n<2||n>size-at)return false;
        if(marker==0xc0) {
            if(n<8||data[at+2]!=8)return false;
            *h=(unsigned)data[at+3]*256+data[at+4];*w=(unsigned)data[at+5]*256+data[at+6];
            unsigned nf=data[at+7];if((nf!=1&&nf!=3)||n!=8+3*nf)return false;
            if(nf==3&&(data[at+8]!=1||data[at+11]!=2||data[at+14]!=3))return false;
            unsigned sample=data[at+9],mx=sample>>4,my=sample&15;
            if(nf==3&&(sample!=0x11&&sample!=0x21&&sample!=0x22))return false;
            if(!mx||!my||mx>2||my>2)return false;
            if(nf==3&&(data[at+12]!=0x11||data[at+15]!=0x11))return false;
            if(!*w||!*h||*w>8192||*h>8192||((uint64_t)(*w)*(*h))%8)return false;
            *pw=(*w+mx*8-1)/(mx*8)*(mx*8);*ph=(*h+my*8-1)/(my*8)*(my*8);*gray=nf==1;return true;
        }
        if(marker>=0xc1&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc)return false;
        at+=n;
    }
    return false;
}
static bool jpeg_hw_file_shape(FILE *file,size_t size,unsigned *w,unsigned *h,unsigned *pw,unsigned *ph,bool *gray) {
    if(fgetc(file)!=255||fgetc(file)!=216)return false;
    while(ftell(file)>=0&&(size_t)ftell(file)+4<=size) {
        if(fgetc(file)!=255)return false;
        int marker;do{marker=fgetc(file);}while(marker==255);
        if(marker<0||marker==0xda||marker==0xd9)return false;
        if(marker==1||(marker>=0xd0&&marker<=0xd8))continue;
        int hi=fgetc(file),lo=fgetc(file);if(hi<0||lo<0)return false;
        size_t n=(unsigned)hi*256+(unsigned)lo;
        if(n<2||n-2>size-(size_t)ftell(file))return false;
        if(marker==0xc0) {
            uint8_t frame[21]={255,216,255,192,(uint8_t)hi,(uint8_t)lo};
            if(n>sizeof(frame)-4||fread(frame+6,1,n-2,file)!=n-2)return false;
            return jpeg_hw_shape(frame,n+4,w,h,pw,ph,gray);
        }
        if(marker>=0xc1&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc)return false;
        if(fseek(file,(long)n-2,SEEK_CUR))return false;
    }
    return false;
}
static bool jpeg_hardware(const uint8_t *data,size_t size,FILE *file,unsigned dw,unsigned dh,uint8_t *out) {
    unsigned w=0,h=0,pw=0,ph=0;bool gray=false;
    if(file){bool shape=jpeg_hw_file_shape(file,size,&w,&h,&pw,&ph,&gray);
        if(fseek(file,0,SEEK_SET)||!shape)return false;
    }else if(!jpeg_hw_shape(data,size,&w,&h,&pw,&ph,&gray))return false;
    size_t bytes=(size_t)pw*ph*(gray?1:3),input_bytes=0,output_bytes=0;
    if(size>4u*1024u*1024u||size>UINT32_MAX||!jpeg_budget_ok(size+bytes+512))return false;
    // 大图小封面优先沿用流式缩解，避免为缩略图分配整幅RGB。
    // Prefer existing streaming scaled decode for small thumbnails of large source images.
    if(dw<=BOOK_COVER_W&&dh<=BOOK_COVER_H&&(uint64_t)w*h>1024u*1024u)return false;
    uint8_t *input=NULL,*pixels=NULL;bool ok=false;jpeg_decoder_handle_t engine=NULL;
    jpeg_decode_memory_alloc_cfg_t mem={.buffer_direction=JPEG_DEC_ALLOC_INPUT_BUFFER};
    input=jpeg_alloc_decoder_mem(size,&mem,&input_bytes);if(!input)goto done;
    if(file){if(fread(input,1,size,file)!=size)goto done;}else memcpy(input,data,size);
    mem.buffer_direction=JPEG_DEC_ALLOC_OUTPUT_BUFFER;
    pixels=jpeg_alloc_decoder_mem(bytes,&mem,&output_bytes);if(!pixels)goto done;
    jpeg_decode_engine_cfg_t config={.timeout_ms=200};
    if(jpeg_new_decoder_engine(&config,&engine)!=ESP_OK)goto done;
    jpeg_decode_cfg_t cfg={.output_format=gray?JPEG_DECODE_OUT_FORMAT_GRAY:JPEG_DECODE_OUT_FORMAT_RGB888,
        .rgb_order=JPEG_DEC_RGB_ELEMENT_ORDER_RGB,.conv_std=JPEG_YUV_RGB_CONV_STD_BT601};
    uint32_t used=0;int64_t start=esp_timer_get_time();
    if(jpeg_decoder_process(engine,&cfg,input,size,pixels,output_bytes,&used)!=ESP_OK||used<bytes)goto done;
    if(!gray) {
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
            const uint8_t *rgb=pixels+((size_t)y*pw+x)*3;
            // RGB888的实际字节顺序由rgb_order显式指定并由宿主/实机对照验证。
            // Explicit rgb_order controls RGB888 byte order, covered by host/device comparisons.
            pixels[(size_t)y*w+x]=(77u*rgb[0]+150u*rgb[1]+29u*rgb[2]+128u)>>8;
        }
    }
    ok=jpeg_scale(pixels,w,h,gray?pw:w,dw,dh,out);
    if(ok)ESP_LOGI(TAG,"hardware %ux%u padded=%ux%u ms=%lld",w,h,pw,ph,(long long)((esp_timer_get_time()-start)/1000));
done:
    if(engine)jpeg_del_decoder_engine(engine);
    free(input);free(pixels);return ok;
}

bool metalio_jpeg_gray(const uint8_t *data,size_t size,FILE *file,unsigned width,unsigned height,uint8_t *out) {
    if((!data&&!file)||(data&&file)||!size||!out||!width||!height||width>1216||height>1216)return false;
    static atomic_flag owner=ATOMIC_FLAG_INIT;
    if(atomic_flag_test_and_set(&owner))return false;
    bool ok=jpeg_hardware(data,size,file,width,height,out);
    if(!ok) {
        if(file&&(fseek(file,0,SEEK_SET)||ferror(file)))goto done;
        ok=jpeg_progressive(data,size,file,width,height,out);
    }
    if(!ok)ESP_LOGD(TAG,"software fallback input=%u",(unsigned)size);
done:
    atomic_flag_clear(&owner);
    return ok;
}
