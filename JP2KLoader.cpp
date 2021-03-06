#include "stdafx.h"
#include <Windows.h>
#include <detours.h>
#include <iostream>
#include <fstream>
#include <vector>

// #define DEBUG

/* helpers*/

void dump_mem(void *p, int size)
{
	printf("Memory @ %#x, size: %d", p, size);
	size = size > 0x100 ? 0x100 : size;
	for (int i = 0; i < size; i++)
	{
		if (i % 16 == 0) { printf("\n"); }
		printf("%02x ", *((BYTE *)p + i));
	}
	printf("\n");
}

class JP2KCodeStm {
	uint32_t pos = 0;
	char *bytes;
	size_t fileLen;
public:
	JP2KCodeStm(const char *filename);
	int read(void *, int);
	uint32_t seek(int flag, int pos);
	uint32_t GetCurPos();
};

JP2KCodeStm::JP2KCodeStm(const char *filename) {
	// read and load byte array
	std::ifstream f;
	f.open(filename, std::ios_base::in | std::ios_base::binary);
	f.seekg(0, std::ios::end);
	size_t len = f.tellg();
	fileLen = len;
	bytes = new char[len];
	f.seekg(0, std::ios::beg);
	f.read(bytes, len);
	f.close();
#ifdef DEBUG
	printf("[i] fileLen=%d\n", fileLen);
	printf("[i] bytes[0-11]: %x %x %x %x %x %x %x %x %x %x %x %x\n", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10], bytes[11]);
#endif
}

int JP2KCodeStm::read(void *outBuf, int size) {
	printf("[i] JP2KCodeStm::read() - writing %d bytes to %#x, ", size, outBuf);
	uint32_t end = (pos + size > fileLen) ? fileLen : pos + size;
	uint32_t bytesWritten = end - pos;
	void *tmp = new char[bytesWritten];
	printf("bytes_written: %d\n", bytesWritten);
	memcpy(tmp, bytes + pos, bytesWritten);
	memcpy(outBuf, tmp, bytesWritten);
	dump_mem(outBuf, bytesWritten);
	return bytesWritten;
}

uint32_t JP2KCodeStm::seek(int flag, int newPos) {
	switch (flag)
	{
	case 0:
		// relative seek
		pos += newPos;
		break;
	case 1:
		// absolute seek
		pos = newPos;
		break;
	default:
		printf("[x] ERR_NOT_IMPLEMENTED - seek flag %d", flag);
	}
	return pos;
}

uint32_t JP2KCodeStm::GetCurPos() { return pos; }

struct MemObjEx
{
	int(__cdecl *init_something)(int);
	int(__cdecl *get_something)(int);
	int(__cdecl *not_impl)();
	void(__cdecl *free_2)(void *);
	void *(__cdecl *malloc_1)(int);
	void(__cdecl *free_1)(void *);
	void *(__cdecl *memcpy_memset)(void *dest, void *src, int size);
	void *(__cdecl *memset_wrapper)(void *dest, int val, int size);
};

struct decOpt 
{
	DWORD unk_1;
	DWORD unk_2;
	DWORD unk_3;
	DWORD unk_4;
	DWORD unk_5;
	DWORD unk_6;
};

typedef void(*JP2KLibInitEx)(void *);
typedef void *(*JP2KImageCreate)();
typedef decOpt *(*JP2KDecOptCreate)();
typedef void(*JP2KDecOptInitToDefaults)(void *opt);
typedef int(*JP2KImageInitDecoderEx)(void *img, void *unk_1, void *unk_2, decOpt *opt, void *unk_3);

static JP2KLibInitEx libInit;
static JP2KImageCreate imgCreate;
static JP2KDecOptCreate decOptCreate;
static JP2KDecOptInitToDefaults decOptInit;
static JP2KImageInitDecoderEx decode;

static uint32_t pos;
static JP2KCodeStm *stm;
static MemObjEx *memobj;
static std::vector<void *> allocations;

extern "C" __declspec(dllexport) int fuzz_jp2k(char *filename);


int fuzz_jp2k(char *filename) 
{
	stm = new JP2KCodeStm(filename);
	
	// will fail unless MemObj routines implemented
	void *img;
	decOpt *opt;
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
	int ret = decode(img, unk_1, unk_2, opt, unk_3);
	printf("[i] decoder returned: %#x\n", ret);
	return ret;
}

/* ================ */
/* MemObj emulation */
/* ================ */

int init_something(int foo) 
{
	printf("[i] ERR_NOT_IMPLEMENTEd - MemObj::init_something() - %x\n", foo);
	return 0;
}

int get_something(int foo) 
{
	printf("[i] ERR_NOT_IMPLEMENTEd - MemObj::get_something() - %x\n", foo);
	return 0;
}

int not_impl() 
{
	printf("[i] MemObj::not_impl()\n");
	return 0;
}

void free_1(void *p)
{
	printf("[i] MemObj::free_1() - 0x%x\n", p);
	free(p);
}

