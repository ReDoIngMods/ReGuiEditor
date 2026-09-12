# Converts IN_FILE into a generated header declaring VAR_NAME (a byte array)
# and VAR_NAME_len, so the file's contents can be linked into the executable
# instead of shipped alongside it. Invoked via add_custom_command; see
# regui_embed_asset() in CMakeLists.txt.

if(NOT DEFINED IN_FILE OR NOT DEFINED OUT_FILE OR NOT DEFINED VAR_NAME)
    message(FATAL_ERROR "embed_binary.cmake requires IN_FILE, OUT_FILE and VAR_NAME")
endif()

file(READ "${IN_FILE}" hex_content HEX)
string(LENGTH "${hex_content}" hex_length)
math(EXPR byte_count "${hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex_content}")

file(WRITE "${OUT_FILE}"
    "// Auto-generated from ${IN_FILE} - do not edit.\n"
    "#pragma once\n"
    "inline constexpr unsigned char ${VAR_NAME}[] = {${bytes}};\n"
    "inline constexpr unsigned long ${VAR_NAME}_len = ${byte_count}UL;\n"
)
