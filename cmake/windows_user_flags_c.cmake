if(NOT AV_WINDOWS_KIT_DIR)
  message(FATAL_ERROR "AV_WINDOWS_KIT_DIR is not set; run tools/fetch_windows_kit.sh")
endif()

if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_DEFINES /D_X86_)
else()
  set(AV_GUEST_ARCH_DEFINES /D_AMD64_ /DAMD64 /D_WIN64)
endif()

string(JOIN " " CMAKE_C_FLAGS_INIT
  /Zl /X /Z7 /GS- /Gy /Gw
  ${AV_GUEST_ARCH_DEFINES} /DWIN32_LEAN_AND_MEAN /DNOMINMAX
  /clang:-ivfsoverlay${AV_WINDOWS_KIT_DIR}/overlay.yaml
  -imsvc/windows-kit/um -imsvc/windows-kit/sdk-shared -imsvc/windows-kit/km/crt)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_C_FLAGS_${config}_INIT "")
endforeach()
set(CMAKE_C_STANDARD_LIBRARIES_INIT "")

if(AV_GUEST_ARCH STREQUAL "x86")
  set(AV_GUEST_ARCH_LINK -stdcall-fixup)
else()
  set(AV_GUEST_ARCH_LINK "")
endif()
string(JOIN " " CMAKE_EXE_LINKER_FLAGS_INIT
  /SUBSYSTEM:CONSOLE /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%
  ${AV_GUEST_ARCH_LINK})
string(JOIN " " CMAKE_SHARED_LINKER_FLAGS_INIT
  /DLL /NODEFAULTLIB /MANIFEST:NO /OPT:REF /DEBUG /Brepro /PDBALTPATH:%_PDB%
  ${AV_GUEST_ARCH_LINK})
