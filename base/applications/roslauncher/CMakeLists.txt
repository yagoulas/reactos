set_cpp(WITH_RUNTIME)

add_executable(roslauncher roslauncher.cpp roslauncher.rc)
set_module_type(roslauncher win32gui UNICODE)
target_link_libraries(roslauncher atlnew uuid wine)
add_importlibs(roslauncher advapi32 msvcrt kernel32 ntdll)
add_cd_file(TARGET roslauncher DESTINATION reactos/system32 FOR all)
