cmake_minimum_required(VERSION 3.20)
project(crash_bash_online_installer C)

include(ps3-self)

if("$ENV{CB_PKGFILES}" STREQUAL "")
    message(FATAL_ERROR "CB_PKGFILES must point to the staged PRX payload directory")
endif()

add_executable(crash_bash_online_installer source/main.c)
target_compile_features(crash_bash_online_installer PRIVATE c_std_11)
target_link_libraries(crash_bash_online_installer PRIVATE sysutil_stub rt lv2_stub)

ps3_add_self(crash_bash_online_installer)
ps3_add_pkg(crash_bash_online_installer
    TITLE "Crash Bash Online"
    APPID "CBAS00001"
    CONTENTID "UP0001-CBAS00001_00-CRASHBASHONLINE1"
    PKGFILES "$ENV{CB_PKGFILES}"
)
