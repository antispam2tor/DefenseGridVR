#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <map>
#include <string>
#include <atomic>
#include <regex>
#include "vr.h"
#include "matrix.h"
#include "visibility.h"

// Diagnostic stage: forwards rendering unchanged and records camera evidence.
namespace {
HMODULE runtime;
std::once_flag loaded;
std::recursive_mutex guard;
std::map<std::pair<void**,unsigned>,void*> originals;
std::atomic<unsigned> frames{0}, shaders{0};
unsigned captureBudget=0;
bool keyWasDown=false;
std::wstring directory;
struct CameraRegisters { int combined=-1,view=-1,projection=-1,inverse=-1,reticle=-1,eye=-1,bones=-1; };
std::map<IDirect3DVertexShader9*,CameraRegisters> cameraRegisters;
bool captureDraws=false;
std::map<IDirect3DVertexShader9*,unsigned> drawCounts;
void log(const char* fmt, ...) {
    std::lock_guard<std::recursive_mutex> lock(guard);
    FILE* f=nullptr; _wfopen_s(&f,(directory+L"dg1-camera.log").c_str(),L"a");
    if (!f) return;
    va_list args; va_start(args,fmt); vfprintf(f,fmt,args); va_end(args);
    fputc('\n',f); fclose(f);
}
void init() {
    std::call_once(loaded,[] {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr,path,MAX_PATH);
        directory=path; directory.resize(directory.find_last_of(L"\\/")+1);
        GetSystemDirectoryW(path,MAX_PATH);
        std::wstring full=std::wstring(path)+L"\\d3d9.dll";
        runtime=LoadLibraryW(full.c_str());
        log("DG1 camera probe v1; system runtime=%p; F8 captures constants",runtime);
    });
}
template<class T> T proc(const char* name) { init(); return reinterpret_cast<T>(GetProcAddress(runtime,name)); }
template<class T> T original(void* object,unsigned slot) {
    std::lock_guard<std::recursive_mutex> lock(guard);
    return reinterpret_cast<T>(originals.at({*reinterpret_cast<void***>(object),slot}));
}
bool hook(void* object,unsigned slot,void* replacement) {
    std::lock_guard<std::recursive_mutex> lock(guard);
    auto table=*reinterpret_cast<void***>(object);
    if(originals.count({table,slot})) return true;
    DWORD old=0;
    if(!VirtualProtect(table+slot,sizeof(void*),PAGE_EXECUTE_READWRITE,&old)) return false;
    originals[{table,slot}]=table[slot];
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(table+slot),replacement);
    DWORD ignored; VirtualProtect(table+slot,sizeof(void*),old,&ignored);
    return true;
}
using KeyboardState=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPVOID);
HRESULT STDMETHODCALLTYPE keyboardState(IDirectInputDevice8W* device,DWORD bytes,LPVOID data) {
    auto hr=original<KeyboardState>(device,9)(device,bytes,data);
    if(SUCCEEDED(hr)&&bytes==256&&data) {
        auto keys=static_cast<BYTE*>(data);
        if(vrTowerControlsActive()){keys[DIK_LEFT]=0;keys[DIK_RIGHT]=0;keys[DIK_LBRACKET]=0;keys[DIK_RBRACKET]=0;}
        if(vrBirdControlsActive()){keys[DIK_UP]=0;keys[DIK_DOWN]=0;}
    }
    return hr;
}
using KeyboardData=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
HRESULT STDMETHODCALLTYPE keyboardData(IDirectInputDevice8W* device,DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags) {
    auto hr=original<KeyboardData>(device,10)(device,size,data,count,flags);
    if(SUCCEEDED(hr)&&data&&count&&size>=sizeof(DIDEVICEOBJECTDATA)&&(vrTowerControlsActive()||vrBirdControlsActive())) {
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(device->GetCapabilities(&caps))&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_KEYBOARD) {
            for(DWORD i=0;i<*count;++i) {
                auto entry=reinterpret_cast<DIDEVICEOBJECTDATA*>(reinterpret_cast<BYTE*>(data)+i*size);
                // Forward releases so the game's original camera cannot retain a held arrow.
                if(vrTowerControlsActive()&&(entry->dwOfs==DIK_LEFT||entry->dwOfs==DIK_RIGHT||entry->dwOfs==DIK_LBRACKET||entry->dwOfs==DIK_RBRACKET))entry->dwData=0;
                if(vrBirdControlsActive()&&(entry->dwOfs==DIK_UP||entry->dwOfs==DIK_DOWN))entry->dwData=0;
            }
        }
    }
    return hr;
}
void installKeyboardHooks() {
    static std::once_flag installed;
    std::call_once(installed,[] {
        wchar_t path[MAX_PATH]{};GetSystemDirectoryW(path,MAX_PATH);
        auto module=LoadLibraryW((std::wstring(path)+L"\\dinput8.dll").c_str());
        if(!module){log("Keyboard hook: cannot load system DirectInput");return;}
        using Create=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
        auto create=reinterpret_cast<Create>(GetProcAddress(module,"DirectInput8Create"));
        // A/W interfaces have identical ABI for the methods used here (no strings).
        const GUID interfaces[]={IID_IDirectInput8W,IID_IDirectInput8A};
        for(const auto& iid:interfaces) {
        IDirectInput8W* input=nullptr;IDirectInputDevice8W* keyboard=nullptr;
        if(create&&SUCCEEDED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,iid,reinterpret_cast<void**>(&input),nullptr))) {
            if(SUCCEEDED(input->CreateDevice(GUID_SysKeyboard,&keyboard,nullptr))) {
                log("Tower keyboard hooks state=%d buffered=%d",hook(keyboard,9,(void*)keyboardState),hook(keyboard,10,(void*)keyboardData));
                keyboard->Release();
            }
            input->Release();
        }
        }
        // Keep the module loaded: hooked vtables and original function pointers live in it.
    });
}
using Present=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
HRESULT STDMETHODCALLTYPE present(IDirect3DDevice9* d,const RECT* a,const RECT* b,HWND w,const RGNDATA* r) {
    if(captureDraws) {
        log("DRAW_COUNTS frame=%u",frames.load());
        for(const auto& entry:drawCounts)log(" DRAW shader=%p calls=%u",entry.first,entry.second);
        drawCounts.clear();captureDraws=false;
    }
    vrPresent(d);
    auto frame=++frames;
    if(frame==1||frame%600==0)reportVisibilityStats();
    bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
    { std::lock_guard<std::recursive_mutex> lock(guard);
      if((down&&!keyWasDown)||frame==600) { captureBudget=240;captureDraws=true; log("CAPTURE frame=%u",frame); }
      keyWasDown=down;
    }
    if(frame==1||frame==600) log("Present frame=%u",frame);
    return original<Present>(d,17)(d,a,b,w,r);
}
using Constants=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,const float*,UINT);
HRESULT STDMETHODCALLTYPE constants(IDirect3DDevice9* d,UINT first,const float* values,UINT count) {
    { std::lock_guard<std::recursive_mutex> lock(guard);
      if(captureBudget&&values&&count<=256) {
        --captureBudget;
        IDirect3DVertexShader9* shader=nullptr; d->GetVertexShader(&shader);
        log("VS_CONSTANT frame=%u shader=%p c%u count=%u",frames.load(),shader,first,count);
        if(shader) shader->Release();
        for(UINT i=0;i<count;++i) log(" c%u %.9g %.9g %.9g %.9g",first+i,values[4*i],values[4*i+1],values[4*i+2],values[4*i+3]);
      }
    }
    return original<Constants>(d,94)(d,first,values,count);
}
using CreateShader=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,const DWORD*,IDirect3DVertexShader9**);
HRESULT STDMETHODCALLTYPE createShader(IDirect3DDevice9* d,const DWORD* code,IDirect3DVertexShader9** out) {
    HRESULT hr=original<CreateShader>(d,91)(d,code,out);
    if(SUCCEEDED(hr)&&out&&*out) {
        UINT size=0; (*out)->GetFunction(nullptr,&size);
        unsigned id=++shaders;
        log("VERTEX_SHADER id=%u pointer=%p bytes=%u",id,*out,size);
        // Bytecode from GetFunction, never scan beyond the driver's reported length.
        if(size&&size<1024*1024&&id<=512) {
            ID3DBlob* text=nullptr;
            if(SUCCEEDED(D3DDisassemble(code,size,0,nullptr,&text))) {
                std::string assembly(static_cast<const char*>(text->GetBufferPointer()),text->GetBufferSize());
                std::smatch match;
                std::lock_guard<std::recursive_mutex> lock(guard);
                cameraRegisters.erase(*out);
                auto findRegister=[&](const char* name,int size=4) {
                    return std::regex_search(assembly,match,std::regex(std::string("//[ \\t]+")+name+"[ \\t]+c([0-9]+)[ \\t]+"+std::to_string(size)+"[ \\t\\r\\n]"))?std::stoi(match[1].str()):-1;
                };
                CameraRegisters registers;
                registers.combined=findRegister("g_ViewProj");
                registers.view=findRegister("g_matView");
                registers.projection=findRegister("g_matProj");
                registers.inverse=findRegister("g_InvViewProj");registers.reticle=findRegister("ReticlePosition",1);registers.eye=findRegister("g_EyePos",1);registers.bones=findRegister("g_Bones",60);
                if(registers.combined>=0||registers.view>=0&&registers.projection>=0) {
                    cameraRegisters[*out]=registers;
                    log("CAMERA_REGISTERS shader=%p combined=%d view=%d projection=%d",*out,registers.combined,registers.view,registers.projection);
                }
                auto name=directory+L"dg1-shader-"+std::to_wstring(id)+L".txt";
                FILE* f=nullptr; _wfopen_s(&f,name.c_str(),L"wb");
                if(f) { fwrite(text->GetBufferPointer(),1,text->GetBufferSize(),f); fclose(f); }
                text->Release();
            }
        }
    }
    return hr;
}
struct CameraDraw {
    IDirect3DDevice9* d; UINT reg=0,projectionReg=0; float saved[16]{},savedProjection[16]{}; bool changed=false,projectionChanged=false;
    explicit CameraDraw(IDirect3DDevice9* device):d(device) {
        IDirect3DVertexShader9* shader=nullptr;
        if(FAILED(d->GetVertexShader(&shader))||!shader)return;
        if(captureDraws)++drawCounts[shader];
        bool found=false;
        CameraRegisters registers;
        { std::lock_guard<std::recursive_mutex> lock(guard);
          auto it=cameraRegisters.find(shader);if(it!=cameraRegisters.end()){registers=it->second;found=true;} }
        shader->Release();
        if(!found)return;
        reg=static_cast<UINT>(registers.combined>=0?registers.combined:registers.view);
        if(FAILED(d->GetVertexShaderConstantF(reg,saved,4)))return;
        camera::Matrix sceneView;
        if(registers.combined>=0&&camera::extractView(saved,sceneView)&&registers.bones>=0&&registers.inverse>=0&&registers.reticle>=0&&registers.eye>=0) {
            float bones[12],inverse[16],reticle[4],eyePosition[4];
            if(SUCCEEDED(d->GetVertexShaderConstantF(registers.bones,bones,3))&&SUCCEEDED(d->GetVertexShaderConstantF(registers.inverse,inverse,4))&&SUCCEEDED(d->GetVertexShaderConstantF(registers.reticle,reticle,1))&&SUCCEEDED(d->GetVertexShaderConstantF(registers.eye,eyePosition,1)))vrObserveTower(bones,inverse,reticle,eyePosition);
        }
        float transformed[16];
        if(registers.combined>=0) {
            if(vrCamera(saved,transformed))changed=SUCCEEDED(original<Constants>(d,94)(d,reg,transformed,4));
        } else {
            projectionReg=static_cast<UINT>(registers.projection);
            if(FAILED(d->GetVertexShaderConstantF(projectionReg,savedProjection,4)))return;
            float projection[16];
            if(vrSeparateCamera(saved,savedProjection,transformed,projection)) {
                changed=SUCCEEDED(original<Constants>(d,94)(d,reg,transformed,4));
                if(changed)projectionChanged=SUCCEEDED(original<Constants>(d,94)(d,projectionReg,projection,4));
                if(changed&&!projectionChanged){original<Constants>(d,94)(d,reg,saved,4);changed=false;}
            }
        }
    }
    ~CameraDraw(){if(changed)original<Constants>(d,94)(d,reg,saved,4);if(projectionChanged)original<Constants>(d,94)(d,projectionReg,savedProjection,4);}
};
using Draw=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
HRESULT STDMETHODCALLTYPE draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT start,UINT count){CameraDraw c(d);return original<Draw>(d,81)(d,t,start,count);}
using DrawIndexed=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
HRESULT STDMETHODCALLTYPE drawIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT base,UINT min,UINT vertices,UINT start,UINT count){CameraDraw c(d);return original<DrawIndexed>(d,82)(d,t,base,min,vertices,start,count);}
using DrawUP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
HRESULT STDMETHODCALLTYPE drawUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT count,const void* data,UINT stride){CameraDraw c(d);return original<DrawUP>(d,83)(d,t,count,data,stride);}
using DrawIndexedUP=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
HRESULT STDMETHODCALLTYPE drawIndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT min,UINT vertices,UINT count,const void* indices,D3DFORMAT fmt,const void* data,UINT stride){CameraDraw c(d);return original<DrawIndexedUP>(d,84)(d,t,min,vertices,count,indices,fmt,data,stride);}
using Reset=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
HRESULT STDMETHODCALLTYPE reset(IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* p){vrReset();return original<Reset>(d,16)(d,p);}
using CreateDevice=HRESULT(STDMETHODCALLTYPE*)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
HRESULT STDMETHODCALLTYPE createDevice(IDirect3D9* api,UINT adapter,D3DDEVTYPE type,HWND wnd,DWORD flags,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out) {
    HRESULT hr=original<CreateDevice>(api,16)(api,adapter,type,wnd,flags,pp,out);
    log("CreateDevice hr=%08lx flags=%lx",hr,flags);
    if(SUCCEEDED(hr)&&out&&*out) {
        installKeyboardHooks();
        installVisibilityHook();
        log("Device %p size=%ux%u windowed=%d",*out,pp->BackBufferWidth,pp->BackBufferHeight,pp->Windowed);
        log("Hooks present=%d shader=%d constants=%d",hook(*out,17,(void*)present),hook(*out,91,(void*)createShader),hook(*out,94,(void*)constants));
        log("Stereo draw hooks=%d,%d,%d,%d reset=%d",hook(*out,81,(void*)draw),hook(*out,82,(void*)drawIndexed),hook(*out,83,(void*)drawUP),hook(*out,84,(void*)drawIndexedUP),hook(*out,16,(void*)reset));
    }
    return hr;
}
}
extern "C" IDirect3D9* WINAPI proxyCreate9(UINT sdk) {
    auto fn=proc<IDirect3D9*(WINAPI*)(UINT)>("Direct3DCreate9");
    auto api=fn?fn(sdk):nullptr;
    if(api) log("Direct3DCreate9 sdk=%u hook=%d",sdk,hook(api,16,(void*)createDevice));
    return api;
}
extern "C" HRESULT WINAPI proxyCreate9Ex(UINT sdk,IDirect3D9Ex** out) {
    auto fn=proc<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>("Direct3DCreate9Ex");
    log("Direct3DCreate9Ex: diagnostic interception not enabled for Ex devices");
    return fn?fn(sdk,out):D3DERR_NOTAVAILABLE;
}
extern "C" int WINAPI proxyBegin(D3DCOLOR c,LPCWSTR s) { auto f=proc<int(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_BeginEvent");return f?f(c,s):0; }
extern "C" int WINAPI proxyEnd() { auto f=proc<int(WINAPI*)()>("D3DPERF_EndEvent");return f?f():0; }
extern "C" DWORD WINAPI proxyStatus() { auto f=proc<DWORD(WINAPI*)()>("D3DPERF_GetStatus");return f?f():0; }
extern "C" BOOL WINAPI proxyRepeat() { auto f=proc<BOOL(WINAPI*)()>("D3DPERF_QueryRepeatFrame");return f?f():FALSE; }
extern "C" void WINAPI proxyMarker(D3DCOLOR c,LPCWSTR s) { auto f=proc<void(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_SetMarker");if(f)f(c,s); }
extern "C" void WINAPI proxyRegion(D3DCOLOR c,LPCWSTR s) { auto f=proc<void(WINAPI*)(D3DCOLOR,LPCWSTR)>("D3DPERF_SetRegion");if(f)f(c,s); }
extern "C" void WINAPI proxyOptions(DWORD o) { auto f=proc<void(WINAPI*)(DWORD)>("D3DPERF_SetOptions");if(f)f(o); }
