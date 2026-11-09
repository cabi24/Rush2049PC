#include "game/game.h"

typedef struct ModelView {
    u8 other0[40];f32 A[3],AA[3],V[3],W[3];
    u8 other88[676];f32 up;u8 other768[208];f32 brake,throttle;
    u8 other984[24];f32 magvel;u8 other1012[504];f32 contact[4];
    u8 other1532[56];f32 dt;u8 other1592[8];s8 sliding;
    u8 other1601[15];f32 damping;u8 other1620[370];s16 node;
} ModelView;

extern f32 fabsf ( f32 );
#pragma intrinsic (fabsf)
void velocities(ModelView *m)
{
    f32 temp[3],velfact,speed;f32 coef;
    temp[0]=m->A[0]*m->dt;
    temp[1]=m->A[1]*m->dt;
    temp[2]=m->A[2]*m->dt;
    m->V[0]=m->V[0]+temp[0];
    m->V[1]=m->V[1]+temp[1];
    m->V[2]=m->V[2]+temp[2];
    if(m->brake<0.1f) {
        if(fabsf(m->V[0])<0.025f)m->V[0]=0.0f;
        if(fabsf(m->V[2])<0.025f)m->V[2]=0.0f;
    }
    if(PC_CAR_STATE(m->node)!=2)m->magvel=vector_magnitude(m->V);
    if(m->magvel>300.0f) {
        velfact=300.0f/m->magvel;
        m->V[0]*=velfact;
        m->V[1]*=velfact;
        m->V[2]*=velfact;
        return;
    }
    temp[0]=m->AA[0]*m->dt;
    temp[1]=m->AA[1]*m->dt;
    temp[2]=m->AA[2]*m->dt;
    if(m->damping!=0.0f) {
        temp[0]-=m->W[0]*.025f;
        temp[1]-=m->W[1]*.025f;
        temp[2]-=m->W[2]*.025f;
    }
    m->W[0]=m->W[0]+temp[0];
    m->W[1]=m->W[1]+temp[1];
    m->W[2]=m->W[2]+temp[2];
    if((m->contact[0]>0 && m->contact[1]>0 && m->contact[2]>0 && m->contact[3]>0) || m->sliding) {
        speed=vector_magnitude(m->W);
        speed=speed*speed;
        if(m->sliding)coef=gWingAngVDrag_1;else coef=gWingAngVDrag;
        speed=speed*coef;
        m->W[0]-=m->W[0]*speed;
        m->W[1]-=m->W[1]*speed;
        m->W[2]-=m->W[2]*speed;
    }
    if(m->magvel<3.0f && (m->throttle>0.5f || m->up<0.0f)) {
        m->V[0]=0.0f;
        m->V[2]=0.0f;
    }
    if(m->magvel<3.0f && m->up< -0.75f) {
        if(vector_magnitude(m->W)<0.5f) {
            m->W[0]=0.0f;
            m->W[1]=0.0f;
            m->W[2]=0.0f;
        }
    }
}
