#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <windows.h>
#include <vector>
#include <memory>
#include <string>
#include <d3d9.h>
//gdi+的头文件要放到d3d9之后，不然会出现依赖循环
#include <gdiplus.h>
#include <map>
#include "AsmHelper.h"
#include "dc3wy.h"
#include "detours.h"

#pragma comment(lib, "detours.lib")

using namespace Gdiplus;

std::vector<uint8_t> *cacheA;
std::vector<uint8_t> *cachePresent;


namespace Hook::Mem {

    static bool MemWrite(uintptr_t Addr, void* Buf, size_t Size) {
        DWORD OldPro = NULL; SIZE_T wirteBytes = NULL;
        if (VirtualProtect((VOID*)Addr, Size, PAGE_EXECUTE_READWRITE, &OldPro)) {
            WriteProcessMemory(INVALID_HANDLE_VALUE, (VOID*)Addr, Buf, Size, &wirteBytes);
            VirtualProtect((VOID*)Addr, Size, OldPro, &OldPro);
            return Size == wirteBytes;
        }
        return false;
    }

    static bool JmpWrite(uintptr_t orgAddr, uintptr_t tarAddr, bool iscall = false, bool isSuper = true) {
        uint8_t jmp_write[5] = { 0xE9, 0x0, 0x0, 0x0, 0x0 };
		if (iscall) jmp_write[0] = 0xE8;

        tarAddr = tarAddr - orgAddr - 5;
        memcpy(jmp_write + 1, &tarAddr, 4);
		if (isSuper) return MemWrite(orgAddr, jmp_write, 5);
		memcpy((void*)orgAddr, jmp_write, 5);
		return true;
    }

	static DWORD SetMemProtect(std::vector<uint8_t> *vec, DWORD newProtectFlag) {
		if (vec) {
			DWORD OldPro = NULL;
			if (VirtualProtect((VOID*)vec->data(), vec->size(), newProtectFlag, &OldPro));
			return OldPro;
		}
		return 0;
	}
	
	//标准化的32位跳板call
	//d3d9 present hook
	//pushad 1
	//push param  ---1 n
	//call function ---2 5
	//popad 1
	//fix byte --3 n
	//jmp addr --4 5
	static std::vector<uint8_t>* CreateBuffCall(std::vector<uint8_t> paramList, void *funAddr, std::vector<uint8_t> fixList, intptr_t backAddr) {
		//printf("paramList size is %d, fixList size is %d", paramList.size(), fixList.size());
		auto vec = new std::vector<uint8_t>(12 + paramList.size() + fixList.size());
		//printf("vec size is %d\n", vec.size());

		uint8_t *ptr = vec->data();
		*ptr = 0x60; //pushad
		ptr++;
		for (int ii = 0; ii < paramList.size(); ii++, ptr++) *ptr = paramList[ii]; //size()

		Mem::JmpWrite((intptr_t)ptr, (intptr_t)funAddr, true, false);
		ptr += 5;
		*ptr = 0x61;
		ptr++;
		for (int ii = 0; ii < fixList.size(); ii++, ptr++) *ptr = fixList[ii]; //size()
		Mem::JmpWrite((intptr_t)ptr, backAddr, false, false);

		//权限
		Mem::SetMemProtect(vec, PAGE_EXECUTE_READWRITE);
		return vec;
	}
}

namespace Hook::Type {
    typedef struct { HFONT jis_f; HFONT gbk_f; size_t size; } Font;
    typedef HANDLE(WINAPI* FindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
    typedef DWORD (WINAPI* GetGlyphOutlineA)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, MAT2*);
    typedef HANDLE(WINAPI* CreateFileA)(LPCSTR , DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    typedef HANDLE(WINAPI* CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
}

namespace Hook::Fun {

    static std::string  NewReplacePathA;
    static std::wstring NewReplacePathW;
    static std::vector<Type::Font*> Fonts;

    static Type::Font* GetFontStruct(HDC& hdc, tagTEXTMETRICA lptm = {}) {
        GetTextMetricsA(hdc, &lptm);
        size_t size = (size_t)lptm.tmHeight;
        for (Type::Font* f : Fonts) if (f->size == size) return f;
        Type::Font* nf = new Type::Font{ nullptr, nullptr, size };
        nf->gbk_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x86, 4, 0x20, 4, 4, "黑体");
        nf->jis_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x80, 4, 0x20, 4, 4, "黑体");
        Fonts.push_back(nf);
        return nf;
    }

