/* SetSheenColor.c: the colours of a car's sheen (MAT3 visuals.c, SetSheenColor()). */
#include "game/game.h"

typedef union Color { unsigned char b[4]; unsigned int w; } Color;

/* Give car `slot`'s sheen object the colour `col` and the environment colour `env` (CreateCar: white on the car
 * screen, the track's SheenColors pair in a race). */
void SetSheenColor(short slot, Color *col, Color *env)
{
    CarParts *car;
    car = &gCarParts[slot];
    ZOID_SetColor(car->sheen, col);
    ZOID_SetEnvColor(car->sheen, env);
}
