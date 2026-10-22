/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：显式本地S31构建才启用的无线计数；无命令入口、文件或硬件设置写入。
 * English: Opt-in local S31 radio counters, without commands, files or hardware configuration writes.
 */
#pragma once
#include "esp_wifi_he.h"
#include "esp_private/esp_wifi_he_private.h"

static esp_err_t probe_rx_enabled,probe_tx_enabled;
static void transfer_network_probe_start(void) {
    probe_rx_enabled=esp_wifi_enable_rx_statistics(true,false);
    probe_tx_enabled=esp_wifi_enable_tx_statistics(ESP_WIFI_ACI_BE,true);
}

static void transfer_network_probe_info(cJSON *json) {
    cJSON *j=cJSON_AddObjectToObject(json,"radio_probe");
    if(!j)return;
    cJSON_AddNumberToObject(j,"rx_enable_error",probe_rx_enabled);
    cJSON_AddNumberToObject(j,"tx_enable_error",probe_tx_enabled);
    wifi_phy_mode_t phy;
    if(esp_wifi_sta_get_negotiated_phymode(&phy)==ESP_OK)cJSON_AddNumberToObject(j,"negotiated_phy",phy);
    wifi_bandwidth_t bw;
    if(esp_wifi_get_bandwidth(WIFI_IF_STA,&bw)==ESP_OK)cJSON_AddNumberToObject(j,"bandwidth",bw);
    esp_test_rx_statistics_t rx={0};
    esp_err_t err=esp_wifi_get_rx_statistics(0,&rx);
    cJSON_AddNumberToObject(j,"rx_stats_error",err);
    if(err==ESP_OK) {
#define PROBE_RX(field) cJSON_AddNumberToObject(j,#field,rx.field)
        PROBE_RX(legacy);PROBE_RX(legacy_noeb);PROBE_RX(ht);PROBE_RX(ht_noeb);PROBE_RX(ht_retry);
        PROBE_RX(su);PROBE_RX(su_noeb);PROBE_RX(su_retry);PROBE_RX(rx_isr);PROBE_RX(rx_nblks);
#undef PROBE_RX
    }
    esp_test_hw_rx_statistics_t hw={0};
    err=esp_test_get_hw_rx_statistics(&hw);
    cJSON_AddNumberToObject(j,"hw_stats_error",err);
    if(err==ESP_OK) {
#define PROBE_HW(field) cJSON_AddNumberToObject(j,#field,hw.field)
        PROBE_HW(rx_fcs_err);PROBE_HW(rx_mpdu);PROBE_HW(rx_datasuc);PROBE_HW(rx_buf_fullcnt);
        PROBE_HW(rx_fifo_ovfcnt);PROBE_HW(rx_btblock_err);PROBE_HW(nrx_err);PROBE_HW(rxhung_statis);
#undef PROBE_HW
    }
    esp_test_tx_statistics_t tx={0};esp_test_tx_fail_statistics_t fail={0};
    err=esp_wifi_get_tx_statistics(ESP_WIFI_ACI_BE,&tx,&fail);
    cJSON_AddNumberToObject(j,"tx_stats_error",err);
    if(err==ESP_OK) {
#define PROBE_TX(field) cJSON_AddNumberToObject(j,#field,tx.field)
        PROBE_TX(retry_edca);PROBE_TX(tx_complete);PROBE_TX(tx_succ);PROBE_TX(tx_no_mem);
        PROBE_TX(tx_max_rtt);PROBE_TX(tx_min_rtt);PROBE_TX(tx_tot_rtt);PROBE_TX(collision);PROBE_TX(timeout);
#undef PROBE_TX
        cJSON_AddNumberToObject(j,"tx_fail_count",fail.count);
    }
}
