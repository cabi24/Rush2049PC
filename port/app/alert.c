/* port/app/alert.c: message boxes for what the player must know when the program can't go on (docs/port/RELEASE.md):
 * SDL2's SDL_ShowSimpleMessageBox, which needs no window or SDL_Init, or Windows' MessageBox in a build without
 * SDL2. RUSH2049_MESSAGE_BOX=print prints the box instead, for the tests; =off shows none. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PC_HAVE_SDL2
#include <SDL.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "app.h"

int pc_app_alert(int show, const char *title, const char *text)
{
    const char *mode = getenv("RUSH2049_MESSAGE_BOX");
    if (!show || (mode && !strcmp(mode, "off")))
        return -1;
    if (mode && !strcmp(mode, "print")) {
        fprintf(stderr, "rush2049pc: message box: %s\n%s\nrush2049pc: (end of message box)\n", title, text);
        return 0;
    }
#ifdef PC_HAVE_SDL2
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, NULL) == 0)
        return 0;
    fprintf(stderr, "rush2049pc: no message box: %s\n", SDL_GetError());
    return -1;
#elif defined(_WIN32)
    return MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR | MB_SETFOREGROUND) ? 0 : -1;
#else
    return -1;
#endif
}
