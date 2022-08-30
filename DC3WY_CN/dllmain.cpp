#include "framework.h"
#include "dc3wy.h"

string szWndName ("【-COKEZIGE汉化组-】Da Capo Ⅲ With You - ");
string verData ("");

HWND WINAPI NewCreateWindowExA(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, 
    int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam){
    return oldCreateWindowExA(dwExStyle, lpClassName, (LPCTSTR)szWndName.c_str(),
        dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

HFONT WINAPI newCreateFontIndirectA(LOGFONTA* lplf) {
    lplf->lfCharSet = 0x86;
    strcpy_s(lplf->lfFaceName, "黑体");
    return oldCreateFontIndirectA(lplf);
}

HANDLE WINAPI NewCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile){
    string newName(lpFileName);
    ReplacePathA(&newName);
    return oldCreateFileA(
        newName.c_str(),  dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI newCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,HANDLE hTemplateFile) {
    wstring newName(lpFileName);
    ReplacePathW(&newName);
    return oldCreateFileW(
            newName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
};

HANDLE WINAPI NewFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    string newName(lpFileName);
    ReplacePathA(&newName);
    return oldFindFirstFileA(newName.c_str(), lpFindFileData);
}

HANDLE WINAPI newMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    if (lpText != 0 && strcmp((const char*)lpText, (const char*)"廔椆偟傑偡偐丠") == 0)
        return oldMessageBoxA(hWnd, "要结束游戏吗？", "提示", uType);
    else return oldMessageBoxA(hWnd, lpText, lpCaption, uType);
}

LRESULT WINAPI newSendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM IParam) {
    const char* oldptr = (const char*)IParam;
    if (oldptr && (strlen(oldptr) == 48)) 
        IParam = (LPARAM)verData.c_str();
    return oldSendMessageA(hWnd, Msg, wParam, IParam);
}


DWORD WINAPI newGetGlyphOutlineA(HDC hdc, UINT uChar, UINT fuFormat, LPGLYPHMETRICS lpgm,
    DWORD cjBuffer, LPVOID pvBuffer, MAT2* lpmat) {
    tagTEXTMETRICA lptm;
    LONG tmHeight;
    GetTextMetricsA(hdc, &lptm);
    tmHeight = lptm.tmHeight;
    Dc3Font* font = GetFontStruct(tmHeight);
    if (uChar == 0xA1EC) {
        uChar = 0x81F4;
        if (!font->jisFont) 
            font->jisFont = CreateFontA(
                tmHeight, tmHeight / 2, 0, 0, 0, 0, 0, 0, 0x80, 4, 0x20, 4, 4, "黑体"
            );
        SelectObject(hdc, font->jisFont);
    }
    else SelectObject(hdc, font->chsFont);
    if (uChar == 0x23) uChar = 0x20;
    return oldGetGlyphOutlineA(hdc, uChar, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat);
}

bool initComplete() {

    const wchar_t* _chsinfo_ = L"cn_Data\\_chsinfo_";
    if (GetFileAttributes(_chsinfo_) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributes(L"cn_Data\\LOGO.crx") == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributes(L"cn_Data\\config.mes") == INVALID_FILE_ATTRIBUTES
        ) return false;

    FILE * _chsinfo_fp_ = _wfopen(_chsinfo_, L"r");
    if (_chsinfo_fp_ == NULL) return false;
    else {
        char *data = new char[100];
        fgets(data, 100, _chsinfo_fp_);
        szWndName.append(data);
        while (fgets(data, 100, _chsinfo_fp_))
            verData.append(data).append("\n");
        delete[] data;
        fclose(_chsinfo_fp_);
    }

    if (_gdi32 == NULL ||_user32 == NULL || _kernel32 == NULL || BaseAddr == NULL)
        return false;
    
    oldCreateFontIndirectA = (_CreateFontIndirectA)GetProcAddress(_gdi32, "CreateFontIndirectA");
    oldCreateWindowExA = (_CreateWindowExA)GetProcAddress(_user32, "CreateWindowExA");
    oldCreateFileA = (_CreateFileA)GetProcAddress(_kernel32, "CreateFileA");
    oldCreateFileW = (_CreateFileW)GetProcAddress(_kernel32, "CreateFileW");
    oldFindFirstFileA = (_FindFirstFileA)GetProcAddress(_kernel32, "FindFirstFileA");
    oldMessageBoxA = (_MessageBoxA)GetProcAddress(_user32, "MessageBoxA");
    oldSendMessageA = (_SendMessageA)GetProcAddress(_user32, "SendMessageA");
    oldGetGlyphOutlineA = (_GetGlyphOutlineA)GetProcAddress(_gdi32, "GetGlyphOutlineA");

    if (oldCreateFontIndirectA == NULL || oldCreateWindowExA == NULL ||
        oldFindFirstFileA == NULL || oldMessageBoxA == NULL ||
        oldCreateFileA == NULL || oldCreateFileW == NULL||
        oldSendMessageA == NULL || oldGetGlyphOutlineA == NULL
        ) return false;

    return true;
}

void initHookLoader() {
    //make_console();
    if (initComplete()) { 

        memcpy((void*)(BaseAddr + 0x9DF58), chapterTitle, sizeof(chapterTitle));
        DetourTransactionBegin();
        //DetourAttach((void**)&oldCreateFontIndirectA, newCreateFontIndirectA);
        DetourAttach(&oldCreateWindowExA, NewCreateWindowExA);
        DetourAttach(&oldCreateFileA, NewCreateFileA);
        DetourAttach(&oldCreateFileW, newCreateFileW);
        DetourAttach(&oldFindFirstFileA, NewFindFirstFileA);
        DetourAttach((void**)&oldMessageBoxA, newMessageBoxA);
        DetourAttach((void**)&oldSendMessageA, newSendMessageA);
        DetourAttach((void**)&oldGetGlyphOutlineA, newGetGlyphOutlineA);
        DetourUpdateThread(GetCurrentThread());
        DetourTransactionCommit();
        return;
    }
    MessageBoxW(NULL, L"补丁初始化失败！", L"警告", MB_OK | MB_ICONINFORMATION);
    exit(0);
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            initHookLoader();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
