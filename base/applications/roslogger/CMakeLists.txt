project(roslauncher)

set_cpp(WITH_RUNTIME)

add_executable(roslogger roslogger.cpp stacktrace.cpp roslogger.rc)
set_module_type(roslogger win32cui UNICODE)
target_link_libraries(roslogger atlnew uuid wine)
add_importlibs(roslogger dbghelp psapi msvcrt kernel32 ntdll)
add_cd_file(TARGET roslogger DESTINATION reactos/system32 FOR all)
