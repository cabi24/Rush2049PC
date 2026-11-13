/* include/game/cars.h: the cars, their visuals, skids and smoke */
#ifndef GAME_CARS_H
#define GAME_CARS_H

#include "game/types.h"

/* Constants: the names of MAT3's rush_flags.h, the same values. */

/* tModelData.appearance, copied each frame to tCarData.appearance: the effects the car shows. The physics side
 * (the model step, respawning, the road collision) sets them, update_game_data copies them, the visuals, smoke and
 * skids read them. Tires and corners: 0 and 1 the front pair, 2 and 3 the rear; 0 and 2 on the +x side. */
#define APPEAR_SKID_T1       0x2        /* a skid mark under tire 1 */
#define APPEAR_SKID_T0       0x4
#define APPEAR_INVISIBLE     0x8        /* not drawn this frame: the blink while not solid after a respawn */
#define APPEAR_HULK          0x10       /* draw the burnt-out wreck (body 13); also the "drive the hulk" cheat */
#define APPEAR_WRECK_SMOKE   0x60       /* the wreck's smoke (StartSmoke type 4); 0x20 and 0x40 go together */
#define APPEAR_SPARK_C0      0x80       /* the body scraping at corner 0: sparks (above 20 ft/s) */
#define APPEAR_SPARK_C1      0x100
#define APPEAR_SKID_T3       0x200
#define APPEAR_SKID_T2       0x400
#define APPEAR_THUMP         0x800      /* an underbody hit: sparks under the car */
#define APPEAR_BRAKE         0x1000     /* the brake lights on */
#define APPEAR_SPARK_C3      0x2000
#define APPEAR_SPARK_C2      0x4000
#define APPEAR_SMOKE_T2      0x10000    /* tire smoke (StartSmoke type: the tire's number) */
#define APPEAR_SMOKE_T3      0x20000
#define APPEAR_SMOKE_T0      0x40000
#define APPEAR_SMOKE_T1      0x80000
#define APPEAR_BODY_HIT      0x100000   /* one frame: the body hit the world (the stunt code counts it) */
#define APPEAR_CRASHED       (APPEAR_HULK | APPEAR_WRECK_SMOKE)   /* a crash makes the car a smoking hulk */

/* tCarData.visuals[21]: which visual animates what (StartCar, StartEnvironment). MAT3's car has a headlight visual
 * at 1 and 22 visuals, so its indices from there on are one more. 15 is left unused. */
#define VIS_BRAKE_LIGHTS     0          /* AnimateBrakeLights */
#define VIS_SPARKS           1          /* 1-5: the sparks at the front left, rear left, front right, rear right
                                         * corners and under the car (AnimateLFSpark ... AnimateBSpark) */
#define VIS_TIRES            6          /* 6-9: the tires 0-3 (AnimateTire) */
#define VIS_SHADOW           10         /* AnimateShadow */
#define VIS_ARROWS           11         /* the arrows over a human's car in split screen, one for each other
                                         * window: 11-12, and 13 with four windows */
#define VIS_ARROWS_END       13         /* StartArrow clears the unused arrow visuals below it */
#define VIS_FRAME            14         /* the body (AnimateFrame) */
#define VIS_WINGS            16         /* 16-17: the left and right wings (AnimateWing) */
#define VIS_WING_FLAMES      18         /* 18-19: their flames (AnimateWingFlame) */
#define VIS_WEAPON           20         /* the battle weapon (AnimateWeapon) */
#define CAR_VISUALS          21

/* gObjList: the scene object definitions the cars and the track's objects are made from, by kind
 * (InitVisuals, InitCarDynamicObjs, InitTrackDynamicObjs). */
#define OBJLIST_CAR_PARTS    0          /* 3 a car slot: the frame, the hood, the sheen (carObjNames), for 52
                                         * slots: 13 cars for each of the car screen's 4 players */
