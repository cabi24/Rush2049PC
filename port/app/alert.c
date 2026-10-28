/* port/app/alert.c: message boxes for what the player must know when the program can't go on (docs/port/RELEASE.md):
 * SDL2's SDL_ShowSimpleMessageBox, which needs no window or SDL_Init, or Windows' MessageBox in a build without
 * SDL2. RUSH2049_MESSAGE_BOX=print prints the box instead, for the tests; =off shows none. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef PC_HAVE_SDL2
#include <SDL.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "app.h"

/* 1 when the box is printed or not shown at all (RUSH2049_MESSAGE_BOX), 0 when it is to be shown. Printed, it goes
 * to the real stderr even when the output is captured (startup.c, RUSH2049_CONSOLE=none). */
static int print_or_skip(int show, const char *title, const char *text)
{
    const char *mode = getenv("RUSH2049_MESSAGE_BOX");
    if (!show || (mode && !strcmp(mode, "off")))
        return 1;
    if (mode && !strcmp(mode, "print")) {
        int fd = pc_app_console_print_fd();
        size_t n = strlen(title) + strlen(text) + 80;
        char *s = malloc(n);
        if (!s)
            return 1;
        n = (size_t)snprintf(s, n, "rush2049pc: message box: %s\n%s\nrush2049pc: (end of message box)\n", title, text);
        fflush(stderr);
        for (size_t done = 0; done < n;) {
            long k = (long)write(fd, s + done, (unsigned)(n - done));
            if (k <= 0)
                break;
            done += (size_t)k;
        }
        free(s);
        pc_app_box_shown();
        return 1;
    }
    return 0;
}

int pc_app_alert_native(int show, const char *title, const char *text)
{
#ifdef _WIN32
    if (print_or_skip(show, title, text))
        return 0;
    pc_app_box_shown();
    return MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST) ? 0 : -1;
#else
    return pc_app_alert(show, title, text);
#endif
}

int pc_app_alert(int show, const char *title, const char *text)
{
    if (print_or_skip(show, title, text))
        return 0;
#ifdef PC_HAVE_SDL2
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, NULL) == 0) {
        pc_app_box_shown();
        return 0;
    }
    fprintf(stderr, "rush2049pc: no message box: %s\n", SDL_GetError());
    return -1;
#elif defined(_WIN32)
    pc_app_box_shown();
    return MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR | MB_SETFOREGROUND) ? 0 : -1;
#else
    return -1;
#endif
}
