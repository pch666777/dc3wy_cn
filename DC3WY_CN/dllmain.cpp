#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <windows.h>
#include <fstream>
#include <vector>
#include "detours.h"
#include "dc3wy.h"
#pragma comment(lib, "detours.lib")

namespace Hook::Mem {

    bool MemWrite(uintptr_t Addr, void* Buf, size_t Size) {
        DWORD OldPro;
        SIZE_T wirteBytes = 0;
        if (VirtualProtect((VOID*)Addr, Size, PAGE_EXECUTE_READWRITE, &OldPro)) {
            WriteProcessMemory(INVALID_HANDLE_VALUE, (VOID*)Addr, Buf, Size, &wirteBytes);
            VirtualProtect((VOID*)Addr, Size, OldPro, &OldPro);
            return Size == wirteBytes;
        }
        return false;
    }

    bool JmpWrite(uintptr_t orgAddr, uintptr_t tarAddr) {
        uint8_t jmp_write[5] = { 0xe9, 0x0, 0x0, 0x0, 0x0 };
        tarAddr = tarAddr - orgAddr - 5;
        memcpy(jmp_write + 1, &tarAddr, 4);
        return MemWrite(orgAddr, jmp_write, 5);
    }
}

namespace Hook::type {
    typedef struct { HFONT jis_f; HFONT gbk_f; size_t size; } font;
    typedef HANDLE(WINAPI* FindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
    typedef DWORD(WINAPI* GetGlyphOutlineA)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, MAT2*);
    typedef HANDLE(WINAPI* CreateFileA)(LPCSTR , DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    typedef HANDLE(WINAPI* CreateFileW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
}

namespace Hook::val {
    DWORD BaseAddr;
    std::string replacePathA;
    std::wstring replacePathW;
    std::vector<type::font*> fonts;
    HMODULE gdi32_dll, kernel32_dll;
}

namespace Hook::def {

    type::font* GetFontStruct(HDC& hdc, tagTEXTMETRICA lptm = {}) {
        GetTextMetricsA(hdc, &lptm);
        size_t size = (size_t)lptm.tmHeight;
        for (type::font* f : val::fonts) if (f->size == size) return f;
        type::font* nf = new type::font{ nullptr, nullptr, size };
        nf->gbk_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x86, 4, 0x20, 4, 4, "黑体");
        nf->jis_f = CreateFontA(size, size / 2, 0, 0, 0, 0, 0, 0, 0x80, 4, 0x20, 4, 4, "黑体");
        val::fonts.push_back(nf);
        return nf;
    }

    bool ReplacePathW(std::wstring path) {
        path.assign(path.substr(path.find_last_of(L"\\") + 1)).insert(0, L"cn_Data\\");
        if(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        val::replacePathW.assign(path);
        return true;
    }

    bool ReplacePathA(std::string path) {
        path.assign(path.substr(path.find_last_of("\\") + 1)).insert(0, "cn_Data\\");
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
        val::replacePathA.assign(path);
        return true;
    }
}

namespace Hook::fun {

    type::GetGlyphOutlineA OldGetGlyphOutlineA;
    DWORD WINAPI NewGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuf, LPGLYPHMETRICS lpgm, DWORD cjbf, LPVOID pvbf, MAT2* lpmat) {
        type::font* font = def::GetFontStruct(hdc);
        if (uChar == 0xA1EC) { // 替换♪
            uChar = 0x81F4;
            SelectObject(hdc, font->jis_f);
        }
        else {
            // 替换半角空格
            if (uChar == 0x23) uChar = 0x20;
            SelectObject(hdc, font->gbk_f);
        }
        return OldGetGlyphOutlineA(hdc, uChar, fuf, lpgm, cjbf, pvbf, lpmat);
    }

    type::FindFirstFileA OldFindFirstFileA;
    HANDLE WINAPI NewFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
        return OldFindFirstFileA(def::ReplacePathA(lpFileName) ? val::replacePathA.c_str() : lpFileName, lpFindFileData);
    }

    type::CreateFileA OldCreateFileA;
    HANDLE WINAPI NewCreateFileA(LPCSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF) {
        return OldCreateFileA(def::ReplacePathA(lpFN) ? val::replacePathA.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }

    type::CreateFileW OldCreateFileW;
    HANDLE WINAPI NewCreateFileW(LPCWSTR lpFN, DWORD dwDA, DWORD dwSM, LPSECURITY_ATTRIBUTES lpSA, DWORD dwCD, DWORD dwFAA, HANDLE hTF) {
        return OldCreateFileW(def::ReplacePathW(lpFN) ? val::replacePathW.c_str() : lpFN, dwDA, dwSM, lpSA, dwCD, dwFAA, hTF);
    }
}

namespace Hook {

    void init() {
        val::BaseAddr = (DWORD)GetModuleHandle(NULL);
        Mem::MemWrite(val::BaseAddr + 0x0E8DB, (void*)&dc3wy::wTitle_name, 4);
        Mem::MemWrite(val::BaseAddr + 0x0DABF, (void*)&dc3wy::description, 4);
        Mem::MemWrite(val::BaseAddr + 0x9DF58, dc3wy::chapter_titles, sizeof(dc3wy::chapter_titles));
        if (val::gdi32_dll = GetModuleHandleA("gdi32.dll")) {
            fun::OldGetGlyphOutlineA = (type::GetGlyphOutlineA)GetProcAddress(val::gdi32_dll, "GetGlyphOutlineA");
        }
        if (val::kernel32_dll = GetModuleHandleA("kernel32.dll")) {
            fun::OldFindFirstFileA = (type::FindFirstFileA)GetProcAddress(val::kernel32_dll, "FindFirstFileA");
            fun::OldCreateFileA = (type::CreateFileA)GetProcAddress(val::kernel32_dll, "CreateFileA");
            fun::OldCreateFileW = (type::CreateFileW)GetProcAddress(val::kernel32_dll, "CreateFileW");
        }
    }

    void start() {
        DetourTransactionBegin();
        if (fun::OldGetGlyphOutlineA) {
            DetourAttach((void**)&fun::OldGetGlyphOutlineA, fun::NewGetGlyphOutlineA);
        }
        if (fun::OldFindFirstFileA)   {
            DetourAttach((void**)&fun::OldFindFirstFileA,  fun::NewFindFirstFileA);
        }
        if (fun::OldCreateFileA) {
            DetourAttach((void**)&fun::OldCreateFileA, fun::NewCreateFileA);
        }

        if (fun::OldCreateFileW) {
            DetourAttach((void**)&fun::OldCreateFileW, fun::NewCreateFileW);
        }
        DetourUpdateThread(GetCurrentThread());
        DetourTransactionCommit();
    }
}

extern "C" __declspec(dllexport) void hook(void) {}
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved){
    switch (ul_reason_for_call) 
    {
    case DLL_PROCESS_ATTACH:
    #ifdef _DEBUG
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONIN$", "r", stdin);
    #endif
        Hook::init();
        Hook::start();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

