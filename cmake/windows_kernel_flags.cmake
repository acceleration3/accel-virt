if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

if(AV_CHECKED)
  set(AV_CHECKED_DEFINE /DAV_CHECKED=1)
endif()

string(JOIN " " CMAKE_CXX_FLAGS_INIT
  /kernel /GR- /EHs-c- /Zl /X /Z7 /O2
  /D_AMD64_ /DAMD64 /D_WIN64 ${AV_CHECKED_DEFINE}
  /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/km/crt -imsvc/windows-kit/km
  -imsvc/windows-kit/wdk-shared -imsvc/windows-kit/sdk-shared)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_CXX_FLAGS_${config}_INIT "")
  set(CMAKE_EXE_LINKER_FLAGS_${config}_INIT "")
endforeach()

set(CMAKE_CXX_CREATE_CONSOLE_EXE "")
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /DRIVER /SUBSYSTEM:NATIVE /ENTRY:GsDriverEntry /NODEFAULTLIB /MANIFEST:NO
  /TSAWARE:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%)

set(AV_WINDOWS_KIT_LIBRARIES ntoskrnl.lib hal.lib BufferOverflowK.lib displib.lib)
list(TRANSFORM AV_WINDOWS_KIT_LIBRARIES PREPEND "\"${AV_WINDOWS_KIT_DIR}/lib/x64/")
list(TRANSFORM AV_WINDOWS_KIT_LIBRARIES APPEND "\"")
list(JOIN AV_WINDOWS_KIT_LIBRARIES " " CMAKE_CXX_STANDARD_LIBRARIES_INIT)
