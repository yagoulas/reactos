project(roslauncher)

set_cpp(WITH_RUNTIME)

add_executable(roslogger roslogger.cpp roslogger.rc)
set_module_type(roslogger win32cui UNICODE)
target_link_libraries(roslogger atlnew uuid wine)
add_importlibs(roslogger psapi msvcrt kernel32 ntdll)
add_cd_file(TARGET roslogger DESTINATION reactos/system32 FOR all)
