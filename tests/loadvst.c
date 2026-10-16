/* Load a VST3 DLL the way a plugin host does (LoadLibraryW, then
 * GetPluginFactory), so copy protection that runs in DllMain can be tested
 * without a DAW. Prints the result and the time DllMain took.
 *
 * Build: x86_64-w64-mingw32-gcc -O1 -Wall -municode -o loadvst.exe loadvst.c
 * Run:   wine loadvst.exe 'C:\Program Files\Common Files\VST3\Plugin.vst3'
 */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
	HMODULE module;
	FARPROC factory;
	DWORD start, error;

	if (argc != 2) {
		printf("usage: loadvst.exe <plugin.vst3>\n");
		return 2;
	}

	start = GetTickCount();
	module = LoadLibraryW(argv[1]);
	error = GetLastError();
	printf("LoadLibraryW: %s after %lu ms", module ? "OK" : "FAILED",
		GetTickCount() - start);
	if (!module) {
		printf(", error %lu\n", error);
		return 1;
	}
	printf("\n");

	factory = GetProcAddress(module, "GetPluginFactory");
	printf("GetPluginFactory: %s\n", factory ? "found" : "missing");
	if (factory)
		printf("factory object: %p\n", ((void *(*)(void))factory)());

	FreeLibrary(module);
	return 0;
}