void free_2(void *p)
{
	printf("[i] MemObj::free_2() - 0x%x\n", p);
	free(p);
}

void *malloc_1(int size)
{
	void *allocation = malloc(size);
	allocations.push_back(allocation);
	printf("[i] MemObj::alloc() - allocated 0x%x bytes at %x\n", size, allocation);
	return allocation;
}

void *memcpy_memset(void *dest, void *src, int size)
{
	printf("[i] MemObj::memcpy_memset() - dest:0x%x   src:0x%x   size:0x%x\n", dest, src, size);
	if (src == NULL)
	{
		return memset(dest, 0, size);
	} 
	else {
		return memcpy(dest, src, size);
	}
}

void *memset_wrapper(void *dest, int val, int size)
{
	printf("[i] MemObj::memset() - dest:0x%x   val:0x%x   size:0x%x\n", dest, val, size);
	return memset(dest, val, size);;
}

void hook_memobj() 
{
	// implement all routines in MemObjEx and call LibInitEx
	memobj = new MemObjEx;
	memobj->init_something = init_something;
	memobj->get_something = get_something;
	memobj->not_impl = not_impl;
	memobj->malloc_1 = malloc_1;
	memobj->free_1 = free_1;
	memobj->free_2 = free_2;
	memobj->memcpy_memset = memcpy_memset;
	memobj->memset_wrapper = memset_wrapper;
	libInit(memobj);
	printf("[i] MemObj initialized via JP2KLibInitEx()\n");
}

/* ====================================== */
/* JP2KCodeStm interception and emulation */
/* ====================================== */

int __stdcall jp2k_init(void *a, void *b, void *c, void *d, void *e, void *f, void *g)
{
	printf("[i] JP2KCodeStm::Init()\n");
	return 1;
}

int __stdcall jp2k_read(void *outBuf, int size)
{
	return stm->read(outBuf, size);
}

int __stdcall jp2k_seek(int flag, int pos)
{
	return stm->seek(flag, pos);
}

int jp2k_getcurpos()
{
	return stm->GetCurPos();
}

int jp2k_is_seekable()
{
	return 1;
}

void hook_jp2kstm() 
{
	PVOID addr;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	addr = DetourFindFunction("JP2KLib.dll", "?InitJP2KCodeStm@JP2KCodeStm@@QAEH_KHPAXPAUJP2KStreamProcsEx@@W4JP2KStmOpenMode@@H@Z");
	printf("[i] JP2KCodeStm::Init() addr - %x\n", addr);
	DetourAttach(&addr, jp2k_init);

	addr = DetourFindFunction("JP2KLib.dll", "?read@JP2KCodeStm@@QAEHPAEH@Z");
	printf("[i] JP2KCodeStm::read() addr - %x\n", addr);
	DetourAttach(&addr, jp2k_read);

	addr = DetourFindFunction("JP2KLib.dll", "?seek@JP2KCodeStm@@QAE_JH_J@Z");
	printf("[i] JP2KCodeStm::seek() addr - %x\n", addr);
	DetourAttach(&addr, jp2k_seek);

	addr = DetourFindFunction("JP2KLib.dll", "?IsSeekable@JP2KCodeStm@@QAE_NXZ");
	printf("[i] JP2KCodeStm::IsSeekable() addr - %x\n", addr);
	DetourAttach(&addr, jp2k_is_seekable);

	addr = DetourFindFunction("JP2KLib.dll", "?GetCurPos@JP2KCodeStm@@QAE_JXZ");
	printf("[i] JP2KCodeStm::GetCurPos() addr - %x\n", addr);
	DetourAttach(&addr, jp2k_getcurpos);

	DetourTransactionCommit();
}


int main(int argc, char** argv)
{
	if (argc != 2) 
	{
		printf("JP2KLoader.exe [jp2k_file]");
		return 1;
	}

	int foo;
	HINSTANCE JP2KLib = LoadLibrary(L"JP2KLib.dll");
	if (JP2KLib) 
	{
		libInit = (JP2KLibInitEx)GetProcAddress(JP2KLib, (LPCSTR)185);
		imgCreate = (JP2KImageCreate)GetProcAddress(JP2KLib, (LPCSTR)58);
		decOptCreate = (JP2KDecOptCreate)GetProcAddress(JP2KLib, (LPCSTR)43);
		decOptInit = (JP2KDecOptInitToDefaults)GetProcAddress(JP2KLib, (LPCSTR)45);
		decode = (JP2KImageInitDecoderEx)GetProcAddress(JP2KLib, (LPCSTR)157);

		hook_jp2kstm();
		hook_memobj();

#ifdef DEBUG
		printf("[!] Press any key and enter to continue...");
		std::cin >> foo;
#endif
		fuzz_jp2k(argv[1]);
	}
	else {
		printf("[error] couldn't load JP2KLib.dll");
		return 0;
	}
}

