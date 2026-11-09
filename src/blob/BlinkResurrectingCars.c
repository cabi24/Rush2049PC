/* BlinkResurrectingCars.c: make respawned cars blink until they are solid again (MAT3 resurrect.c,
 * BlinkResurrectingCars()).
 *
 * After a respawn a car is not solid for a while (it can't hit or be hit); meanwhile it blinks, hidden for a few
 * frames and shown for one, the hidden phase getting shorter as the time since the respawn grows. */
#include "game/game.h"

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields BlinkResurrectingCars uses. */
typedef struct tCarData
{
  u8 pad000[0xEC];
  s8 data_valid;                                /* +0x0EC the record holds a car this race */
  u8 pad0ED[0x110 - 0xED]; struct pc_visual visuals[21];
  s8 collidable;                                /* +0x308 */
  s8 solid;                                     /* +0x309 solid again since the last respawn */
  u8 pad30A[0x30C - 0x30A];
  f32 collide_time;                             /* +0x30C the model time it stopped being solid (the respawn) */
  s8 collide_count;                             /* +0x310 frames left in this blink phase */
  s8 collide_state;                             /* +0x311 the blink phase: odd shown, even hidden */
  u8 pad312[0x359 - 0x312];
  s8 dead;                                      /* +0x359 CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
} tCarData;

/* The model record (tModelData), the fields BlinkResurrectingCars uses. */
typedef struct tModelData
{
  _tCar *car; Tune44 *tune;                     /* +0x000 the physics descriptor and its tune */
  u8 pad008[0x3A0 - 0x008]; streetyp *lasttp[4]; /* +0x3A0 each tire's last road node */
  u8 pad3B0[0x40C - 0x3B0]; const s16 *torquecurve;   /* +0x40C the torque curve */
  u8 pad410[0x648 - 0x410]; surftyp *movingSurface;   /* +0x648 the moving surface under the car */
  u8 pad64C[0x714 - 0x64C]; f32 modelTime;      /* +0x714 fastin.modelTime: the model thread's race clock, seconds */
  u8 pad718[0x7C8 - 0x718]; s16 in_game;        /* +0x7C8 in the race */
  u8 pad7CA[0x7D4 - 0x7CA]; s32 appearance;     /* +0x7D4 APPEAR_* bits */
} tModelData;

/* Once a frame, for each car in the race: while it is not solid, count its blink phase down (a frame of the game's
 * 30 a second: at 60 frames a second only every other one, PC_EACH30); when the count runs out the phase flips. A
 * shown phase lasts one frame and clears APPEAR_INVISIBLE; a hidden one lasts 2 frames in the first two seconds
 * after the respawn, 1 frame up to four seconds, then none, and sets APPEAR_INVISIBLE unless the car is out of the
 * race (dead CAR_DYING or CAR_DEAD). A solid car is always shown. */
void BlinkResurrectingCars(void)
{
  tCarData *car;
  s16 i;
  float delta_time;
  unsigned int whole_seconds;
  tModelData *m;
  for (i = 0; i < PC_RACE_CARS; i++)
  {
    m = PC_MODEL(i);
    if (m->in_game != 0)
    {
      car = PC_CAR(i);
      if (car->data_valid != 0)
      {
        if (car->solid == 0)
        {
          PC_EACH30(car->collide_count = car->collide_count - 1;)
          delta_time = m->modelTime - car->collide_time;
          if (car->collide_count < 0)
          {
            car->collide_state = car->collide_state + 1;
            if (car->collide_state & 1)
            {
              car->collide_count = 0;
              m->appearance = (s32) (m->appearance & (~APPEAR_INVISIBLE));
            }
            else
            {
              whole_seconds = (s32) delta_time;
              switch (whole_seconds)
              {
                default:
                  car->collide_count = 0;
                  break;
                case 0:
                case 1:
                  car->collide_count = 2;
                  break;
                case 2:
                case 3:
                  car->collide_count = 1;
                  break;
              }

              if (car->dead <= CAR_ALIVE)
              {
                m->appearance = (s32) (m->appearance | APPEAR_INVISIBLE);
              }
            }
          }
        }
        else
        {
          m->appearance = (s32) (m->appearance & (~APPEAR_INVISIBLE));
        }
      }
    }
  }
}
