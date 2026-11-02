#ifndef AV_RUNTIME_CALL_HPP
#define AV_RUNTIME_CALL_HPP

#if defined(_M_IX86)
#define AV_RUNTIME_CALL __stdcall
#else
#define AV_RUNTIME_CALL
#endif

#endif
