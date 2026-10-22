#include "matrix.h"
#include "tower_view.h"
#include <cassert>
#include <cstdio>
int main() {
    camera::TowerList list;
    camera::TowerAnchor a{{0,0,90},{0,200,90},true},b{{60,0,180},{60,200,180},true},c{{120,0,90},{120,200,90},true},selected;
    assert(!list.cycle(a,1,selected));
    list.observe(a);assert(!list.cycle(a,1,selected));
    list.observe(c);list.observe(b);list.observe(a);
    auto duplicate=b;duplicate.base[2]=200;list.observe(duplicate);
    assert(list.towers.size()==3);
    assert(list.cycle(a,1,selected)&&camera::sameTowerPosition(selected,b)&&selected.base[2]==180);
    assert(list.cycle(a,-1,selected)&&camera::sameTowerPosition(selected,c));
    assert(list.cycle(c,1,selected)&&camera::sameTowerPosition(selected,a));
    assert(list.cycle(b,-1,selected)&&camera::sameTowerPosition(selected,a));
    list.towers.clear();list.observe(c);list.observe(a); // Sold current tower.
    assert(list.cycle(b,1,selected)&&camera::sameTowerPosition(selected,c));
    assert(list.cycle(b,-1,selected)&&camera::sameTowerPosition(selected,a));
    list.towers.clear();assert(!list.cycle(b,1,selected));
    puts("PASS: tower cycling order, both wrap directions, duplicate model parts, empty/single list and sold current tower");
    camera::TowerFacingMemory facing;
    camera::Matrix viewA,viewB;
    assert(camera::makeTowerView(a,{},viewA)&&camera::makeTowerView(b,{},viewB));
    assert(facing.restore(a,viewA)==0);
    facing.save(a,viewA,1.2f);facing.save(b,viewB,-.7f);
    assert(std::abs(facing.restore(a,viewA)-1.2f)<.0001f);
    assert(std::abs(facing.restore(b,viewB)+.7f)<.0001f);
    facing.save(a,viewA,2.f);assert(facing.entries.size()==2);
    auto changedBase=camera::turnInPlace(viewA,.4f);
    auto restoredFacing=camera::turnInPlace(changedBase,facing.restore(a,changedBase));
    auto savedFacing=camera::turnInPlace(viewA,2.f);
    for(int i=0;i<3;++i)assert(std::abs(restoredFacing.m[2][i]-savedFacing.m[2][i])<.0001f);
    assert(std::abs(facing.restore(b,viewB)+.7f)<.0001f);
    facing.entries.clear();assert(facing.restore(a,viewA)==0);
    puts("PASS: independent tower facing, overwrite, unchanged world heading after base-camera rotation, session reset");
    auto transform=camera::identity();transform.m[0][0]=0;transform.m[0][1]=-1;transform.m[1][0]=1;transform.m[1][1]=0;
    transform.m[0][3]=2;transform.m[1][3]=3;transform.m[2][3]=4;
    auto result=camera::multiply(transform,camera::rigidInverse(transform));
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)assert(std::abs(result.m[i][j]-(i==j?1.f:0.f))<1e-5f);
    // Actual battlefield capture: recovered view rows must form an orthonormal basis.
    float vp[16]={1.55458796f,-.971617341f,0,-1181.56738f,1.09663081f,1.75460958f,1.30133557f,-16.9933891f,.290465295f,.464744508f,-.871395826f,944.550293f,.282166302f,.451466143f,-.846498907f,1017.56317f};
    camera::Matrix v;assert(camera::extractView(vp,v));
    auto unit=camera::multiply(v,camera::rigidInverse(v));
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)assert(std::abs(unit.m[i][j]-(i==j?1.f:0.f))<0.001f);
    auto shadow=camera::identity();assert(!camera::extractView(&shadow.m[0][0],v));
    auto left=camera::identity(),right=camera::identity();left.m[0][3]=-.032f*600;right.m[0][3]=.032f*600;
    assert(camera::rigidInverse(left).m[0][3]>0&&camera::rigidInverse(right).m[0][3]<0);
    // Split-matrix effects and combined scene shaders must project identical points.
    assert(camera::extractView(vp,v));
    auto zoomed=camera::dollyView(v,400.f);
    auto zoomPose=camera::rigidInverse(zoomed),normalPose=camera::rigidInverse(v);
    for(int i=0;i<3;++i)assert(std::abs(zoomPose.m[i][3]-(normalPose.m[i][3]-v.m[2][i]*400.f))<.001f);
    auto resetZoom=camera::dollyView(v,0);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(resetZoom.m[i][j]==v.m[i][j]);
    auto local=camera::identity();local.m[0][3]=120;local.m[2][3]=50;
    auto splitZoom=camera::dollyView(camera::multiply(v,local),400);
    auto expectedZoom=camera::multiply(zoomed,local);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)assert(std::abs(splitZoom.m[i][j]-expectedZoom.m[i][j])<.001f);
    assert(camera::clampZoom(10000)==600&&camera::clampZoom(-10000)==-3000&&camera::clampZoom(0)==0);
    puts("PASS: overhead zoom direction, default reset, split effects agreement and zoom limits");
    auto head=camera::identity();head.m[0][0]=0;head.m[0][2]=1;head.m[2][0]=-1;head.m[2][2]=0;head.m[0][3]=.1f;
    auto p=camera::identity();p.m[0][0]=1.8f;p.m[1][1]=2.4f;p.m[2][2]=-1.01f;p.m[2][3]=-5;p.m[3][2]=-1;p.m[3][3]=0;
    auto tracked=camera::trackedView(v,head,left,600);
    auto combined=camera::multiply(p,tracked);
    float point[4]={300,-700,50,1},inView[4]{},splitClip[4]{},combinedClip[4]{};
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)inView[i]+=tracked.m[i][j]*point[j];
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){splitClip[i]+=p.m[i][j]*inView[j];combinedClip[i]+=combined.m[i][j]*point[j];}
    for(int i=0;i<4;i++)assert(std::abs(splitClip[i]-combinedClip[i])<.001f);
    // Head motion must change the effect's view matrix, not leave it headset-locked.
    auto neutral=camera::trackedView(v,camera::identity(),left,600);
    assert(std::abs(tracked.m[0][0]-neutral.m[0][0])>.1f);
    camera::TowerAnchor anchor{{300,-700,150},{300,-500,150},true};
    camera::Matrix tower;
    assert(camera::makeTowerView(anchor,{},tower));
    auto standing=camera::rigidInverse(tower);
    assert(std::abs(standing.m[2][3]-185.f)<.001f); // Raised platform, not world zero.
    float targetX=tower.m[0][0]*anchor.path[0]+tower.m[0][1]*anchor.path[1]+tower.m[0][3];
    float targetZ=tower.m[2][0]*anchor.path[0]+tower.m[2][1]*anchor.path[1]+tower.m[2][3];
    assert(std::abs(targetX)<.001f&&targetZ<0); // Path directly ahead in RH view space.
    anchor.verified=false;assert(!camera::makeTowerView(anchor,{},tower));
    anchor.verified=true;anchor.path[0]=anchor.base[0];anchor.path[1]=anchor.base[1];
    assert(!camera::makeTowerView(anchor,{},tower));
    puts("PASS: tower-side elevation, path-facing orientation, rejection of unknown/degenerate anchors");
    camera::TowerAnchor capturedTower{{990,-450,180},{1138.8f,-544.5f,180},true};
    float invTower[16]={.462567061f,.183544889f,-7.35783195f,7.85640383f,-.289104223f,.293671489f,7.9446187f,-7.72681713f,6.61578454e-8f,.217806384f,-8.40351391f,7.80417538f,6.46072709e-11f,0,-.00971428677f,.00999999978f};
    float reticle[4]={0,0,0,1},cameraEye[4]={757.423828f,-817.828369f,865.067505f,1};
    assert(camera::hoverMatchesTower(capturedTower,invTower,reticle,cameraEye));
    float away[4]={.8f,.8f,0,1};assert(!camera::hoverMatchesTower(capturedTower,invTower,away,cameraEye));
    // Moving tower, original camera and cursor ray together must select the new
    // location, including a different platform elevation, with no profile match.
    float bones[12]={.63f,0,0,990,0,.63f,0,-450,0,0,.63f,180};
    camera::TowerAnchor dynamicTower;
    assert(camera::towerFromModel(bones,v,dynamicTower));
    float originalScore=camera::towerHoverDistanceSquared(dynamicTower,invTower,reticle,cameraEye);
    auto shiftedInverse=camera::identity();memcpy(shiftedInverse.m,invTower,sizeof(invTower));
    auto translation=camera::identity();float shift[3]={600,-300,90};
    float shiftedEye[4]={cameraEye[0]+shift[0],cameraEye[1]+shift[1],cameraEye[2]+shift[2],1};
    for(int i=0;i<3;++i){bones[i*4+3]+=shift[i];translation.m[i][3]=shift[i];}
    shiftedInverse=camera::multiply(translation,shiftedInverse);
    assert(camera::towerFromModel(bones,v,dynamicTower));
    assert(camera::hoverMatchesTower(dynamicTower,&shiftedInverse.m[0][0],reticle,shiftedEye));
    assert(std::abs(camera::towerHoverDistanceSquared(dynamicTower,&shiftedInverse.m[0][0],reticle,shiftedEye)-originalScore)<1.f);
    assert(camera::makeTowerView(dynamicTower,{},tower));
    assert(std::abs(camera::rigidInverse(tower).m[2][3]-305.f)<.001f);
    assert((dynamicTower.path[0]-dynamicTower.base[0])*(-v.m[2][0])+(dynamicTower.path[1]-dynamicTower.base[1])*(-v.m[2][1])>0);
    auto distantTower=dynamicTower;distantTower.base[0]+=100;
    assert(camera::towerHoverDistanceSquared(distantTower,&shiftedInverse.m[0][0],reticle,shiftedEye)>originalScore);
    bones[0]=bones[5]=bones[10]=1.f;assert(!camera::towerFromModel(bones,v,dynamicTower));
    bones[0]=NAN;assert(!camera::towerFromModel(bones,v,dynamicTower));
    puts("PASS: dynamic tower selection at translated/elevated locations, cursor ranking, camera facing, rejection of non-tower scale/invalid data");
    camera::Matrix standingView;assert(camera::makeTowerView(capturedTower,{},standingView));
    auto object=camera::identity();object.m[0][3]=120;object.m[1][3]=-50;
    auto originalModelView=camera::multiply(v,object);
    auto delta=camera::multiply(standingView,camera::rigidInverse(v));
    auto changedModelView=camera::multiply(delta,originalModelView);
    auto expectedModelView=camera::multiply(standingView,object);
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)assert(std::abs(changedModelView.m[i][j]-expectedModelView.m[i][j])<.01f);
    puts("PASS: actual Fire Control hover ray accepts tower and rejects distant cursor; tower view preserves effect object transforms");
    auto basePose=camera::rigidInverse(standingView);
    auto leftPose=camera::rigidInverse(camera::turnInPlace(standingView,1.57079632679f));
    auto rightPose=camera::rigidInverse(camera::turnInPlace(standingView,-1.57079632679f));
    for(int i=0;i<3;++i){
        assert(std::abs(basePose.m[i][3]-leftPose.m[i][3])<.001f);
        assert(std::abs(basePose.m[i][3]-rightPose.m[i][3])<.001f);
        assert(std::abs(leftPose.m[i][2]-basePose.m[i][0])<.001f); // Forward now points left.
        assert(std::abs(rightPose.m[i][2]+basePose.m[i][0])<.001f);
    }
    auto fullTurn=camera::turnInPlace(standingView,6.28318530718f);
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)assert(std::abs(fullTurn.m[i][j]-standingView.m[i][j])<.001f);
    puts("PASS: left/right turning signs, fixed standing position and height, full rotation returns to start");
    puts("PASS: captured camera, shadow/UI rejection, inverse, stereo offsets, split/combined projection agreement with tracked pose");
}
