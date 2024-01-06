#include <iostream>
#include <thread>
#include <windows.h>
#include <dsound.h>
#include "dc3wy.h"

namespace Dc3wy::subtitle {

    HWND SubtitleWnd = NULL;
    intptr_t PtrSubWndProc = NULL;
    IDirectSoundBuffer* pDsBuffer;
    using WndProc = LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
    static WndProc MainWndProc = NULL;
    static double playedTime = 0.0l;
    static HWND hStaticText;



    LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CREATE: {
            // 在窗口创建时创建字体
            LOGFONT lf;
            ZeroMemory(&lf, sizeof(LOGFONT));
            lf.lfHeight = 20; // 字体高度
            lf.lfWeight = FW_NORMAL; // 字体粗细
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
            lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            lf.lfQuality = DEFAULT_QUALITY;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
            wcscpy_s(lf.lfFaceName, L"黑体"); // 字体名称

            HFONT hFont = CreateFontIndirect(&lf);
            if (hFont != NULL) {
                // 将字体句柄保存到窗口数据中
                SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)hFont);
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_SIZE: {

            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // 设置文本颜色
            SetTextColor(hdc, RGB(0, 0, 0));

            // 设置文本背景颜色为透明
            SetBkMode(hdc, TRANSPARENT);

            // 获取客户区域的大小
            RECT clientRect, mainClientRect;
            HWND hWndMain = GetParent(hWnd); // 获取主窗口句柄
            GetClientRect(hWndMain, &mainClientRect);
            GetClientRect(hWnd, &clientRect);

            // 获取字体句柄
            HFONT hFont = (HFONT)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            if (hFont != NULL) {
                // 选择字体
                HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                wchar_t buffer[100];
                swprintf_s(buffer, L"PlayedTime: %f", playedTime / 10);
                TextOut(hdc, 10, 20, buffer, wcslen(buffer));
                // 恢复原来的字体
                SelectObject(hdc, hOldFont);
            }
            EndPaint(hWnd, &ps);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }


