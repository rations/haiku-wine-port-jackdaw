/* Hardware breakpoints through SetThreadContext, as anti-tamper code uses
 * them: on the current thread, on another (suspended) thread, and a write
 * watchpoint. Prints PASS or FAIL.
 *
 * Build: x86_64-w64-mingw32-gcc -O1 -Wall -o hwbptest.exe hwbptest.c
 */
#include <windows.h>
#include <stdio.h>

static volatile LONG hits[2], badHits;
static volatile int watched;
static int failures;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		printf("FAIL: " __VA_ARGS__); \
		printf("\n"); \
		failures++; \
	} \
} while (0)

__attribute__((noinline)) static int target(int value)
{
	__asm__ volatile("" ::: "memory");
	return value + 1;
}

static LONG CALLBACK handler(EXCEPTION_POINTERS *info)
{
	CONTEXT *context = info->ContextRecord;

	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
		return EXCEPTION_CONTINUE_SEARCH;
	if (context->Dr6 & 1) {
		hits[0]++;
		if (info->ExceptionRecord->ExceptionAddress != (void *)target)
			badHits++;
		context->EFlags |= 0x10000;	/* resume flag, as debuggers do */
	} else if (context->Dr6 & 2) {
		hits[1]++;
		if (watched != 7)
			badHits++;
	} else {
		printf("single step without a breakpoint, Dr6 %#llx\n",
			(unsigned long long)context->Dr6);
		badHits++;
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI thread_calls_target(void *arg)
{
	return target(10);
}

static BOOL set_regs(HANDLE thread, DWORD64 dr0, DWORD64 dr1, DWORD64 dr7)
{
	CONTEXT context = { .ContextFlags = CONTEXT_DEBUG_REGISTERS };
	context.Dr0 = dr0;
	context.Dr1 = dr1;
	context.Dr7 = dr7;
	return SetThreadContext(thread, &context);
}

int main(void)
{
	CONTEXT context = { .ContextFlags = CONTEXT_DEBUG_REGISTERS };
	HANDLE thread;
	int value;

	AddVectoredExceptionHandler(1, handler);

	/* current thread: an instruction breakpoint, twice */
	CHECK(set_regs(GetCurrentThread(), (DWORD64)target, 0, 1), "SetThreadContext");
	GetThreadContext(GetCurrentThread(), &context);
	CHECK(context.Dr0 == (DWORD64)target && context.Dr7 == 1,
		"read back Dr0 %#llx Dr7 %#llx", (unsigned long long)context.Dr0,
		(unsigned long long)context.Dr7);
	value = target(1) + target(2);
	CHECK(value == 5, "target returned %d", value);
	CHECK(hits[0] == 2, "%ld breakpoint hits on this thread, expected 2", hits[0]);

	/* a write watchpoint in slot 1: L1, R/W1 = write, LEN1 = 4 bytes */
	CHECK(set_regs(GetCurrentThread(), 0, (DWORD64)&watched,
		0x4 | (1 << 20) | (3 << 22)), "SetThreadContext watchpoint");
	watched = 7;
	CHECK(hits[1] == 1, "%ld watchpoint hits, expected 1", hits[1]);

	/* cleared */
	set_regs(GetCurrentThread(), 0, 0, 0);
	target(3);
	watched = 8;
	CHECK(hits[0] == 2 && hits[1] == 1, "hits after clearing: %ld %ld",
		hits[0], hits[1]);

	/* another thread, set while it is suspended */
	thread = CreateThread(NULL, 0, thread_calls_target, NULL, CREATE_SUSPENDED, NULL);
	CHECK(set_regs(thread, (DWORD64)target, 0, 1), "SetThreadContext on another thread");
	context.Dr0 = context.Dr7 = 0;
	GetThreadContext(thread, &context);
	CHECK(context.Dr0 == (DWORD64)target && context.Dr7 == 1,
		"other thread reads back Dr0 %#llx Dr7 %#llx",
		(unsigned long long)context.Dr0, (unsigned long long)context.Dr7);
	ResumeThread(thread);
	WaitForSingleObject(thread, INFINITE);
	CHECK(hits[0] == 3, "%ld breakpoint hits after the other thread, expected 3", hits[0]);

	CHECK(badHits == 0, "%ld wrong hits", badHits);
	printf(failures ? "FAIL\n" : "PASS\n");
	return failures != 0;
}
