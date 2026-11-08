/* The PC layer's own struct member of the same spelling as a game datum (Windows' DISPLAYCONFIG_PATH_INFO has a
 * targetInfo): not the datum, so not renamed. */
typedef struct { int id; } DISPLAYCONFIG_PATH_TARGET_INFO;
typedef struct { DISPLAYCONFIG_PATH_TARGET_INFO targetInfo; } DISPLAYCONFIG_PATH_INFO;

int display_target(const DISPLAYCONFIG_PATH_INFO *path, DISPLAYCONFIG_PATH_INFO copy)
{
    return path->targetInfo.id + copy.targetInfo.id;
}
