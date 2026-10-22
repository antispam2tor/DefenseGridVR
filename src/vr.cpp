#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <atomic>
#include "../vendor/openvr/openvr.h"
#include "matrix.h"
#include "vr.h"
#include "tower_view.h"

namespace {
vr::IVRSystem* systemVR=nullptr;
ID3D11Device* gpu=nullptr;
ID3D11DeviceContext* context=nullptr;
ID3D11Texture2D* eyes[2]{};
IDirect3DSurface9* readback=nullptr;
IDirect3DSurface9* resolved=nullptr;
UINT width=0,height=0;
D3DFORMAT format=D3DFMT_UNKNOWN;
bool active=false,valid=false,centered=false;
bool f6=false,f7=false;
bool f10=false,visibilityBypass=true;
std::atomic<bool> visibilityActive{false};
bool f9=false,towerMode=false,pendingTowerToggle=false,hoveredTower=false,gameViewValid=false;
camera::Matrix towerView=camera::identity(),gameView=camera::identity();
camera::TowerAnchor towerAnchor{};
camera::TowerAnchor hoveredAnchor{},pendingAnchor{};
camera::TowerAnchor currentAnchor{};
camera::TowerList observedTowers;
camera::TowerFacingMemory towerFacing;
bool bracketLeft=false,bracketRight=false;
int pendingCycle=0;
float hoverDistanceSquared=INFINITY;
camera::TowerViewSettings towerSettings;
float towerUnitsPerMeter=20.f;
float towerYaw=0,pendingYaw=0,turnSpeed=60.f;
std::atomic<bool> towerControls{false};
std::atomic<bool> birdControls{false};
float birdZoom=0,pendingZoom=0;
bool pendingRecenter=false;
ULONGLONG previousInputTick=0;
unsigned eye=0,pairs=0,changes=0;
camera::Matrix center=camera::identity(),pose=camera::identity();
float unitsPerMeter=600;
void loadTowerProfile() {
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    std::wstring ini=executable;ini.resize(ini.find_last_of(L"\\/")+1);ini+=L"dg1-tower.ini";
    auto number=[&](const wchar_t* key,float fallback) {
        wchar_t text[64]{};GetPrivateProfileStringW(L"Tower",key,L"",text,64,ini.c_str());
        if(!*text)return fallback;
        wchar_t* end=nullptr;float value=wcstof(text,&end);
        return end!=text&&!*end&&std::isfinite(value)?value:fallback;
    };
    towerAnchor.verified=false;
    if(GetPrivateProfileIntW(L"Tower",L"Enabled",0,ini.c_str())!=1)return;
    towerAnchor.base[0]=number(L"X",NAN);towerAnchor.base[1]=number(L"Y",NAN);towerAnchor.base[2]=number(L"Z",NAN);
    towerAnchor.path[0]=number(L"PathX",NAN);towerAnchor.path[1]=number(L"PathY",NAN);towerAnchor.path[2]=number(L"PathZ",NAN);
    towerSettings.sideOffset=number(L"SideOffset",35);towerSettings.backOffset=number(L"BackOffset",10);towerSettings.eyeHeight=number(L"EyeHeight",35);
    towerUnitsPerMeter=number(L"UnitsPerMeter",20);
    turnSpeed=number(L"TurnSpeed",60);
    if(turnSpeed<1||turnSpeed>180)turnSpeed=60;
    towerAnchor.verified=true;
    if(towerUnitsPerMeter<1||towerUnitsPerMeter>600)towerUnitsPerMeter=20;
    if(!camera::makeTowerView(towerAnchor,towerSettings,towerView)){towerAnchor.verified=false;towerSettings={};}
    vrLog("Tower profile valid=%d base=%.2f,%.2f,%.2f path=%.2f,%.2f; F9 toggle",towerAnchor.verified,towerAnchor.base[0],towerAnchor.base[1],towerAnchor.base[2],towerAnchor.path[0],towerAnchor.path[1]);
}
template<class T>void release(T*& p){if(p)p->Release();p=nullptr;}
void resources() {
    release(readback);release(resolved);release(eyes[0]);release(eyes[1]);width=height=0;format=D3DFMT_UNKNOWN;
}
camera::Matrix convert(const vr::HmdMatrix34_t& h) {
    auto m=camera::identity();
    for(int i=0;i<3;++i)for(int j=0;j<4;++j)m.m[i][j]=h.m[i][j];
    return m;
}
void tracking() {
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
    auto err=vr::VRCompositor()->WaitGetPoses(poses,vr::k_unMaxTrackedDeviceCount,nullptr,0);
    auto& h=poses[vr::k_unTrackedDeviceIndex_Hmd];
    valid=err==vr::VRCompositorError_None&&h.bPoseIsValid&&h.bDeviceIsConnected;
    if(!valid)return;
    pose=convert(h.mDeviceToAbsoluteTracking);
    if(!centered){center=pose;centered=true;vrLog("VR tracking recentered");}
}
bool start() {
    vr::EVRInitError error=vr::VRInitError_None;
    systemVR=vr::VR_Init(&error,vr::VRApplication_Scene);
    if(error!=vr::VRInitError_None||!systemVR){vrLog("VR init failed: %s",vr::VR_GetVRInitErrorAsEnglishDescription(error));systemVR=nullptr;return false;}
    if(!vr::VRCompositor()){vrLog("VR compositor unavailable");vr::VR_Shutdown();systemVR=nullptr;return false;}
    IDXGIFactory1* factory=nullptr;IDXGIAdapter* adapter=nullptr;
    int index=0;systemVR->GetDXGIOutputInfo(&index);
    HRESULT hr=CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory));
    if(SUCCEEDED(hr)&&index>=0)hr=factory->EnumAdapters(static_cast<UINT>(index),&adapter);
    if(SUCCEEDED(hr)&&adapter)hr=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&gpu,nullptr,&context);
    else hr=E_FAIL;
    release(adapter);release(factory);
    if(FAILED(hr)){vrLog("VR D3D11 adapter creation failed %08lx",hr);vr::VR_Shutdown();systemVR=nullptr;return false;}
    vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);
    eye=0;pairs=0;centered=false;towerMode=false;pendingTowerToggle=false;hoveredTower=false;towerYaw=pendingYaw=0;
    birdZoom=pendingZoom=0;pendingRecenter=false;
    observedTowers.towers.clear();pendingCycle=0;
    towerFacing.entries.clear();
    loadTowerProfile();
    tracking();
    vrLog("VR enabled; adapter=%d tracking=%d; alternating-frame stereo prototype",index,valid);
    return true;
}
void stop() {
    towerFacing.entries.clear();
    observedTowers.towers.clear();pendingCycle=0;
    birdControls=false;birdZoom=pendingZoom=0;pendingRecenter=false;
    visibilityActive=false;
    towerControls=false;
    active=false;valid=false;towerMode=false;pendingTowerToggle=false;hoveredTower=false;gameViewValid=false;resources();release(context);release(gpu);
    if(systemVR)vr::VR_Shutdown();systemVR=nullptr;
    vrLog("VR disabled");
}
bool capture(IDirect3DDevice9* d) {
    IDirect3DSurface9* back=nullptr;
    if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)))return false;
    D3DSURFACE_DESC desc{};back->GetDesc(&desc);
    if(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8){release(back);return false;}
    if(width!=desc.Width||height!=desc.Height||format!=desc.Format||!readback||!eyes[0]||!eyes[1]) {
        resources();width=desc.Width;height=desc.Height;format=desc.Format;
        HRESULT hr=d->CreateOffscreenPlainSurface(width,height,format,D3DPOOL_SYSTEMMEM,&readback,nullptr);
        if(SUCCEEDED(hr))hr=d->CreateRenderTarget(width,height,format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);
        D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        for(int i=0;i<2&&SUCCEEDED(hr);++i)hr=gpu->CreateTexture2D(&td,nullptr,&eyes[i]);
        vrLog("VR capture textures %ux%u hr=%08lx",width,height,hr);
        if(FAILED(hr)){release(back);resources();return false;}
    }
    HRESULT hr=d->StretchRect(back,nullptr,resolved,nullptr,D3DTEXF_NONE);release(back);
    if(SUCCEEDED(hr))hr=d->GetRenderTargetData(resolved,readback);
    D3DLOCKED_RECT lock{};
    if(SUCCEEDED(hr))hr=readback->LockRect(&lock,nullptr,D3DLOCK_READONLY);
    if(FAILED(hr))return false;
    context->UpdateSubresource(eyes[eye],0,nullptr,lock.pBits,static_cast<UINT>(lock.Pitch),0);
    readback->UnlockRect();context->Flush();return true;
}
}
void vrLog(const char* fmt,...) {
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
    std::wstring file=path;file.resize(file.find_last_of(L"\\/")+1);file+=L"dg1-steamvr.log";
    FILE* f=nullptr;_wfopen_s(&f,file.c_str(),L"a");if(!f)return;
    va_list args;va_start(args,fmt);vfprintf(f,fmt,args);va_end(args);fputc('\n',f);fclose(f);
}
void vrReset(){towerFacing.entries.clear();observedTowers.towers.clear();pendingCycle=0;birdControls=false;birdZoom=pendingZoom=0;pendingRecenter=false;visibilityActive=false;towerControls=false;resources();eye=0;towerMode=false;pendingTowerToggle=false;hoveredTower=false;gameViewValid=false;}
bool vrTowerControlsActive(){return towerControls.load();}
bool vrBirdControlsActive(){return birdControls.load();}
bool vrVisibilityActive(){return visibilityActive.load();}
void vrPresent(IDirect3DDevice9* d) {
    ULONGLONG now=GetTickCount64();
    float seconds=previousInputTick?static_cast<float>(now-previousInputTick)/1000.f:0.f;
    previousInputTick=now;
    // Ignore long stalls and input directed to other apps.
    if(seconds>.1f)seconds=.1f;
    DWORD foregroundProcess=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
    bool key10=(GetAsyncKeyState(VK_F10)&0x8000)!=0;
    if(key10&&!f10&&active&&foregroundProcess==GetCurrentProcessId()) {
        visibilityBypass=!visibilityBypass;
        vrLog("VR visibility expansion %s (F10)",visibilityBypass?"enabled":"disabled");
    }
    f10=key10;
    bool leftBracket=(GetAsyncKeyState(VK_OEM_4)&0x8000)!=0;
    bool rightBracket=(GetAsyncKeyState(VK_OEM_6)&0x8000)!=0;
    if(active&&valid&&towerMode&&foregroundProcess==GetCurrentProcessId()) {
        int step=(rightBracket&&!bracketRight?1:0)-(leftBracket&&!bracketLeft?1:0);
        if(step)pendingCycle=step;
    }
    bracketLeft=leftBracket;bracketRight=rightBracket;
    if(towerMode&&active&&valid&&foregroundProcess==GetCurrentProcessId()) {
        int direction=((GetAsyncKeyState(VK_LEFT)&0x8000)?1:0)-((GetAsyncKeyState(VK_RIGHT)&0x8000)?1:0);
        pendingYaw+=static_cast<float>(direction)*turnSpeed*seconds*0.0174532925199433f;
    }
    if(!towerMode&&active&&valid&&foregroundProcess==GetCurrentProcessId()) {
        int direction=((GetAsyncKeyState(VK_UP)&0x8000)?1:0)-((GetAsyncKeyState(VK_DOWN)&0x8000)?1:0);
        pendingZoom+=static_cast<float>(direction)*400.f*seconds;
    }
    bool key9=(GetAsyncKeyState(VK_F9)&0x8000)!=0;
    if(key9&&!f9&&active) {
        if(towerMode)pendingTowerToggle=true;
        else if(!pendingTowerToggle&&hoveredTower&&gameViewValid) {
            // Freeze this frame's pick until the stereo pair has finished.
            pendingAnchor=hoveredAnchor;pendingTowerToggle=true;
        }
        else vrLog("F9 rejected: no supported tower model under the game reticle");
    }
    f9=key9;hoveredTower=false;hoverDistanceSquared=INFINITY;
    bool key6=(GetAsyncKeyState(VK_F6)&0x8000)!=0,key7=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
    bool toggle=key6&&!f6,recenter=key7&&!f7;f6=key6;f7=key7;
    if(toggle){if(active)stop();else active=start();visibilityActive=active&&visibilityBypass;birdControls=active&&!towerMode;return;}
    visibilityActive=active&&visibilityBypass;
    birdControls=active&&!towerMode;
    if(!active)return;
    if(recenter)pendingRecenter=true;
    vr::VREvent_t event{};while(systemVR->PollNextEvent(&event,sizeof(event)))if(event.eventType==vr::VREvent_Quit){systemVR->AcknowledgeQuit_Exiting();stop();return;}
    if(!valid){observedTowers.towers.clear();pendingCycle=0;eye=0;tracking();return;}
    if(!capture(d)){vrLog("VR readback failed; disabling prototype");stop();return;}
    if(eye==0){eye=1;return;}
    vr::Texture_t left{eyes[0],vr::TextureType_DirectX,vr::ColorSpace_Gamma};
    vr::Texture_t right{eyes[1],vr::TextureType_DirectX,vr::ColorSpace_Gamma};
    auto a=vr::VRCompositor()->Submit(vr::Eye_Left,&left);
    auto b=vr::VRCompositor()->Submit(vr::Eye_Right,&right);
    vr::VRCompositor()->PostPresentHandoff();
    if(++pairs<=3||a!=vr::VRCompositorError_None||b!=vr::VRCompositorError_None)vrLog("VR pair=%u submit=%d,%d cameraDraws=%u",pairs,a,b,changes);
    if(a!=vr::VRCompositorError_None||b!=vr::VRCompositorError_None){stop();return;}
    changes=0;eye=0;
    // Include the last turn input before saving the tower being left.
    if(towerMode)towerYaw=std::remainder(towerYaw+pendingYaw,6.283185307179586f);
    pendingYaw=0;
    if(pendingTowerToggle){
        if(towerMode){towerFacing.save(currentAnchor,towerView,towerYaw);towerMode=false;}
        else {towerMode=camera::makeTowerView(pendingAnchor,towerSettings,towerView);if(towerMode){currentAnchor=pendingAnchor;towerYaw=towerFacing.restore(currentAnchor,towerView);}}
        towerControls=towerMode;pendingTowerToggle=false;centered=false;
        vrLog("Tower view %s; selected base=%.2f,%.2f,%.2f",towerMode?"entered":"returned to overhead",pendingAnchor.base[0],pendingAnchor.base[1],pendingAnchor.base[2]);
    }
    if(towerMode&&pendingCycle) {
        camera::TowerAnchor next;
        camera::Matrix nextView;
        if(observedTowers.cycle(currentAnchor,pendingCycle,next)&&camera::makeTowerView(next,towerSettings,nextView)) {
            towerFacing.save(currentAnchor,towerView,towerYaw);
            currentAnchor=next;towerView=nextView;towerYaw=towerFacing.restore(currentAnchor,towerView);centered=false;
            vrLog("Tower cycle base=%.2f,%.2f,%.2f candidates=%u",next.base[0],next.base[1],next.base[2],static_cast<unsigned>(observedTowers.towers.size()));
        }
    }
    pendingCycle=0;observedTowers.towers.clear();
    if(!towerMode)birdZoom=camera::clampZoom(birdZoom+pendingZoom);
    pendingZoom=0;
    if(pendingRecenter){
        centered=false;
        if(!towerMode)birdZoom=0;
        pendingRecenter=false;
        vrLog("F7 recenter; overhead zoom=%.1f",birdZoom);
    }
    birdControls=!towerMode;
    tracking();
}
void vrObserveTower(const float* bones,const float* inverseVP,const float* reticle,const float* eyePosition) {
    if(!active||!gameViewValid)return;
    camera::TowerAnchor candidate;
    if(!camera::towerFromModel(bones,gameView,candidate))return;
    // Retain the user-confirmed facing at the original calibration point.
    bool calibrated=towerAnchor.verified;
    for(int i=0;i<3;++i)if(std::abs(candidate.base[i]-towerAnchor.base[i])>3.f)calibrated=false;
    if(calibrated)for(int i=0;i<3;++i)candidate.path[i]=towerAnchor.path[i];
    camera::Matrix check;
    if(!camera::makeTowerView(candidate,towerSettings,check))return;
    observedTowers.observe(candidate);
    if(towerMode)return;
    float distance=camera::towerHoverDistanceSquared(candidate,inverseVP,reticle,eyePosition);
    if(distance>45.f*45.f||distance>=hoverDistanceSquared)return;
    hoveredAnchor=candidate;hoverDistanceSquared=distance;hoveredTower=true;
}
bool vrSeparateCamera(const float* originalView,const float* originalProjection,float* outputView,float* outputProjection) {
    if(!active||!valid||!centered)return false;
    camera::Matrix check,view;
    if(!camera::extractView(originalProjection,check))return false;
    memcpy(view.m,originalView,sizeof(view.m));
    if(towerMode&&gameViewValid) {
        // Preserve any object transform embedded in split view/model-view matrices.
        auto turnedView=camera::turnInPlace(towerView,towerYaw);
        view=camera::multiply(camera::multiply(turnedView,camera::rigidInverse(gameView)),view);
    } else if(!towerMode)view=camera::dollyView(view,birdZoom);
    auto relative=camera::multiply(camera::rigidInverse(center),pose);
    auto eyeTransform=convert(systemVR->GetEyeToHeadTransform(eye?vr::Eye_Right:vr::Eye_Left));
    auto eyeView=camera::trackedView(view,relative,eyeTransform,towerMode?towerUnitsPerMeter:unitsPerMeter);
    auto p=systemVR->GetProjectionMatrix(eye?vr::Eye_Right:vr::Eye_Left,towerMode?1.f:5.f,20000.f);
    camera::Matrix projection;memcpy(projection.m,p.m,sizeof(projection.m));
    memcpy(outputView,eyeView.m,sizeof(eyeView.m));
    memcpy(outputProjection,projection.m,sizeof(projection.m));++changes;return true;
}
bool vrCamera(const float* original,float* output) {
    camera::Matrix view,eyeView,projection;
    if(!camera::extractView(original,view))return false;
    gameView=view;gameViewValid=true;
    if(!vrSeparateCamera(&view.m[0][0],original,&eyeView.m[0][0],&projection.m[0][0]))return false;
    auto result=camera::multiply(projection,eyeView);
    memcpy(output,result.m,sizeof(result.m));return true;
}