#define OBJLIST_TIRE         156        /* the tire (tire_object_names) */
#define OBJLIST_HULK         157        /* the burnt-out wreck; 157-215 the effects (InitTrackDynamicObjs_ObjNames) */
#define OBJLIST_SPARK        194
#define OBJLIST_ROCKET_FLAME 213        /* 213-215: the rocket car's three flames */
#define OBJLIST_WEAPONS      216        /* 216-223 (WeaponNames) */
#define OBJLIST_WINGS        224        /* 224-235: 4 per wing type: left, right, left flame, right flame (WingNames) */
#define OBJLIST_TRACK        236        /* the track's objects (TrkNames) */
#define OBJLIST_MINE         236        /* the mine instead of the first of them (the cone) when cones or cars
                                         * carry mines (gMineCones, gCarMines) */

/* StartSmoke's type: what the puffs come from (MAT3 smoke.c calls it `tire`). Types 0-3 are the tires (with
 * StartSmoke's `water` set, a water spray instead: StartWater). */
#define SMOKE_TIRE0          0          /* SMOKE_TIRE0 + n: tire n's smoke, n 0-3 */
#define SMOKE_WRECK          4          /* the wreck's grey smoke (APPEAR_WRECK_SMOKE) */
#define SMOKE_FIRE           5          /* a blast's flames (StartNewBlast) */
#define SMOKE_WING0          6          /* the wing's vapour trail on side 0 (-x), and on side 1 (+x): */
#define SMOKE_WING1          7

/* pc_tire_fx[car] (N64 and MAT3 gCarSmokeDir): each car's smoke latches. Puffs flip their animation's direction
 * one after the other, so neighbouring puffs turn opposite ways. */
#define SMOKE_SPIN_T0        0x1        /* tire 0's next puff turns backwards */
#define SMOKE_SPIN_T1        0x2
#define SMOKE_SPIN_T2        0x4
#define SMOKE_SPIN_T3        0x8
#define SMOKE_SPIN_WRECK     0x10       /* the same for the wreck's smoke */
#define SMOKE_WRECK_ON       0x20       /* the wreck's smoke has started (its grey, pc_smoke_shade, from 175) */
#define SMOKE_WRECK_WAIT1    0x40       /* the wreck smokes every third call: two calls to skip (WAIT1, then */
#define SMOKE_WRECK_WAIT2    0x80       /* WAIT2) after each puff, */
#define SMOKE_WRECK_WAIT3    0x80000    /* and a third with three or four players (the N64's) */
#define SMOKE_WATER_T0       0x100      /* tire 0 is throwing a water spray (its two puffs live) */
#define SMOKE_WATER_T1       0x200
#define SMOKE_WATER_T2       0x400
#define SMOKE_WATER_T3       0x800
#define SMOKE_SPIN_WING0     0x1000     /* SMOKE_SPIN_T0 for the wing trails */
#define SMOKE_SPIN_WING1     0x2000
#define SMOKE_TRAIL_WING0    0x4000     /* the wing trail has a last point (pc_trail_pts): the next puffs fill */
#define SMOKE_TRAIL_WING1    0x8000     /* the gap to it */

/* A smoke puff's data word (gSmokeObjs[].data) */
#define PUFF_FORWARD         0x1        /* its animation runs forwards (else backwards) */
#define PUFF_WATER           0x10       /* a water-spray puff: it follows its tire */
#define PUFF_TYPE_SHIFT      17         /* bits 17-20: its StartSmoke type */

/* A light streak's flags (gStreakArray[].flags: the trails behind battle shots, streaks.c and NewStreak). One of
 * the four widths, which also sets the colour (gStreakColors) and how long its ribbon lasts: */
