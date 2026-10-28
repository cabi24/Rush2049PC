/* port/app/alert.c: message boxes for what the player must know when the program can't go on (docs/port/RELEASE.md):
 * SDL2's SDL_ShowSimpleMessageBox, which needs no window or SDL_Init, or Windows' MessageBox in a build without
 * SDL2. A box can ask too (pc_app_ask: SDL_ShowMessageBox with buttons), as the ROM's box asks whether to choose the
 * file. RUSH2049_MESSAGE_BOX=print prints the box instead, for the tests; =off shows none; neither asks anything.
 * RUSH2049_TEST_ROM_PICK (pick.c) prints every box and answers the questions from its list. */
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

enum { SHOW, PRINT, OFF };

static int box_mode(void)
{
    const char *mode = getenv("RUSH2049_MESSAGE_BOX");
    if (pc_app_pick_test())
        return PRINT;
    if (mode && !strcmp(mode, "off"))
        return OFF;
    if (mode && !strcmp(mode, "print"))
        return PRINT;
    return SHOW;
}

/* The box on the real stderr even when the output is captured (startup.c, RUSH2049_CONSOLE=none), then more (the
 * buttons and the answer, for pc_app_ask's test hook) when it is set. */
static void print_box(const char *title, const char *text, const char *more)
{
    int fd = pc_app_console_print_fd();
    size_t n = strlen(title) + strlen(text) + strlen(more) + 80;
    char *s = malloc(n);
    if (!s)
        return;
    n = (size_t)snprintf(s, n, "rush2049pc: message box: %s\n%s\nrush2049pc: (end of message box)\n%s", title, text,
                         more);
    fflush(stderr);
    for (size_t done = 0; done < n;) {
        long k = (long)write(fd, s + done, (unsigned)(n - done));
        if (k <= 0)
            break;
        done += (size_t)k;
    }
    free(s);
    pc_app_box_shown();
}

/* 1 when the box is printed or not shown at all, 0 when it is to be shown. */
static int print_or_skip(int show, const char *title, const char *text)
{
    int mode = box_mode();
    if (!show || mode == OFF)
        return 1;
    if (mode == PRINT) {
        print_box(title, text, "");
        return 1;
    }
    return 0;
}

int pc_app_box_interactive(int show)
{
    return show && (pc_app_pick_test() || box_mode() == SHOW);
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

#ifdef PC_HAVE_SDL2
static Uint32 sdl_kind(int kind)
{
    return kind == PC_APP_BOX_WARNING ? SDL_MESSAGEBOX_WARNING
           : kind == PC_APP_BOX_INFO  ? SDL_MESSAGEBOX_INFORMATION
                                      : SDL_MESSAGEBOX_ERROR;
}
#endif

int pc_app_alert_kind(int show, int kind, const char *title, const char *text)
{
    if (print_or_skip(show, title, text))
        return 0;
#ifdef PC_HAVE_SDL2
    if (SDL_ShowSimpleMessageBox(sdl_kind(kind), title, text, NULL) == 0) {
        pc_app_box_shown();
        return 0;
    }
    fprintf(stderr, "rush2049pc: no message box: %s\n", SDL_GetError());
    return -1;
#elif defined(_WIN32)
    pc_app_box_shown();
    return MessageBoxA(NULL, text, title,
                       MB_OK | MB_SETFOREGROUND | (kind == PC_APP_BOX_WARNING ? MB_ICONWARNING
                                                   : kind == PC_APP_BOX_INFO  ? MB_ICONINFORMATION
                                                                              : MB_ICONERROR))
               ? 0
               : -1;
#else
    (void)kind;
    return -1;
#endif
}

int pc_app_alert(int show, const char *title, const char *text)
{
    return pc_app_alert_kind(show, PC_APP_BOX_ERROR, title, text);
}

int pc_app_ask(int show, int kind, const char *title, const char *text, const char *const *buttons, int n)
{
    if (!show || n < 1 || n > 4)
        return -1;
    if (pc_app_pick_test()) {
        /* the test hook: the first button while it has answers left (the file dialog's), else the last */
        char more[256];
        size_t k = 0;
        int answer = pc_app_pick_pending() ? 0 : n - 1;
        k += (size_t)snprintf(more + k, sizeof more - k, "rush2049pc: buttons:");
        for (int i = 0; i < n && k < sizeof more; i++)
            k += (size_t)snprintf(more + k, sizeof more - k, " [%s]", buttons[i]);
        if (k < sizeof more)
            snprintf(more + k, sizeof more - k, "; chosen: %s\n", buttons[answer]);
        print_box(title, text, more);
        return answer;
    }
    if (print_or_skip(show, title, text))
        return -1;
#ifdef PC_HAVE_SDL2
    SDL_MessageBoxButtonData b[4];
    for (int i = 0; i < n; i++) {
        b[i].flags = i == 0 ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT : 0;
        if (i == n - 1)
            b[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        b[i].buttonid = i;
        b[i].text = buttons[i];
    }
    Uint32 flags = sdl_kind(kind);
#ifdef SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT
    flags |= SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;   /* in the order given, on every system */
#endif
    SDL_MessageBoxData box = {flags, NULL, title, text, n, b, NULL};
    int hit = -1;
    if (SDL_ShowMessageBox(&box, &hit) == 0) {
        pc_app_box_shown();
        return hit >= 0 && hit < n ? hit : n - 1;   /* closed: the last button (Quit) */
    }
    fprintf(stderr, "rush2049pc: no message box: %s\n", SDL_GetError());
    return -1;
#elif defined(_WIN32)
    /* Windows' own box has no buttons of ours: OK is the first, Cancel the last */
    pc_app_box_shown();
    int r = MessageBoxA(NULL, text, title, MB_OKCANCEL | MB_SETFOREGROUND |
                                               (kind == PC_APP_BOX_WARNING ? MB_ICONWARNING : MB_ICONERROR));
    return r == IDOK ? 0 : r ? n - 1 : -1;
#else
    (void)kind;
    return -1;
#endif
}
