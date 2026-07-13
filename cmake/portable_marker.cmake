# CPack install staging 钩子：仅 ZIP 产物注入 portable.txt 标记文件
# NSIS 安装版不含 portable.txt，避免安装版误进 portable 模式（配置写 Program Files）
message(STATUS "portable_marker: GEN=${CPACK_GENERATOR} PREFIX=${CMAKE_INSTALL_PREFIX}")
if(CPACK_GENERATOR STREQUAL "ZIP")
    file(WRITE "${CMAKE_INSTALL_PREFIX}/portable.txt"
          "ScreenKiller portable marker. Keep this file beside the executable to store settings in this folder.\n")
endif()