    static bool ReplacePathW(std::wstring path) {
        path.assign(path.substr(path.find_last_of(L"\\") + 1)).insert(0, L"cn_Data\\");
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)  return false;
        return NewReplacePathW.assign(path).size();
    }

    static bool ReplacePathA(std::string path) {
        path.assign(path.substr(path.find_last_of("\\") + 1)).insert(0, "cn_Data\\");
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        return NewReplacePathA.assign(path).size();
    }

    Type::GetGlyphOutlineA OldGetGlyphOutlineA;
    static DWORD WINAPI NewGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuf, LPGLYPHMETRICS lpgm, DWORD cjbf, LPVOID pvbf, MAT2* lpmat) {
        Type::Font* font = GetFontStruct(hdc);
        if (uChar == 0xA1EC) { 
            uChar = 0x81F4; // 替换♪
            SelectObject(hdc, font->jis_f);
        } else {
            // 替换半角空格
            if (uChar == 0x23) uChar = 0x20;
            SelectObject(hdc, font->gbk_f);
        }
        return OldGetGlyphOutlineA(hdc, uChar, fuf, lpgm, cjbf, pvbf, lpmat);
    }

    Type::FindFirstFileA OldFindFirstFileA;
    static HANDLE WINAPI NewFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
        return OldFindFirstFileA(ReplacePathA(lpFileName) ? NewReplacePathA.c_str() : lpFileName, lpFindFileData);
    }

    Type::CreateFileA OldCreateFileA;
    static HANDLE WINAPI NewCreateFileA(LPCSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF)  {
        return OldCreateFileA(ReplacePathA(lpFN) ? NewReplacePathA.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }

    Type::CreateFileW OldCreateFileW;
    static HANDLE WINAPI NewCreateFileW(LPCWSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF) {
        return OldCreateFileW(ReplacePathW(lpFN) ? NewReplacePathW.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }

	//在绘图结束前绘制
	DWORD lastCount = 0;
	IDirect3DDevice9* d3d9_device = nullptr;
	IDirect3D9 *d3d9 = nullptr;
	intptr_t callESP = 0;
	HWND wyHwnd = NULL;
	//std::map<int, int> callMap;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken = 0;
	std::unique_ptr<SolidBrush> brush;
	std::unique_ptr<Font> font;

	
	

	static void _stdcall MyD3Ddraw(void*) {
		//printf("yes!");
		DWORD current = GetTickCount();
		printf("time diff:%d", current - lastCount);
		lastCount = current;
	}


	//准备一下Present
	static void _stdcall MyPresent(void *eax) {
		DWORD current = GetTickCount();
		DWORD diff = current - lastCount;

		HDC hdc = GetDC(wyHwnd);
		if (hdc) {
			Graphics graphics(hdc);
			std::wstring sst = std::to_wstring(current);
			//draw it
			PointF rect = { 20,20 };
			INT len = sst.length();
			auto ret = graphics.DrawString(sst.c_str(), len, font.get(), rect, brush.get());
			printf("\rnow is %d,state %d", current, ret);
		}
		//printf("\rCall from %x, diff is %d, playtime is %lf", *(int*)callESP, diff, Dc3wy::subtitle::hasPlayTime);
		lastCount = current;
	}

	static void _stdcall MyPresentBack() {
		DWORD current = GetTickCount();
		DWORD diff = current - lastCount;

		HDC hdc = GetDC(wyHwnd);
		if (hdc) {
			Graphics graphics(hdc);
			std::wstring sst = std::to_wstring(current);
			//draw it
			PointF rect = { 20,20 };
			INT len = sst.length();
			auto ret = graphics.DrawString(sst.c_str(), len, font.get(), rect, brush.get());
			printf("\rnow is %d,state %d", current, ret);
		}
		//printf("\rCall from %x, diff is %d, playtime is %lf", *(int*)callESP, diff, Dc3wy::subtitle::hasPlayTime);
		lastCount = current;
	}

	BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM IParam) {
		int len = GetWindowTextLengthA(hwnd);
		if (len > 0 && len < 256) {
			char buf[256];
			GetWindowTextA(hwnd, buf, 255);
			std::string s(buf, len);
			size_t index = s.find("【COKEZIGE STUDIO】");
			if (index != std::string::npos) {
				wyHwnd = hwnd;
				Dc3wy::subtitle::mainWindow = hwnd;
				printf("hwnd is %x, %s\n", Dc3wy::subtitle::mainWindow, s.c_str());
			}

		}
		return TRUE;
	}

	//获取D3D的信息
	static void _stdcall GetD3Deice(IDirect3DDevice9 *drv) {
		if (!d3d9_device) {
			d3d9_device = drv;
			printf("d3d9 device is %x\n", d3d9_device);
			d3d9_device->GetDirect3D(&d3d9);
			printf("d3d9 is %x\n", d3d9);

			intptr_t vtable = *(intptr_t*)drv;
			printf("endSence at %x, Present at %x\n", *(intptr_t*)(vtable + 0xA8), *(intptr_t*)(vtable + 0x44));

			EnumWindows(EnumWindowsProc, 0);
			//先调用present，再调用gdi绘图
			AsmHelper asm1;
			AsmHelper asm2;
			intptr_t PresetAddress = *(intptr_t*)(vtable + 0x44);
			//hook->跳板1-->ret 至跳板2 -->ret 回原call函数
			//==== 挑板2 ====
			//push ecx
			//push ecx
			//mov mov ecx,$callESP
			asm2.add_valset({ 0x51, 0x51, 0x8b, 0x0d }, &callESP);
			//mov [esp+4],ecx
			//pop ecx
			//pushad
			asm2.add_cmd({0x89, 0x4c, 0x24, 0x4, 0x59, 0x60});
			//call 
			asm2.add_jmp(0xe8, &MyPresentBack);
			//popad
			//ret
			asm2.add_popad({ 0xc3 });

			//==== 跳板1 ====
			//push ecx
			//mov ecx,[esp+4]
			asm1.add_cmd({ 0x51, 0x8b, 0x4c, 0x24, 0x4});
			//mov $xxx,ecx 存储当前跳转
			asm1.add_valset({ 0x89, 0x0d }, &callESP);
			//改写当前的跳板
			//lea ecx, 跳板2
			asm1.add_valset({0x8d, 0x0d }, asm2.GetData());
			//mov [esp+4],ecx
			//pop ecx
			asm1.add_cmd({0x89, 0x4C, 0x24, 0x04, 0x59 });
			//fix code
			asm1.add_cmd({ 0x8b, 0xff, 0x55, 0x8b, 0xec });
			//jmpback hook
			asm1.add_jmp(0xe9, (void*)(PresetAddress + 5));
			asm1.HookForMe(0xe9, PresetAddress);


			//先present，然后再画
			//asm1.add_cmd({ 0x8b, 0xff, 0x55, 0x8b, 0xec });//函数头
			//asm1.add_jmp(0xe9, (void*)(PresetAddress + 5)); //jmp back

			//oldPresent_ptr = (D3DPresent)asm1.GetData();
			//Mem::JmpWrite(PresetAddress, (intptr_t)MyPresentBack);



			////添加一个记录esp的值，以便可以获知是哪个模块调用了函数
			//asm1.add_valset({ 0x89, 0x25 }, &callESP); //mov $xx,esp
			//asm1.add_pushad({ 0x50 }); //pushad, push eax;
			//asm1.add_jmp(0xe8, &MyPresent); //call MyPresent
			//asm1.add_popad({ 0x8b, 0xff, 0x55, 0x8b, 0xec });//popad, fix some
			//asm1.add_jmp(0xe9, (void*)(PresetAddress + 5)); //jmp back
			//asm1.HookForMe(0xe9, PresetAddress);

			//hook D3D endscen
			//AsmHelper asm1;
			////添加一个记录esp的值，以便可以获知是哪个模块调用了函数
			//asm1.add_valset({ 0x89, 0x25 }, &callESP); //mov $xx,esp
			//asm1.add_pushad({ 0x50 }); //pushad, push eax;
			//asm1.add_jmp(0xe8, &MyPresent); //call MyPresent
			////fix code
			//std::vector<uint8_t> tmp;
			//tmp.push_back(0x61);  //popad
			//intptr_t endSenceAddress = *(intptr_t*)(vtable + 0xA8);
			//for (int ii = 0; ii < 7; ii++) tmp.push_back(*(uint8_t*)(endSenceAddress + ii));
			//asm1.add_cmd(tmp);
			//asm1.add_jmp(0xe9, (void*)(endSenceAddress + 7)); //jmpback
			//asm1.HookForMe(0xe9, endSenceAddress); //hook

			

			if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Status::Ok) {
				printf("gdi+ init sucess.\n");
				font.reset(new Font(L"Arial", 24, FontStyleBold));
				brush.reset(new SolidBrush(Color(128, 255, 0, 0)));
			}
			else printf("gdi+ init faild!\n");

		}
	}

	// 用于处理VM_PAINT消息的钩子函数  
	LRESULT CALLBACK VmPaintHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
		CWPSTRUCT* msg = (CWPSTRUCT*)lParam;
		if (msg->message == WM_PAINT) {
			DWORD current = GetTickCount();
			DWORD diff = current - lastCount;
			if (diff >= 15) {
				printf("time diff %d,%d\n", diff, current);
				lastCount = current;
			}
		}
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	//在主消息函数中插入
	bool isrun = false;
	static void _stdcall MessageLoop() {
		//DWORD current = GetTickCount();
		//DWORD diff = current - lastCount;
		//if (diff >= 15) {
		//	printf("time diff %d,%d\n", diff, current);
		//	lastCount = current;
		//}
		//if (!isrun) {
		//	EnumWindows(EnumWindowsProc, 0);
		//	HHOOK hHook = SetWindowsHookExA(WH_CALLWNDPROC, VmPaintHookProc, GetModuleHandle(NULL), GetCurrentThreadId());
		//	if (hHook == NULL) printf("SetWindowsHookEx 失败\n");
		//	else printf("hook sucess!");
		//	isrun = true;
		//}
	}
}

