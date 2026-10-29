# 中文：回移Espressif修复f91ec54912，停驻核在缓存关闭期间禁止推测性取指；仅S31构建使用。
# English: Backport Espressif f91ec54912 to stop parked-core speculative fetches with cache off; S31 builds only.
# https://github.com/espressif/esp-idf/commit/f91ec54912c001be169a5aa3976f04077178fee0
# 中文：保留原文件名以沿用SDK的IRAM链接规则，不修改安装的SDK。
# English: Preserve source basenames for SDK IRAM linker rules and leave the installed SDK untouched.
function(s31_cache_compat_source target relative anchor replacement)
    file(READ "$ENV{IDF_PATH}/components/${relative}" source)
    string(FIND "${source}" "${anchor}" found)
    if(found LESS 0)
        message(FATAL_ERROR "S31 cache backport anchor changed in ${relative}; review SDK before building")
    endif()
    string(REPLACE "${anchor}" "${replacement}" source "${source}")
    set(output "${CMAKE_BINARY_DIR}/s31_cache_compat/${relative}")
    get_filename_component(directory "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    file(WRITE "${output}" "${source}")
    get_target_property(sources ${target} SOURCES)
    get_filename_component(basename "${relative}" NAME)
    list(FILTER sources EXCLUDE REGEX "(^|/)${basename}$")
    set_property(TARGET ${target} PROPERTY SOURCES "${sources}")
    target_sources(${target} PRIVATE "${output}")
endfunction()

set(anchor [=[    uint32_t cpuid = (uint32_t) arg;
    // s_flash_op_complete]=])
set(replacement [=[    uint32_t cpuid = (uint32_t) arg;
#if SOC_BRANCH_PREDICTOR_SUPPORTED
    // 缓存关闭前停止此核的推测性外部取指；原恢复路径重新启用。/ Stop speculative external fetches before cache suspension; the original restore path re-enables prediction.
    esp_cpu_branch_prediction_disable();
#endif
    // s_flash_op_complete]=])
s31_cache_compat_source(__idf_spi_flash "spi_flash/cache_utils.c" "${anchor}" "${replacement}")

set(anchor [=[    while (*(volatile uint32_t *)ipc_isr_finish_cmd == 0) { };]=])
set(replacement [=[#if SOC_BRANCH_PREDICTOR_SUPPORTED
    // 停驻期间禁止推测取指，释放后恢复。/ Disable speculative fetches during the stall and restore on release.
    esp_cpu_branch_prediction_disable();
#endif
    while (*(volatile uint32_t *)ipc_isr_finish_cmd == 0) { };
#if SOC_BRANCH_PREDICTOR_SUPPORTED
    esp_cpu_branch_prediction_enable();
#endif]=])
s31_cache_compat_source(__idf_esp_system "esp_system/port/arch/riscv/esp_ipc_isr_routines.c" "${anchor}" "${replacement}")
# 中文：旧版IPC源没有CPU头文件；只给生成副本加头。/ English: The older IPC source lacks CPU headers; add them to the generated copy only.
set(ipc_output "${CMAKE_BINARY_DIR}/s31_cache_compat/esp_system/port/arch/riscv/esp_ipc_isr_routines.c")
file(READ "${ipc_output}" ipc_source)
file(WRITE "${ipc_output}" "#include \"esp_cpu.h\"\n#include \"soc/soc_caps.h\"\n${ipc_source}")

set(anchor [=[void busy_wait(void)
{
    ESP_INFINITE_LOOP();]=])
set(replacement [=[void busy_wait(void)
{
#if SOC_BRANCH_PREDICTOR_SUPPORTED
    // 异常等待不再返回，不允许推测取指污染缓存错误状态。/ Panic parking never returns; prevent speculative fetches from corrupting cache-error status.
    esp_cpu_branch_prediction_disable();
#endif
    ESP_INFINITE_LOOP();]=])
s31_cache_compat_source(__idf_esp_system "esp_system/port/panic_handler.c" "${anchor}" "${replacement}")
