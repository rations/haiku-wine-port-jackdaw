/* Single step and int3 caught by a vectored exception handler, as
 * anti-debugging code does. Prints PASS or FAIL.
 *
 * Build: x86_64-w64-mingw32-gcc -O1 -Wall -o traptest.exe traptest.c
 */
#include <windows.h>
#include <stdio.h>

static volatile LONG steps, breaks;
static volatile ULONG_PTR stepAddress, breakAddress;
extern char after_int3[];

static LONG CALLBACK handler(EXCEPTION_POINTERS *info)
{
	DWORD code = info->ExceptionRecord->ExceptionCode;

	if (code == EXCEPTION_SINGLE_STEP) {
		steps++;
		stepAddress = (ULONG_PTR)info->ExceptionRecord->ExceptionAddress;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	if (code == EXCEPTION_BREAKPOINT) {
		breaks++;
		breakAddress = (ULONG_PTR)info->ExceptionRecord->ExceptionAddress;
		info->ContextRecord->Rip++;	/* skip the int3 */
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
	AddVectoredExceptionHandler(1, handler);
	__asm__ volatile(
		"lea -128(%%rsp), %%rsp\n\t"
		"pushfq\n\t"
		"orq $0x100, (%%rsp)\n\t"
		"popfq\n\t"
		"nop\n\t"
		"nop\n\t"
		"lea 128(%%rsp), %%rsp\n\t"
		::: "memory", "cc");
	__asm__ volatile("int3\n.globl after_int3\nafter_int3:\n\t" ::: "memory");
	printf("single steps %ld at %p, breakpoints %ld at %p (int3 at %p)\n",
		steps, (void *)stepAddress, breaks, (void *)breakAddress,
		(void *)(after_int3 - 1));
	int ok = steps == 1 && breaks == 1 && breakAddress == (ULONG_PTR)(after_int3 - 1);
	printf(ok ? "PASS\n" : "FAIL\n");
	return ok ? 0 : 1;
}
