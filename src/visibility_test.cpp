#include "visibility.cpp"
#include <cassert>
#include <cstdio>
bool enabled=false;
bool vrVisibilityActive(){return enabled;}
void vrLog(const char*,...){}
unsigned expected=63,calls=0;
void* expectedObject=nullptr;
void __fastcall fakeProcess(void* process,void*,void* object) {
    assert(object==expectedObject);
    auto bytes=static_cast<unsigned char*>(process);
    auto& mask=*reinterpret_cast<unsigned*>(bytes+0x90);
    assert(mask==expected);++calls;
    mask=17; // Engine recursion may change the mask temporarily.
}
int main() {
    alignas(4) unsigned char state[0x94]{};
    int object=0,camera=0;expectedObject=&object;
    *reinterpret_cast<void**>(state+0x10)=&camera;
    auto& mask=*reinterpret_cast<unsigned*>(state+0x90);
    originalProcess=reinterpret_cast<Process>(fakeProcess);
    mask=63;enabled=false;expected=63;
    processVisible(state,nullptr,&object);assert(mask==17); // Disabled forwards untouched.
    mask=63;enabled=true;expected=0;
    processVisible(state,nullptr,&object);assert(mask==63); // Restore after traversal.
    mask=5;processVisible(state,nullptr,&object);assert(mask==5); // Preserve partial mask.
    state[0x2c]=1;mask=63;expected=63;
    processVisible(state,nullptr,&object);assert(mask==17); // Shadow camera untouched.
    state[0x2c]=0;*reinterpret_cast<void**>(state+0x10)=nullptr;mask=63;
    processVisible(state,nullptr,&object);assert(mask==17);
    assert(calls==5&&expanded==2&&shadowPasses==1);
    puts("PASS: x86 culling hook ABI, perspective bypass, mask restoration, VR-off/orthographic/no-camera passthrough");
}
