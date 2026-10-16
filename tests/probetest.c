/* PACE checks that its own hardware breakpoints are still set: it points
 * Dr0-Dr3 at a probe and expects every call to trap. This sets them the same
 * way and calls the probe after different kinds of activity, counting the
 * calls that did not trap. Prints PASS or FAIL.
 *
 * Build: x86_64-w64-mingw32-gcc -O1 -Wall -o probetest.exe probetest.c probe.S -ladvapi32
 * Run:   wine probetest.exe [part of an activity name]
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

extern ULONG_PTR probe(void);
extern char probe_bp[];

static int failures;
static volatile LONG stop;
static HANDLE mainThread;

static LONG CALLBACK continue_raised(EXCEPTION_POINTERS *info)
{
	DWORD code = info->ExceptionRecord->ExceptionCode;

	if (code == 0xe0001234 || code == STATUS_GUARD_PAGE_VIOLATION)
		return EXCEPTION_CONTINUE_EXECUTION;
	return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI nothing(void *arg)
{
	return 0;
}

static DWORD WINAPI suspender(void *arg)
{
	SuspendThread(mainThread);
	Sleep(1);
	ResumeThread(mainThread);
	stop = 1;
	return 0;
}

static void act_none(int i) {}
static void act_sleep0(int i) { Sleep(0); }
static void act_sleep1(int i) { Sleep(1); }
static void act_raise(int i) { RaiseException(0xe0001234, 0, 0, NULL); }
static void act_debugstring(int i) { OutputDebugStringA("probe\n"); }

static void act_thread(int i)
{
	HANDLE thread = CreateThread(NULL, 0, nothing, NULL, 0, NULL);
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

static void act_file(int i)
{
	char buffer[64];
	DWORD read;
	HANDLE file = CreateFileA("C:\\windows\\win.ini", GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, 0, NULL);
	if (file != INVALID_HANDLE_VALUE) {
		ReadFile(file, buffer, sizeof(buffer), &read, NULL);
		CloseHandle(file);
	}
}

static void act_guardpage(int i)
{
	static volatile char *page;
	DWORD old;

	if (!page)
		page = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_READWRITE);
	VirtualProtect((void *)page, 4096, PAGE_READWRITE | PAGE_GUARD, &old);
	page[0]++;
}

static void act_getset(int i)
{
	CONTEXT context = { .ContextFlags = CONTEXT_ALL };

	GetThreadContext(GetCurrentThread(), &context);
	SetThreadContext(GetCurrentThread(), &context);
}

static void act_spin(int i)
{
	DWORD start = GetTickCount();

	while (GetTickCount() - start < 20)
		;
}

static void act_crypt(int i)
{
	HCRYPTPROV provider;
	DWORD random;

	if (CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		CryptGenRandom(provider, sizeof(random), (BYTE *)&random);
		CryptReleaseContext(provider, 0);
	}
}

static void act_suspended(int i)
{
	HANDLE thread;

	/* suspended and resumed while running its own code, not in a wait */
	stop = 0;
	thread = CreateThread(NULL, 0, suspender, NULL, 0, NULL);
	while (!stop)
		;
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

static const struct {
	const char *name;
	void (*act)(int);
	int count;
} kActivities[] = {
	{ "nothing", act_none, 1000 },
	{ "Sleep(0)", act_sleep0, 200 },
	{ "Sleep(1)", act_sleep1, 50 },
	{ "RaiseException continued", act_raise, 200 },
	{ "OutputDebugString", act_debugstring, 50 },
	{ "CreateThread and wait", act_thread, 50 },
	{ "file read", act_file, 100 },
	{ "guard page fault", act_guardpage, 100 },
	{ "GetThreadContext + SetThreadContext", act_getset, 100 },
	{ "busy loop 20 ms", act_spin, 25 },
	{ "CryptGenRandom", act_crypt, 25 },
	{ "suspended by another thread", act_suspended, 100 },
};

int main(int argc, char **argv)
{
	CONTEXT context = { .ContextFlags = CONTEXT_DEBUG_REGISTERS };
	ULONG_PTR result;
	int a, i, misses;

	setvbuf(stdout, NULL, _IONBF, 0);
	AddVectoredExceptionHandler(1, continue_raised);
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
		&mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);

	/* as PACE does: read the context, point all four slots at the probe */
	GetThreadContext(GetCurrentThread(), &context);
	context.Dr0 = context.Dr1 = context.Dr2 = context.Dr3 = (DWORD64)probe_bp;
	context.Dr7 = 0x55;
	if (!SetThreadContext(GetCurrentThread(), &context)) {
		printf("FAIL: SetThreadContext, error %lu\n", GetLastError());
		return 1;
	}

	result = probe();
	printf("first probe: %s\n", result == 1 ? "trapped" : "did not trap");
	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	GetThreadContext(GetCurrentThread(), &context);
	printf("read back: Dr0 %s, Dr6 %#llx, Dr7 %#llx\n",
		context.Dr0 == (DWORD64)probe_bp ? "ok" : "WRONG",
		(unsigned long long)context.Dr6, (unsigned long long)context.Dr7);

	for (a = 0; a < sizeof(kActivities) / sizeof(kActivities[0]); a++) {
		/* an argument picks the activities whose name contains it */
		if (argc > 1 && !strstr(kActivities[a].name, argv[1]))
			continue;
		misses = 0;
		for (i = 0; i < kActivities[a].count; i++) {
			kActivities[a].act(i);
			if (probe() != 1)
				misses++;
		}
		printf("%-38s %4d calls, %4d missed\n", kActivities[a].name,
			kActivities[a].count, misses);
		if (misses)
			failures++;
	}

	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	GetThreadContext(GetCurrentThread(), &context);
	printf("at the end: Dr0 %s, Dr6 %#llx, Dr7 %#llx\n",
		context.Dr0 == (DWORD64)probe_bp ? "ok" : "WRONG",
		(unsigned long long)context.Dr6, (unsigned long long)context.Dr7);
	printf(failures ? "FAIL\n" : "PASS\n");
	return failures != 0;
}
