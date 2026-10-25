/* SPDX-License-Identifier: Apache-2.0
 * 中文：升级元数据边界与版本兼容性。/ English: Release bounds and version compatibility. */
#include "ota_online.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *valid = "{\"schema\":1,\"version\":\"0.3.3-rc79\",\"project\":\"" PICO_OTA_PROJECT "\",\"board\":\"" PICO_OTA_BOARD "\",\"layout\":\"" PICO_OTA_LAYOUT "\",\"minimum_base_version\":\"0.3.3-rc72\",\"url\":\"https://wegooo-cell.github.io/read-pico-reader/Pico-update-0.3.3-rc79.bin\",\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"notes\":\"Test release\",\"size\":4096}";
int main(void) {
 pico_release_t release;
 assert(pico_release_parse(valid,strlen(valid),&release)==ESP_OK);
 assert(release.size==4096 && !strcmp(release.version,"0.3.3-rc79"));
 assert(pico_version_compare("0.3.3-rc79","0.3.3-rc9")>0);
 assert(pico_version_compare("0.3.3","0.3.3-rc79")>0);
 assert(pico_version_compare("0.4.0-rc1","0.3.3")>0);
 assert(pico_release_url_valid("https://kiikoread.com/Pico-update-0.3.3-rc90.bin"));
 assert(!pico_release_url_valid("https://kiikoread.com.evil/a.bin"));
 assert(!pico_release_url_valid("https://kiikoread.com@evil/a.bin"));
 assert(!pico_release_url_valid("https://kiikoread.com:8443/a.bin"));
 assert(!pico_release_url_valid("https://kiikoread.com/../a.bin"));
 assert(!pico_release_url_valid("https://kiikoread.com/a.bin?url=evil"));
 assert(!pico_release_url_valid("http://wegooo-cell.github.io/read-pico-reader/a.bin"));
 assert(!pico_release_url_valid("https://wegooo-cell.github.io.evil/read-pico-reader/a.bin"));
 assert(!pico_release_url_valid("https://wegooo-cell.github.io/read-pico-reader/../a.bin"));
 assert(pico_release_parse(valid,strlen(valid)+1,&release)!=ESP_OK);
 assert(pico_release_parse("{}",2,&release)!=ESP_OK);
 char altered[2048]; snprintf(altered,sizeof(altered),"%s junk",valid);
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 snprintf(altered,sizeof(altered),"{\"schema\":1,%s",valid+1);
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 strcpy(altered,valid); char *p=strstr(altered,"4096"); memcpy(p,"1e10",4);
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 strcpy(altered,valid); p=strstr(altered,PICO_OTA_BOARD); p[0]='X';
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 strcpy(altered,valid); p=strstr(altered,"0123456789abcdef"); p[0]='z';
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 snprintf(altered,sizeof(altered),"%s",valid); p=strstr(altered,PICO_OTA_PROJECT);p[0]='X';
 assert(pico_release_parse(altered,strlen(altered),&release)!=ESP_OK);
 const unsigned sizes[]={0x400001,0x500000,0x500001};
 for(unsigned i=0;i<3;++i){p=strstr(valid,"4096");size_t prefix=p-valid;
  snprintf(altered,sizeof(altered),"%.*s%u%s",(int)prefix,valid,sizes[i],p+4);
  assert((pico_release_parse(altered,strlen(altered),&release)==ESP_OK)==(sizes[i]<=PICO_OTA_IMAGE_MAX));
 }
 puts("PASS: release schema, bounded size, hashes, version ordering, board, duplicate/trailing payload and HTTPS origin");
}
