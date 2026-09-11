# Cross-compiles rvktest.exe, the in-guest suite, for Windows x64 user mode
# with clang-cl and lld-link against the kit's um header closure. No C runtime:
# the program has its own entry point and imports only what src/um/imports
# names from gdi32, kernel32 and ntdll.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

# Compiler detection links a test program, and there is no C runtime to link.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AV_WINDOWS_KIT_DIR)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_LIST_DIR}/windows_user_flags.cmake)