#define STREAK_WIDE          0x1        /* 0.75 ft each side, 0.75 s (NewStreak type 0) */
#define STREAK_MEDIUM        0x2        /* 0.5 ft, 0.5 s */
#define STREAK_NARROW        0x4        /* 0.35 ft, 0.25 s */
#define STREAK_THIN          0x8        /* 0.025 ft, 0.16 s; it fades every frame (the others every 1/30 s) */
#define STREAK_NEWPOS        0x10       /* its shot moved: newpos is the ribbon's next point */
#define STREAK_DONE          0x1000     /* its shot is gone: free it, its ribbon fades out over 0.2 s */
#define STREAK_STARTED       0x2000     /* the ribbon has its first edge */

/* Data */
/* The six physics descriptors (MAT3's basic, forward, backward, shortie, stretch and truck, in MAT3's order), which
 * gCarModels points at by car type; nothing names them but the car set-up's pc_car_setup_data. */
extern _tCar car_descriptors[EXTENT(6)];
extern _tCar *gCarModels[EXTENT(13)];
extern char *CarNames[EXTENT(13)];
extern char *CarTitles[EXTENT(13)];
extern f32 gCarBraking[EXTENT(13)];
extern f32 gCarMass[EXTENT(13)];
extern f32 gCollisionMass[EXTENT(14)];
extern f32 gCrashThreshold[EXTENT(13)];
/* The car settings chosen (MAT3 [5][13] tables: gCarTrans ... gCarHorn, gTireRims, the paint choices in frontend.h):
 * row 0 the cars' own, row 1 + player each player's, a column per car type. */
extern s8 gCarTrans[5][13];
extern f32 carTransGearRatio[EXTENT(8)][6];
extern s8 gCarHandling[5][13];
extern s8 gCarEngines[5][13];
extern f32 carEngineTorque[EXTENT(9)][3];
extern s8 gCarTires[5][13];
extern f32 gTireDrift[EXTENT(5)][3];
extern f32 gTireSideForce[EXTENT(5)];
extern s8 gCarFrame[5][13];
extern f32 carDurability[EXTENT(6)];
/* gCarWings' rows past 4 aren't its: StartSmoke reads row slot + 1 for every car, which for a drone runs on into
 * gWingSizes and the tyre scales after it, as the N64 did; declared to the 13 rows that reaches (12 cars), the data
 * after share its storage (gen_data's OVERLAPS_REVIEWED). */
