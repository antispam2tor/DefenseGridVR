#pragma once
#include <cmath>
#include <cstring>
namespace camera {
struct Matrix { float m[4][4]{}; };
inline Matrix identity() { Matrix a{}; for(int i=0;i<4;++i)a.m[i][i]=1; return a; }
inline Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix c{};
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)c.m[i][j]+=a.m[i][k]*b.m[k][j];
    return c;
}
inline Matrix rigidInverse(const Matrix& a) {
    auto b=identity();
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)b.m[i][j]=a.m[j][i];
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)b.m[i][3]-=b.m[i][j]*a.m[j][3];
    return b;
}
// Positive distance moves forward along the original camera's viewing direction.
// Pre-multiplication preserves any local model transform in a split model-view.
inline Matrix dollyView(const Matrix& view,float distance) {
    auto moved=view;moved.m[2][3]+=distance;return moved;
}
inline float clampZoom(float distance) {
    return distance < -3000.f ? -3000.f : (distance > 600.f ? 600.f : distance);
}
inline float length3(const float* v) { return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }
inline Matrix trackedView(const Matrix& view,const Matrix& relative,const Matrix& eye,float unitsPerMeter) {
    auto headEye=multiply(relative,eye);
    for(int i=0;i<3;++i)headEye.m[i][3]*=unitsPerMeter;
    return multiply(rigidInverse(headEye),view);
}
// D3D9 shader constants are dot-product rows, as confirmed by shader disassembly.
// Reject orthographic shadow passes rather than transforming them as eye views.
inline bool extractView(const float* vp,Matrix& view) {
    float w=length3(vp+12),x=length3(vp),y=length3(vp+4);
    if(!std::isfinite(w)||std::abs(w-1)>0.02f||x<0.05f||y<0.05f||x>20||y>20)return false;
    view=identity();
    for(int j=0;j<4;++j) { view.m[0][j]=vp[j]/x;view.m[1][j]=vp[4+j]/y;view.m[2][j]=-vp[12+j]; }
    return true;
}
}
