#pragma once
#include "matrix.h"
#include <vector>
#include <algorithm>

namespace camera {
// Positive yaw turns left around the standing camera's up axis, without orbiting.
inline Matrix turnInPlace(const Matrix& view,float yaw) {
    auto rotation=identity();
    float c=std::cos(yaw),s=std::sin(yaw);
    rotation.m[0][0]=c;rotation.m[0][2]=-s;
    rotation.m[2][0]=s;rotation.m[2][2]=c;
    return multiply(rotation,view);
}
struct TowerAnchor {
    float base[3]{};          // Tower base on its actual platform, world Z is up.
    float path[3]{};          // A verified nearby navigable point, not a screen coordinate.
    bool verified=false;
};
struct TowerViewSettings {
    float sideOffset=35.f;
    float backOffset=10.f;
    float eyeHeight=35.f;
};
inline bool sameTowerPosition(const TowerAnchor& a,const TowerAnchor& b) {
    return std::abs(a.base[0]-b.base[0])<3.f&&std::abs(a.base[1]-b.base[1])<3.f;
}
struct TowerFacingMemory {
    struct Entry { TowerAnchor anchor;float heading; };
    std::vector<Entry> entries;
    void save(const TowerAnchor& anchor,const Matrix& baseView,float yaw) {
        auto turned=turnInPlace(baseView,yaw);
        float heading=std::atan2(-turned.m[2][1],-turned.m[2][0]);
        for(auto& entry:entries)if(sameTowerPosition(entry.anchor,anchor)){entry.heading=heading;return;}
        if(entries.size()<512)entries.push_back({anchor,heading});
    }
    float restore(const TowerAnchor& anchor,const Matrix& baseView) const {
        for(const auto& entry:entries)if(sameTowerPosition(entry.anchor,anchor))
            return std::remainder(entry.heading-std::atan2(-baseView.m[2][1],-baseView.m[2][0]),6.283185307179586f);
        return 0;
    }
};
// Rebuilt from each stereo pair, so sold towers and previous levels do not linger.
struct TowerList {
    std::vector<TowerAnchor> towers;
    void observe(const TowerAnchor& anchor) {
        if(!anchor.verified)return;
        for(float coordinate:anchor.base)if(!std::isfinite(coordinate))return;
        for(auto& entry:towers)if(sameTowerPosition(entry,anchor)) {
            if(anchor.base[2]<entry.base[2])entry=anchor;
            return;
        }
        if(towers.size()<512)towers.push_back(anchor);
    }
    bool cycle(const TowerAnchor& current,int direction,TowerAnchor& output) {
        if(towers.empty()||!direction)return false;
        auto less=[](const TowerAnchor& a,const TowerAnchor& b){
            return a.base[0]!=b.base[0]?a.base[0]<b.base[0]:a.base[1]<b.base[1];
        };
        std::sort(towers.begin(),towers.end(),less);
        int count=static_cast<int>(towers.size());
        for(int i=0;i<count;++i)if(sameTowerPosition(towers[i],current)) {
            if(count==1)return false;
            output=towers[(i+(direction>0?1:count-1))%count];return true;
        }
        int insertion=static_cast<int>(std::lower_bound(towers.begin(),towers.end(),current,less)-towers.begin());
        output=towers[(direction>0?insertion:(insertion+count-1))%count];return true;
    }
};
inline float towerHoverDistanceSquared(const TowerAnchor& anchor,const float* inverseVP,const float* reticle,const float* eye) {
    if(!anchor.verified)return INFINITY;
    float ndc[4]={reticle[0],reticle[1],0,1},point[4]{};
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)point[i]+=inverseVP[i*4+j]*ndc[j];
    if(!std::isfinite(point[3])||std::abs(point[3])<1e-7f)return INFINITY;
    float dir[3];for(int i=0;i<3;++i)dir[i]=point[i]/point[3]-eye[i];
    if(!std::isfinite(dir[2])||std::abs(dir[2])<1e-7f)return INFINITY;
    float t=(anchor.base[2]+25.f-eye[2])/dir[2];
    if(!std::isfinite(t)||t<=0)return INFINITY;
    float x=eye[0]+t*dir[0]-anchor.base[0],y=eye[1]+t*dir[1]-anchor.base[1];
    return std::isfinite(x)&&std::isfinite(y)?x*x+y*y:INFINITY;
}
inline bool hoverMatchesTower(const TowerAnchor& anchor,const float* inverseVP,const float* reticle,const float* eye,float radius=45.f) {
    return towerHoverDistanceSquared(anchor,inverseVP,reticle,eye)<=radius*radius;
}
// Render-model heuristic, not an engine tower-ID lookup. The captured gun root is
// uniformly scaled by .63; observed enemies/platforms use a different scale.
inline bool towerFromModel(const float* bones,const Matrix& gameView,TowerAnchor& output) {
    for(int i=0;i<12;++i)if(!std::isfinite(bones[i]))return false;
    for(int i=0;i<3;++i)if(std::abs(length3(bones+4*i)-.63f)>.03f)return false;
    float dx=-gameView.m[2][0],dy=-gameView.m[2][1];
    float length=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(length)||length<.001f)return false;
    TowerAnchor candidate{};
    for(int i=0;i<3;++i)candidate.base[i]=candidate.path[i]=bones[4*i+3];
    candidate.path[0]+=200.f*dx/length;candidate.path[1]+=200.f*dy/length;
    candidate.verified=true;output=candidate;return true;
}
// Path may be a calibrated route point or a synthetic horizontal facing target.
inline bool makeTowerView(const TowerAnchor& anchor,const TowerViewSettings& settings,Matrix& output) {
    if(!anchor.verified)return false;
    for(int i=0;i<3;++i)if(!std::isfinite(anchor.base[i])||!std::isfinite(anchor.path[i]))return false;
    if(!std::isfinite(settings.sideOffset)||!std::isfinite(settings.backOffset)||!std::isfinite(settings.eyeHeight)||settings.sideOffset<0||settings.backOffset<0||settings.eyeHeight<=0)return false;
    float dx=anchor.path[0]-anchor.base[0],dy=anchor.path[1]-anchor.base[1];
    float distance=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(distance)||distance<1.f)return false;
    dx/=distance;dy/=distance;
    // Right = forward cross world-up. Place beside, not inside, the tower.
    float x=anchor.base[0]+dy*settings.sideOffset-dx*settings.backOffset;
    float y=anchor.base[1]-dx*settings.sideOffset-dy*settings.backOffset;
    float z=anchor.base[2]+settings.eyeHeight;
    // Aim horizontally toward the path from the actual standing position.
    dx=anchor.path[0]-x;dy=anchor.path[1]-y;distance=std::sqrt(dx*dx+dy*dy);
    if(distance<1.f)return false;
    dx/=distance;dy/=distance;
    auto view=identity();
    view.m[0][0]=dy;view.m[0][1]=-dx;view.m[0][2]=0;
    view.m[1][0]=0;view.m[1][1]=0;view.m[1][2]=1;
    view.m[2][0]=-dx;view.m[2][1]=-dy;view.m[2][2]=0;
    for(int i=0;i<3;++i)view.m[i][3]=-(view.m[i][0]*x+view.m[i][1]*y+view.m[i][2]*z);
    output=view;return true;
}
}