extern s8 gCarWings[13][13];
extern f32 gWingSizes[EXTENT(3)];
extern f32 gFrontTireScale[5][13];
extern f32 gRearTireScale[5][13];
extern s8 gCarDifferential[EXTENT(124)];
extern const struct _tSusp *carSuspensions[EXTENT(3)];
extern s8 gCarSuspensions[EXTENT(16)];
/* The tire rims chosen (MAT3 gTireRims[5][13]); TIRE_RIMS(row) is a row. */
extern s8 gTireRims[5][13];
#define TIRE_RIMS(row) (gTireRims[row])
extern s8 gCarHorn[5][13];
extern Tune44 gTune[EXTENT(3)];
extern u8 gDirtColor[EXTENT(4)];
extern f32 gCarMiddles[EXTENT(13)];
extern char *FireTexNames[EXTENT(6)];
extern s32 start_water_sentinel;
extern tRGBA smoke_white;
extern tRGBA smoke_wing_color1;
extern tRGBA smoke_wing_color2;
extern tRGBA smoke_wing_color3;
extern tRGBA smoke_road_color;
extern tRGBA smoke_dirt_color;
extern tRGBA gShadowColor;
/* gDummyVtx: declared by its files (src/blob/groups/animate_arrow/group.c: defines it) */
extern tRGBA SheenColors[EXTENT(19)][2];
extern tRGBA Track1Colors[EXTENT(78)];
extern tRGBA *gTrackColors[EXTENT(19)];
extern char *WeaponNames[EXTENT(8)];
extern char *WingNames[EXTENT(12)];
extern char *InitTrackDynamicObjs_ObjNames[EXTENT(59)];
extern char *TrkNames[EXTENT(162)];
extern char *RimNames[EXTENT(24)];
extern char *tire_object_names[EXTENT(1)];
extern char *TexNames[EXTENT(10)];
extern char *carObjNames[EXTENT(3)];
extern char *sheenName[EXTENT(12)];
extern s32 gTrackBaseColors[EXTENT(19)];
extern f32 wingOffsets[EXTENT(13)][3];
extern tRGBA arrowColors[EXTENT(5)];
extern u8 arrowinvsColor[EXTENT(12)];
extern ArrowColor arrow_base_color;
extern u16 taillight_colors_dark[EXTENT(8)];
extern tUVPair taillight_colors_dim;
extern u16 taillight_colors_bright[EXTENT(8)];
extern tUVPair headlight_colors_night;
extern s16 stdtorquecurve[EXTENT(120)];   /* MAT3 signed short [10][12]: torque by throttle and rpm */
extern TireDesc frontTire[EXTENT(2)];   /* MAT3 frontTire and backTire: the front and the rear tires' */
extern Collision16 collsize[EXTENT(13)];
extern f32 shadow_scales[EXTENT(14)][4];
extern BlitColor paint_colors_rgb[EXTENT(745)];
extern u8 tire_blur_texture[EXTENT(340)];
extern char car_part_name_format[EXTENT(96)];
extern u32 random_bit_masks[EXTENT(32)];
extern char car_palette_name_format[EXTENT(12)];
extern u8 car_mine_model_name[EXTENT(8)];
extern f32 rrspark_x_offset;
extern f32 rfspark_x_offset;
extern f32 lrspark_x_offset;
extern f32 lfspark_x_offset;
extern f32 wing_flame_height;
extern s16 gEnvCount;
extern s16 gEnvCountMax;
extern s8 gCarTeam[EXTENT(4)];
extern s8 gCarTeamColor[EXTENT(4)];   /* each team's paint (a colour choice) */
/* gEnvirons: declared by its files (src/blob/InitEnvs.c: defines it) */
extern struct tVisual *gEnvList;
extern struct Node *gFreeVis;
extern CarParts gCarParts[EXTENT(52)];
extern s16 gBlastMatCnt;
extern tBlastMat gBlastMats[EXTENT(50)];
extern PcPuff gSmokeObjs[EXTENT(100)];
extern tStatList gSmokeList;
extern u32 paint_colors[EXTENT(32)];
extern u16 gFireTexs[EXTENT(1416)];
extern u8 gCarPaintJobInited[EXTENT(52)];
/* Each car slot's paint (car_palette_init.c): the whole TLUT, its colours a heap block. A race's slots, or the car
 * select's 4 players x 13 cars (slot = car, UpdateSelectCar): 52, to gRealEdges on the N64, as gCarPaintJobInited. */
extern tPaletteDef car_palettes[EXTENT(52)];
extern s8 gInvisibleCar;
extern s8 gEnableTireScaling;
extern u16 gObjList[EXTENT(1)];
extern s16 gCarModelNums[EXTENT(13)];
extern u8 gGoodFrameRate;
extern s8 gMassiveMass;
extern struct tTexture *gWheelTextures[EXTENT(24)];
extern s8 gDriveHulk[EXTENT(4)];
extern tStatList gSkidList;
extern PcEffectObj gSkidArray[EXTENT(100)];
extern u16 gTexList[EXTENT(10)];
extern s8 gFrameScale;
extern s8 gFadePlayerArrow[EXTENT(4)][4];
extern u8 gStreakColors[EXTENT(4)][4];
extern tStatList gStreakList;
extern PcTrail gStreakArray[EXTENT(50)];
extern tStatList gStreakPolyList;
extern PcFade gStreakPolyArray[EXTENT(150)];

