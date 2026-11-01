# S31-only build overlay: keep installed IDF sources untouched.
set(identity_source "$ENV{IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/src/ble_hs_id.c")
file(READ "${identity_source}" identity_text)
set(identity_anchor "    if (memcmp(id_addr, ble_hs_misc_null_addr, 6) == 0) {")
string(FIND "${identity_text}" "${identity_anchor}" identity_found)
if(identity_found LESS 0)
    message(FATAL_ERROR "NimBLE identity comparison changed; review S31 byte-access overlay")
endif()
set(identity_replacement [=[    /* Identity arrays may begin at ctx+6 in external RAM. Force byte accesses. */
    const volatile uint8_t *identity_bytes = id_addr;
    uint8_t identity_nonzero = 0;
    for (unsigned i = 0; i < 6; ++i) identity_nonzero |= identity_bytes[i];
    if (identity_nonzero == 0) {]=])
string(REPLACE "${identity_anchor}" "${identity_replacement}" identity_text "${identity_text}")
set(identity_output "${CMAKE_BINARY_DIR}/s31_nimble_compat/ble_hs_id.c")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/s31_nimble_compat")
file(WRITE "${identity_output}" "${identity_text}")
get_target_property(bt_sources __idf_bt SOURCES)
list(FILTER bt_sources EXCLUDE REGEX "(^|/)ble_hs_id.c$")
set_property(TARGET __idf_bt PROPERTY SOURCES "${bt_sources}")
get_filename_component(identity_directory "${identity_source}" DIRECTORY)
target_include_directories(__idf_bt PRIVATE "${identity_directory}")
target_sources(__idf_bt PRIVATE "${identity_output}")
