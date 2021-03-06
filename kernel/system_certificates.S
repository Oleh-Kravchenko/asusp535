/* SYMBOL_PREFIX defined on commandline from CONFIG_SYMBOL_PREFIX */
#ifndef SYMBOL_PREFIX
#define ASM_SYMBOL(sym) sym
#else
#define PASTE2(x,y) x##y
#define PASTE(x,y) PASTE2(x,y)
#define ASM_SYMBOL(sym) PASTE(SYMBOL_PREFIX, sym)
#endif

#define GLOBAL(name)	\
	.globl ASM_SYMBOL(name);	\
	ASM_SYMBOL(name):

	.section ".init.data","aw"

GLOBAL(system_certificate_list)
	.incbin "kernel/x509_certificate_list"
GLOBAL(system_certificate_list_end)
