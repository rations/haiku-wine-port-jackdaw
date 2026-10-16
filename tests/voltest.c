/* Print what GetDriveType and GetVolumeInformation return for drive roots, as
 * copy protection uses them to identify the machine.
 *
 * Build: x86_64-w64-mingw32-gcc -O1 -Wall -o voltest.exe voltest.c
 * Run:   wine voltest.exe C Z
 */
#include <windows.h>
#include <stdio.h>

static void show(char letter)
{
	char root[] = "?:\\", name[MAX_PATH], fs[MAX_PATH];
	DWORD serial = 0, maxlen = 0, flags = 0;

	root[0] = letter;
	printf("%s type %u: ", root, GetDriveTypeA(root));
	if (GetVolumeInformationA(root, name, sizeof(name), &serial, &maxlen,
			&flags, fs, sizeof(fs)))
		printf("name \"%s\" serial %08lx maxlen %lu flags %08lx fs \"%s\"\n",
			name, serial, maxlen, flags, fs);
	else
		printf("GetVolumeInformation failed, error %lu\n", GetLastError());
}

int main(int argc, char **argv)
{
	int i;

	if (argc < 2) {
		show('C');
		show('Z');
	}
	for (i = 1; i < argc; i++)
		show(argv[i][0]);
	return 0;
}