namespace Hook {

    static void Init() {
        if (HMODULE GDI32_DLL = GetModuleHandleW(L"gdi32.dll")) {
            Fun::OldGetGlyphOutlineA = (Type::GetGlyphOutlineA)GetProcAddress(GDI32_DLL, "GetGlyphOutlineA");
        }
        if (HMODULE KERNEL32_DLL = GetModuleHandleW(L"kernel32.dll")) {
            //
            Fun::OldFindFirstFileA = (Type::FindFirstFileA)GetProcAddress(KERNEL32_DLL, "FindFirstFileA");
            Fun::OldCreateFileA = (Type::CreateFileA)GetProcAddress(KERNEL32_DLL, "CreateFileA");
            Fun::OldCreateFileW = (Type::CreateFileW)GetProcAddress(KERNEL32_DLL, "CreateFileW");
        }
        if (DWORD BaseAddr = (DWORD)GetModuleHandleW(NULL)) {
            Dc3wy::jmp_hook_init((intptr_t)BaseAddr);
            Mem::MemWrite(BaseAddr + 0x0E8DB, (void*)&Dc3wy::WdTitleName, 0x04);
            Mem::MemWrite(BaseAddr + 0x0DABF, (void*)&Dc3wy::Description, 0x04);
            Mem::MemWrite(BaseAddr + 0x9DF58, Dc3wy::ChapterTitles, sizeof(Dc3wy::ChapterTitles));
            //Mem::MemWrite(BaseAddr + 0x101A5, &Dc3wy::subtitle::PtrSubWndProc, 0x04);
            Mem::JmpWrite(BaseAddr + 0x31870, (intptr_t)&Dc3wy::jmp_audio_play_hook);
            Mem::JmpWrite(BaseAddr + 0x32490, (intptr_t)&Dc3wy::jmp_audio_stop_hook);

			//hook一个可以获取d3d驱动的位置
			//这里只是启动获取窗口hwnd
			AsmHelper asms;
			asms.add_pushad({ 0x50 });  //pushad, push eax
			asms.add_jmp(0xe8, &Fun::GetD3Deice); //call xx
			asms.add_popad({ 0xff, 0xd2,0x8b, 0x46, 0x20 }); //popad, some fix code
			asms.add_jmp(0xe9, (void*)(BaseAddr + 0x3077F)); //jmp back
			if (!asms.HookForMe(0xe9, BaseAddr + 0x3077A)) {
				printf("GetD3Deice() hook faild!\n");
			}

			//AsmHelper asmLoop;
			//asmLoop.add_cmd({0x60});  //pushad
			//asmLoop.add_jmp(0xe8, &Fun::MessageLoop); //call func
			//asmLoop.add_popad({ 0x50, 0xff, 0xd6, 0x85, 0xc0 }); //popad, push eax, call esi, test eax,eax
			//asmLoop.add_jmp(0xe9, (void*)(BaseAddr + 0x1095E));//jmp back
			//asmLoop.HookForMe(0xe9, BaseAddr + 0x10959);
        }
    }

    static void Attach() {
        DetourTransactionBegin();
        if (Fun::OldGetGlyphOutlineA) {
            DetourAttach((void**)&Fun::OldGetGlyphOutlineA, Fun::NewGetGlyphOutlineA);
        }
        if (Fun::OldFindFirstFileA)   {
            DetourAttach((void**)&Fun::OldFindFirstFileA,   Fun::NewFindFirstFileA);
        }
        if (Fun::OldCreateFileA) {
            DetourAttach((void**)&Fun::OldCreateFileA, Fun::NewCreateFileA);
        }
        if (Fun::OldCreateFileW) {
            DetourAttach((void**)&Fun::OldCreateFileW, Fun::NewCreateFileW);
        }
        DetourUpdateThread(GetCurrentThread());
        DetourTransactionCommit();
    }
}

extern "C" __declspec(dllexport) void hook(void) { }
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) 
    {
    case DLL_PROCESS_ATTACH:
    //#ifdef _DEBUG
        AllocConsole();
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONIN$", "r", stdin);
    //#endif
        Hook::Init();
        Hook::Attach();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}