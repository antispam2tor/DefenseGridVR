#pragma once
#include <d3d9.h>
void vrPresent(IDirect3DDevice9* device);
void vrReset();
bool vrCamera(const float* originalProjection, float* outputProjection);
bool vrSeparateCamera(const float* view,const float* projection,float* outputView,float* outputProjection);
void vrLog(const char* format,...);
void vrObserveTower(const float* boneRows,const float* inverseVP,const float* reticle,const float* eye);
bool vrTowerControlsActive();
bool vrVisibilityActive();
bool vrBirdControlsActive();
