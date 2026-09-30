/* The calling convention Windows' runtimes use for the functions they and a
 * driver or program hand each other - APIENTRY, WINAPI, CL_CALLBACK and
 * VKAPI_CALL alike: __stdcall in a 32-bit process, and nothing in a 64-bit one,
 * where x64 has one convention. Spelled here because portable code cannot
 * include windows.h, and a 64-bit host build refuses the attribute. A callback
 * called through a pointer without it, in a 32-bit process, leaves the stack
 * pointer where the callee's own cleanup already moved it. */
#ifndef AV_RUNTIME_CALL_HPP
#define AV_RUNTIME_CALL_HPP

#if defined(_M_IX86)
#define AV_RUNTIME_CALL __stdcall
#else
#define AV_RUNTIME_CALL
#endif

#endif  // AV_RUNTIME_CALL_HPP
