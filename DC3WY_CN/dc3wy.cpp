#include <iostream>
#include <windows.h>
#include "dc3wy.h"

namespace Dc3wy {

    static DWORD dword_a95a4 = 0x00;
    static DWORD dword_a95ec = 0x00;
    static char* current_audio_name = nullptr;
    using Sub32000 = int(__thiscall*)(void*, int);
    using DsCdPlay = int(__stdcall*)(int, int, int, int);
    using DsCdStop = int(__stdcall*)(int);
    using DsSetPos = int(__stdcall*)(int, int);
    static Sub32000 sub_32000 = nullptr;
    static DsCdPlay pDsCdPlay = nullptr;
    static DsCdStop pDsCdStop = nullptr;
    static DsSetPos pDsSetPos = nullptr;
    static DWORD BaseAddr = NULL;

    struct dc3wy {

        static int __stdcall audio_play(int a1, int a2) {
            printf("%s\n", current_audio_name);
            if (a2 == 1) {
                DWORD Buf = *(DWORD*)dword_a95a4;
                *(DWORD*)(Buf + 0x136C) = a1;
            }
            DWORD* val = (DWORD*)((DWORD*)dword_a95ec)[a2];
            pDsCdPlay = *(DsCdPlay*)((*val) + 0x30);
            return pDsCdPlay((DWORD)val, 0, 0, a1);
        }

        int __thiscall audio_stop(int a2) {
         
        }
    };

    __declspec(naked) void jmp_audio_play_hook() {
        __asm {
            push eax
            mov eax, dword ptr ss:[esp + 0x130]
            mov current_audio_name, eax
            mov eax, dword ptr ss:[esp]
            add esp, 0x04
            jmp dc3wy::audio_play
        }
    }

    __declspec(naked) void jmp_audio_stop_hook() {
        __asm jmp dc3wy::audio_stop
    }

    void jmp_hook_init(DWORD base) {
        Dc3wy::sub_32000   = (Sub32000)(base + 0x32000);
        Dc3wy::dword_a95a4 = base + 0xA95A4;
        Dc3wy::dword_a95ec = base + 0xA95EC;
        Dc3wy::BaseAddr = base;
    }
}

