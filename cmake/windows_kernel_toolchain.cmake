# Cross-compiles for the Windows x64 kernel with clang-cl and lld-link, against
# the kit fetched by tools/fetch_windows_kit.sh. No hosted header, C runtime,
# exception or RTTI support is reachable from anything built with this file.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)

# Compiler detection links a test program; a user-mode executable cannot link
# without a C runtime, and there is none.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AV_WINDOWS_KIT_DIR)

# The MSVC platform module appends /RTC1, /EHsc, WIN32 and kernel32.lib to the
# *_INIT variables after this file runs. A rules override runs after it.
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_LIST_DIR}/windows_kernel_flags.cmake)
