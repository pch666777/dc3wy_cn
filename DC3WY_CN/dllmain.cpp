#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <windows.h>
#include <vector>
#include "dc3wy.h"
#include "detours.h"
#pragma comment(lib, "detours.lib")

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

    static bool JmpWrite(uintptr_t orgAddr, uintptr_t tarAddr) {
        uint8_t jmp_write[5] = { 0xE9, 0x0, 0x0, 0x0, 0x0 };
        tarAddr = tarAddr - orgAddr - 5;
        memcpy(jmp_write + 1, &tarAddr, 4);
        return MemWrite(orgAddr, jmp_write, 5);
    }
}

namespace Hook::Type {
    typedef struct { HFONT jis_f; HFONT gbk_f; size_t size; } Font;
    typedef HANDLE(WINAPI* FindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
    typedef DWORD (WINAPI* GetGlyphOutlineA)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, MAT2*);
    typedef HANDLE(WINAPI* CreateFileA)(LPCSTR , DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    typedef HANDLE(WINAPI* CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
}

namespace Hook::Val {
    DWORD BaseAddr;
    std::string  ReplacePathA;
    std::wstring ReplacePathW;
    std::vector<Type::Font*> Fonts;
    HMODULE GDI32_DLL, KERNEL32_DLL;
}

namespace Hook::Def {

    static Type::Font* GetFontStruct(HDC& hdc, tagTEXTMETRICA lptm = {}) {
        GetTextMetricsA(hdc, &lptm);
        size_t size = (size_t)lptm.tmHeight;
        for (Type::Font* f : Val::Fonts) if (f->size == size) return f;
        Type::Font* nf = new Type::Font { nullptr, nullptr, size };
        nf->gbk_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x86, 4, 0x20, 4, 4, "黑体");
        nf->jis_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x80, 4, 0x20, 4, 4, "黑体");
        Val::Fonts.push_back(nf);
        return nf;
    }

    static bool ReplacePathW(std::wstring path) {
        path.assign(path.substr(path.find_last_of(L"\\") + 1)).insert(0, L"cn_Data\\");
        if(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)  return false ;
        return Val::ReplacePathW.assign(path).size();
    }

    static bool ReplacePathA(std::string path) {
        path.assign(path.substr(path.find_last_of("\\") + 1)).insert(0,  "cn_Data\\");
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        return Val::ReplacePathA.assign(path).size();
    }
}

namespace Hook::Fun {

    Type::GetGlyphOutlineA OldGetGlyphOutlineA;
    DWORD WINAPI NewGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuf, LPGLYPHMETRICS lpgm, DWORD cjbf, LPVOID pvbf, MAT2* lpmat) {
        Type::Font* font = Def::GetFontStruct(hdc);
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
    HANDLE WINAPI NewFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
        return OldFindFirstFileA(Def::ReplacePathA(lpFileName) ? Val::ReplacePathA.c_str() : lpFileName, lpFindFileData);
    }

    Type::CreateFileA OldCreateFileA;
    HANDLE WINAPI NewCreateFileA(LPCSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF)  {
        return OldCreateFileA(Def::ReplacePathA(lpFN) ? Val::ReplacePathA.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }

    Type::CreateFileW OldCreateFileW;
    HANDLE WINAPI NewCreateFileW(LPCWSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF) {
        return OldCreateFileW(Def::ReplacePathW(lpFN) ? Val::ReplacePathW.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }
}

namespace Hook {

    struct dc3wy {

        int audio_stop(int a2) {
            using _Stop = int(__stdcall*)(int);
            // ...
            return 0;
        }

        static int __stdcall audio_play(int a1, int a2) {
            char* ptr = ((char*)&a2) + 0x08 + 0x130;
            printf("[argv] a1: %d, a2: %d\n", a1, a2);
            printf("0x%p 0x%p %s\n", &a2, ptr, *(char**)ptr);
            //printf("%s\n", *(char**)name);
            /* if (size_t pos = std::string_view(*(char**)ptr).find_last_of("\\"); pos != std::string::npos) {
                std::string_view _name((*(char**)ptr) + pos + 1);
                std::cout << "name: " << _name << std::endl;
            }*/
            //typedef int(__stdcall* _Play)(int a1, int a2, int a3, int a4);

            using _Play = int(__stdcall*)(int, int, int, int);
            if (a2 == 1) {
                DWORD edx = *(DWORD*)(Val::BaseAddr + 0xA95A4);
                *(DWORD*)(edx + 0x136C) = a1;
            }
            DWORD eax = (DWORD)(Val::BaseAddr + 0xA95EC);
            DWORD* val = (DWORD*)((DWORD*)eax)[a2];
            _Play play = *(_Play*)((*val) + 0x30);
            int result = play((int)val, 0, 0, a1);
            printf("val: 0x%p\n\n", (void*)val);
            return  result;
        }

        static inline intptr_t _audio_stop() {
            using _audio_stop = int(dc3wy::*)(int);
            _audio_stop f_ptr = &dc3wy::audio_stop;
            return (intptr_t)&f_ptr;
        }
    };

    static void Init() {
        if (Val::GDI32_DLL = GetModuleHandleA("gdi32.dll")) {
            Fun::OldGetGlyphOutlineA = (Type::GetGlyphOutlineA)GetProcAddress(Val::GDI32_DLL, "GetGlyphOutlineA");
        }
        if (Val::KERNEL32_DLL = GetModuleHandleA("kernel32.dll")) {
            Fun::OldFindFirstFileA = (Type::FindFirstFileA)GetProcAddress(Val::KERNEL32_DLL, "FindFirstFileA");
            Fun::OldCreateFileA = (Type::CreateFileA)GetProcAddress(Val::KERNEL32_DLL, "CreateFileA");
            Fun::OldCreateFileW = (Type::CreateFileW)GetProcAddress(Val::KERNEL32_DLL, "CreateFileW");
        }
        if (Val::BaseAddr = (DWORD)GetModuleHandle(NULL)) {
            Mem::MemWrite(Val::BaseAddr + 0x0E8DB, (void*)&Dc3wy::WdTitleName, 4);
            Mem::MemWrite(Val::BaseAddr + 0x0DABF, (void*)&Dc3wy::Description, 4);
            Mem::MemWrite(Val::BaseAddr + 0x9DF58, Dc3wy::ChapterTitles, sizeof(Dc3wy::ChapterTitles));
            
            Mem::JmpWrite(Val::BaseAddr + 0x31870, (intptr_t)&dc3wy::audio_play);
            //Mem::JmpWrite(Val::BaseAddr + 0x32490, (intptr_t)dc3wy::_audio_stop());
        }
    }

    static void Start() {
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
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved){
    switch (ul_reason_for_call) 
    {
    case DLL_PROCESS_ATTACH:
    //#ifdef _DEBUG
        AllocConsole();
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONIN$", "r", stdin);
    //#endif
        Hook::Init();
        Hook::Start();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}