/* Functions */
void AddToEnvList(struct tVisual *v);
void AnimateArrow(struct tVisual *v, s16 op);
void AnimateBlast(struct tVisual *v, s16 op);
void AnimateBrakeLights(struct tVisual *v, s16 op);
void AnimateBSpark(struct tVisual *v, s16 op);
void AnimateFrame(struct tVisual *v, s16 op);
void AnimateLFSpark(struct tVisual *v, s16 op);
void AnimateLRSpark(struct tVisual *v, s16 op);
void AnimateNewBlast(struct tVisual *v, s16 op);
void AnimateNewBlastRing(struct tVisual *v, s16 op);
void AnimateRFSpark(struct tVisual *v, s16 op);
void AnimateRocketFlame(s16 slot, s32 hulk);
void AnimateRRSpark(struct tVisual *v, s16 op);
void AnimateShadow(struct tVisual *v, s16 op);
void AnimateSkids(void);
void AnimateTire(struct tVisual *v, s16 op);
void AnimateWing(struct Visual *v, s16 op);
void AnimateWingFlame(struct tVisual *v, s16 op);
u32 blend_rgb5551(u32 left, u32 right, s32 ratio);
s32 CheckVisible(struct tVisual *v, s32 on, s32 camera);
s32 CheckVisibleAll(struct tVisual *v, s32 on);
void ContinueSkid(void);
void CreateCar(s16 slot, s16 camlink, s32 for_game);
void *game_bsearch(void *key, void *base, u32 nmemb, u32 size, s32 (*compar)(void *, void *));
union Color GetCarsIRColor(struct tCarData *car, struct tModelData *m);
struct tVisual *GrabEnvEntry(void);
void HandleASpark(struct tVisual *v, s32 on, s32 spark, f32 x, f32 y, f32 z, s16 op);
void InitBlast(void);
void InitCarDynamicObjs(s16 slot, s16 car, s8 model);
s32 InitCarObj(s32 objnum, s16 camlink, s16 parent, s32 flags, s16 slot, s16 car_obj_num, s32 for_game);
void InitCarVisuals(struct tCarData *car);
void InitEnvs(void);
void InitSkids(void);
void InitSmoke(void);
void InitSmokeBlast(s16 slot);
void InitStreaks(void);
void InitTrackDynamicObjs(void);
void InitVisuals(s32 cars);
void InitWingSmoke(s16 slot, s16 side);
void LoadCars(s32 select);
u16 *MakeRamps(u16 *colors, u32 first_color, u32 second_color, u32 third_color);
u8 pick_random_masked_bit(u8 index);
u16 PickCar(s16 slot, u8 type);
void PositionObject(s32 objnum, f32 dx, f32 dy, f32 dz, f32 x, f32 y, f32 z);
void ReleaseEnviron(struct tVisual *v, s32 unlink);
void RemoveCar(s16 slot);
void RemoveEnvirons(void);
void SetFrameColor(s16 slot, u32 *col);
void SetSheen(s16 slot, s16 player, s16 sheen_index);
void SetSheenColor(s16 slot, union Color *col, union Color *env);
void SetupCar(s16 slot, u8 type, s32 for_game);
void SetupCarColor(s16 slot, s16 car, u8 first, u8 second, u8 third);
void StartBlast(void *data, s32 linkToCar, f32 scale, s32 playSound);
void StartCar(s16 slot);
void StartEnvironment(void);
void StartFrame(s16 slot);
void StartNewBlast(s16 slot);
void StartSmoke(s16 slot, u32 type, s32 water, f32 *smkoffset);
void StartSparks(s16 slot);
void StartWater(s16 slot, u32 tire);
void StartWing(s16 slot, s16 side);
void StartWingFlame(s16 slot, s16 side);
void StopSkid(struct tNewSkid *ns);
void UpdateEnvirons(void);
void UpdateShotObject(struct Shot *shot, s32 unt);
void UpdateSkids(s32 slot);
void UpdateSmoke(void);
void UpdateStreaks(void);
void UpdateVisuals(s16 slot);

#endif /* GAME_CARS_H */
