/* port/app/icon.c: the program's icon on its window (docs/port/WINDOWS.md, "Icon and version").
 *
 * The image is dist/rush2049pc.svg drawn at 64 x 64 by tools/port/make_icon.py, kept zlib-compressed in
 * icon_data.h. On Linux the window gets it with SDL_SetWindowIcon (X11's _NET_WM_ICON, Wayland's xdg-toplevel-icon
 * where SDL has it). On Windows the window already has the executable's icon: SDL registers its window class with
 * the first icon group of the program's resources (rush2049pc.rc), and leaves the small one to Windows, which picks
 * the size that suits the DPI from the .ico's 16 to 256 px. SDL_SetWindowIcon would replace both with this one
 * 64 px image, so it isn't called there. */
#include "app.h"

#if defined(PC_HAVE_SDL2) && !defined(_WIN32)
#include <SDL.h>
#include <zlib.h>

#include "icon_data.h"

void pc_app_window_icon(void)
{
    static unsigned char rgba[PC_ICON_RGBA_BYTES];
    uLongf n = sizeof rgba;
    SDL_Window *w = SDL_GL_GetCurrentWindow();
    if (!w || uncompress(rgba, &n, pc_icon_rgba_z, sizeof pc_icon_rgba_z) != Z_OK || n != sizeof rgba)
        return;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(rgba, PC_ICON_SIZE, PC_ICON_SIZE, 32, PC_ICON_SIZE * 4,
                                                        SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return;
    SDL_SetWindowIcon(w, s);
    SDL_FreeSurface(s);
}
#else
void pc_app_window_icon(void)
{
}
#endif