    LRESULT CALLBACK SubWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_CREATE) { 
            SubtitleWnd = hWnd; 
            // 在窗口创建时创建字体
            LOGFONT lf;
            ZeroMemory(&lf, sizeof(LOGFONT));
            lf.lfHeight = 25; // 字体高度
            lf.lfWeight = FW_NORMAL; // 字体粗细
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
            lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            lf.lfQuality = DEFAULT_QUALITY;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
            wcscpy_s(lf.lfFaceName, L"黑体"); // 字体名称

            HFONT hFont = CreateFontIndirect(&lf);
            if (hFont != NULL) {
                // 将字体句柄保存到窗口数据中
                SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)hFont);
            }
            WNDCLASS childWndClass = { 0 };
            childWndClass.lpfnWndProc = ChildWndProc;
            childWndClass.hInstance = GetModuleHandle(NULL);
            childWndClass.lpszClassName = L"ChildWndClass";
            RegisterClass(&childWndClass);
            // 创建一个静态文本控件
            hStaticText = CreateWindowExW(0, L"ChildWndClass", L"Child Window", WS_CHILD | WS_VISIBLE,
                0, 0, 300, 60,
                hWnd, NULL, GetModuleHandle(NULL), NULL);

            // 设置控件透明背景
            SetWindowLongPtr(hStaticText, GWL_EXSTYLE, GetWindowLongPtr(hStaticText, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hStaticText, RGB(0, 0, 255), 0, LWA_COLORKEY);

        }
        if (message == 114514) {
            //PAINTSTRUCT ps;
            //HDC hdc = BeginPaint(hStaticText, &ps);

            //// 设置文本颜色
            //SetTextColor(hdc, RGB(0, 0, 0));

            //// 设置文本背景颜色为透明
            //SetBkMode(hdc, TRANSPARENT);

            //// 获取客户区域的大小
            //RECT clientRect, mainClientRect;
            //HWND hWndMain = GetParent(hWnd); // 获取主窗口句柄
            //GetClientRect(hWndMain, &mainClientRect);
            //GetClientRect(hWnd, &clientRect);

            //// 获取客户区域的宽度和高度
            //int clientWidth = mainClientRect.right - mainClientRect.left;
            //int clientHeight = mainClientRect.bottom - mainClientRect.top;

            //wchar_t buffer[100];
            //swprintf_s(buffer, L"PlayedTime: %f", playedTime / 10);
            //TextOut(hdc, 0, 30, buffer, wcslen(buffer));

            //EndPaint(hWnd, &ps);
            //std::cout << " 进来了？？？" << std::endl;
            PostMessageA(hStaticText, WM_PAINT, NULL, NULL);
            printf("\r[In SubWndProc] PlayedTime: %f", playedTime / 10);

        }
        else {
            return MainWndProc(hWnd, message, wParam, lParam);
        }
    }
    
    static void init(intptr_t base) {
        Dc3wy::subtitle::PtrSubWndProc = (intptr_t)SubWndProc;
        Dc3wy::subtitle::MainWndProc = (WndProc)(base + 0x0FC20);
    }

    static void run() {
        if(!pDsBuffer) return;
        WAVEFORMATEX format{};
        pDsBuffer->GetFormat(&format, sizeof(WAVEFORMATEX), NULL);
        // 计算采样率和帧大小
        DWORD samplesPerSecond = format.nSamplesPerSec;
        DWORD bytesPerSample = format.wBitsPerSample / 8;
        DWORD channels = format.nChannels;
        while (pDsBuffer) {
            // 获取当前播放位置
            DWORD playCursor, writeCursor;
            pDsBuffer->GetCurrentPosition(&playCursor, &writeCursor);
            DWORD playedSamples = (playCursor * 8) / (channels * bytesPerSample);
            playedTime = static_cast<double>(playedSamples) / static_cast<double>(samplesPerSecond);
            PostMessageA(SubtitleWnd, 114514, NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        printf("\n\n");
    }

    static void test() {
        std::thread(subtitle::run).detach();
    }

    void destroy() {

    }
}

namespace Dc3wy {

    static DWORD dword_a95a4 = 0x00;
    static DWORD dword_a95ec = 0x00;
    static char* current_audio_name = nullptr;
    using Sub32000 = int(__thiscall*)(void*, int);
    static Sub32000 sub32000 = nullptr;

    struct hook {
        
        using PDSB = IDirectSoundBuffer*;
        
        static int __stdcall audio_play(int a1, int a2) {
            /*printf(
                "[audio_play] v1: 0x%02X v2: 0x%02X file: %s\n", 
                a1, a2, current_audio_name
            );*/
            if (a2 == 1) {
                DWORD Buf = *(DWORD*)dword_a95a4;
                *(DWORD*)(Buf + 0x136C) = a1;
            }
            if (PDSB pDSB = (PDSB)((DWORD*)dword_a95ec)[a2]) {
                if (!strncmp("music", current_audio_name, 5)) {
                    printf("Current: [%s]\n", current_audio_name);
                    subtitle::pDsBuffer = pDSB;
                    subtitle::test();
                }
                return pDSB->Play(0, 0, a1 != 0);
            }
            return NULL;
        }

        int __thiscall audio_stop(int a2) {
            DWORD Buf = *(DWORD*)dword_a95a4;
            if ((unsigned int)(a2 - 1) <= 3 && Buf) {
                *((BYTE*)Buf + 0x20 * a2 + 0x4D10) = 0;
            }
            ((DWORD*)this)[0x05 * a2 + 0x15] = 0;
            if (PDSB pDSB = (PDSB)((DWORD*)dword_a95ec)[a2]) {
                if (pDSB == subtitle::pDsBuffer) {
                    subtitle::pDsBuffer = nullptr;
                }
                pDSB->SetCurrentPosition(0x00);
                pDSB->Stop();
                return sub32000(this, a2);
            }
            return NULL;
        }
    };

    __declspec(naked) void jmp_audio_play_hook() {
        __asm {
            push eax
            mov eax, dword ptr ss:[esp + 0x130]
            mov current_audio_name, eax
            mov eax, dword ptr ss:[esp]
            add esp, 0x04
            jmp hook::audio_play
        }
    }

    __declspec(naked) void jmp_audio_stop_hook() {
        __asm jmp hook::audio_stop
    }


    void jmp_hook_init(intptr_t base) {
        Dc3wy::sub32000 = (Sub32000)(base + 0x32000);
        Dc3wy::dword_a95a4 = base + 0xA95A4;
        Dc3wy::dword_a95ec = base + 0xA95EC;
        subtitle::init(base);
    }
    
}

