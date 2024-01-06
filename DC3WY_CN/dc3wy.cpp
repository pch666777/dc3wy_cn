#include <iostream>
#include <thread>
#include <windows.h>
#include <dsound.h>
#include "dc3wy.h"

namespace Dc3wy::subtitle {
    
    void init() {

    }

    IDirectSoundBuffer* pDsBuffer;

    static void run() {
        if(!pDsBuffer) return;
        WAVEFORMATEX format{};
        // 计算采样率和帧大小
        pDsBuffer->GetFormat(&format, sizeof(WAVEFORMATEX), NULL);
        DWORD samplesPerSecond = format.nSamplesPerSec;
        DWORD bytesPerSample = format.wBitsPerSample / 8;
        DWORD channels = format.nChannels;
        while (pDsBuffer) {
            // 获取当前播放位置
            DWORD playCursor, writeCursor;
            pDsBuffer->GetCurrentPosition(&playCursor, &writeCursor);
            DWORD playedSamples = (playCursor * 8) / (channels * bytesPerSample);
            double playedTime = static_cast<double>(playedSamples) / static_cast<double>(samplesPerSecond);
            printf("\rplayedTime: %f", playedTime / 10);
        }
        std::cout << std::endl;
    }

    static void test() {
        std::thread testThread(subtitle::run);
        testThread.detach();
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
                    printf("%s\n", current_audio_name);
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
    }
    
}

