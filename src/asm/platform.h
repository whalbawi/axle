#pragma once

#if defined(__APPLE__)
#define ASM_SYM(sym) _##sym
#else
#define ASM_SYM(sym) sym
#endif // defined(__APPLE__)
