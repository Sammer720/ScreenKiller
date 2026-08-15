# =============================================================================
# 便携包（ZIP）重命名脚本
# 在 cpack 打包完成之后由「CMake: 打包 (Release)」任务调用（见 .vscode/tasks.json）：
# 把输出目录 build/dist 下的 ScreenKiller-<版本>-win64.zip
# 改名为 ScreenKiller-<版本>-win64-portable.zip，
# 与安装包（NSIS .exe）在文件名上明确区分「免安装版」。
# 说明：不能用 CPack 的 CPACK_POST_BUILD_SCRIPTS 做这件事，
#       该钩子在归档复制到输出目录之前执行，且复制步骤使用原文件名。
# =============================================================================

# 输出目录：脚本位于 cmake/，向上取工作区根的 build/dist
set(_portable_dist_dir "${CMAKE_CURRENT_LIST_DIR}/../build/dist")

# 扫描该目录下所有 zip 逐一重命名；已带 -portable 后缀的跳过，避免重复改名
file(GLOB _portable_zips "${_portable_dist_dir}/*.zip")
foreach(_portable_zip IN LISTS _portable_zips)
    if(_portable_zip MATCHES "-portable[.]zip$")
        continue()
    endif()
    string(REGEX REPLACE "[.]zip$" "-portable.zip" _portable_zip_target "${_portable_zip}")
    file(RENAME "${_portable_zip}" "${_portable_zip_target}")
    message(STATUS "便携包已重命名: ${_portable_zip_target}")
endforeach()