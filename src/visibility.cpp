#include <windows.h>
#include <atomic>
#include <cstring>
#include "visibility.h"
#include "vr.h"

namespace {
// Verified against this game's running x86 image, not a layout from another game.
// NiCullingProcess: camera +0x10, copied frustum +0x14 (ortho +0x2c),
// six planes +0x30, active plane mask +0x90. Process(object) is vtable slot 2.
using Process=void(__thiscall*)(void*,void*);
Process originalProcess=nullptr;
std::atomic<unsigned> examined{0},expanded{0},shadowPasses{0};
void __fastcall processVisible(void* process,void*,void* object) {
    ++examined;
    auto bytes=static_cast<unsigned char*>(process);
    if(vrVisibilityActive()&&*reinterpret_cast<void**>(bytes+0x10)&&bytes[0x2c]==0) {
        auto& mask=*reinterpret_cast<unsigned*>(bytes+0x90);
        const unsigned saved=mask;
        // Zero invokes the engine's existing fully-visible traversal branch. Keep
        // application-hidden flags, node traversal, LOD and geometry append intact.
        mask=0;++expanded;
        originalProcess(process,object);
        mask=saved;
    } else {
        if(bytes[0x2c]!=0)++shadowPasses;
        originalProcess(process,object);
    }
}
}
void installVisibilityHook() {
    static bool attempted=false;if(attempted)return;attempted=true;
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||pe->FileHeader.TimeDateStamp!=0x510c67e9||pe->OptionalHeader.SizeOfImage!=0xc35000) {
        vrLog("Visibility hook skipped: unsupported executable version");return;
    }
    const unsigned char processSignature[]={0x53,0x56,0x8b,0xf1,0x8b,0x9e,0x90,0,0,0,0x85,0xdb,0x75,0x14};
    const unsigned char traversalSignature[]={0xf6,0x81,0xa8,0,0,0,1,0x75,0x11};
    const unsigned char maskSignature[]={0xc7,0x83,0x90,0,0,0,0x3f,0,0,0};
    auto slot=reinterpret_cast<void**>(base+0x5f9abc);
    if(memcmp(base+0x150cc0,processSignature,sizeof(processSignature))||
       memcmp(base+0x129190,traversalSignature,sizeof(traversalSignature))||
       memcmp(base+0x150daa,maskSignature,sizeof(maskSignature))||*slot!=base+0x150cc0) {
        vrLog("Visibility hook skipped: code or vtable signature mismatch");return;
    }
    DWORD protection=0;
    if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)){vrLog("Visibility hook failed: vtable protection");return;}
    originalProcess=reinterpret_cast<Process>(*slot);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(processVisible));
    DWORD ignored;VirtualProtect(slot,sizeof(void*),protection,&ignored);
    vrLog("Visibility hook installed: verified NiCullingProcess slot 2; perspective bypass only while VR/F10 enabled");
}
void reportVisibilityStats() {
    vrLog("Visibility calls=%u expanded=%u orthographic=%u enabled=%d",examined.exchange(0),expanded.exchange(0),shadowPasses.exchange(0),vrVisibilityActive());
}
