#include "stdafx.h"
#include <Windows.h>
#include <iostream>

extern "C" __declspec(dllexport) int fuzz_jp2k(wchar_t* filename);

struct decOpt {
	DWORD unk_1;
	DWORD unk_2;
	DWORD unk_3;
	DWORD unk_4;
	DWORD unk_5;
	DWORD unk_6;
};

typedef void *(*JP2KImageCreate)();
typedef decOpt *(*JP2KDecOptCreate)();
typedef void(*JP2KDecOptInitToDefaults)(void *opt);
typedef int(*JP2KImageInitDecoderEx)(void *img, void *unk_1, void *unk_2, decOpt *opt, void *unk_3);

JP2KImageCreate imgCreate;
JP2KDecOptCreate decOptCreate;
JP2KDecOptInitToDefaults decOptInit;
JP2KImageInitDecoderEx decoder;

int fuzz_jp2k(wchar_t *filename) {
	return 0;
}

int main()
{
	int foo;
	void *img;
	decOpt *opt;
	HINSTANCE JP2KLib = LoadLibrary(L"JP2KLib.dll");
	if (JP2KLib) {
		imgCreate = (JP2KImageCreate)GetProcAddress(JP2KLib, (LPCSTR)58);
		decOptCreate = (JP2KDecOptCreate)GetProcAddress(JP2KLib, (LPCSTR)43);
		decOptInit = (JP2KDecOptInitToDefaults)GetProcAddress(JP2KLib, (LPCSTR)45);
		decoder = (JP2KImageInitDecoderEx)GetProcAddress(JP2KLib, (LPCSTR)157);

		printf("[!] Press any key and enter to continue...");
		std::cin >> foo;
		
		// will fail unless alloc routine hooked
		img = imgCreate();
		printf("[image addr] %x\n", img);
		opt = decOptCreate();
		printf("[opt addr] %x\n", opt);
		decOptInit(opt);
		printf("[initialized opt]\n");
		printf("[structure at offset 0x14] %x\n", opt->unk_6);
		// JP2KStm code must be replaced or this will fail
		void *unk_1 = malloc(0x100);
		void *unk_2 = malloc(0x100);
		void *unk_3 = malloc(0x100);
		printf("[i] decoder returned: 0x%x\n", decoder(img, unk_1, unk_2, opt, unk_3));
	}
	else {
		printf("[error] couldn't load JP2KLib.dll");
		return 0;
	}
}

