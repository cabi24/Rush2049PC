/* port/config/presets.c: the presets, several settings in one step (docs/port/FRONTEND.md "Presets").
 *
 * A preset is a list of settings and their values, as the file writes them. It is not a setting of its own: the
 * settings overlay's Presets page sets each of its settings as if the player had changed it (so Save writes them and
 * the pages show them changed), and --preset NAME puts them on the command line at its place (so a later --set wins,
 * and Save doesn't write them). The file only ever holds the settings themselves.
 *
 * n64 puts every setting another preset sets back to its default (the N64's game; widescreen's default is auto, the
 * window's shape). enhanced and maximum leave out what changes the races (game.cars), the player's own content and
 * choices (mods, saves, accessories, the race HUD's place, the bindings), and what belongs to the display and the
 * machine (scale, the window, fullscreen, resolution, vsync, video.frame_rate, the graphics API, the audio output,
 * the volume, the latency). frontend.presets checks that every value is one the settings take and that n64 is the
 * defaults of every setting the others set. */
#include <strings.h>

#include "config.h"

static const PcSettingValue n64_values[] = {
    {"game", "frame_rate", "original"},    {"game", "memory", "original"},     {"video", "widescreen", "auto"},
    {"video", "draw_distance", "original"}, {"video", "detail", "original"},    {"video", "fog", "original"},
    {"video", "texture_lod", "original"},   {"video", "textures", "original"},  {"audio", "voices", "original"},
    {"audio", "engines", "original"},
};

/* the game at 60, more drawn further and sharper, every drone heard; the N64's art, fog and races */
static const PcSettingValue enhanced_values[] = {
    {"game", "frame_rate", "60"},          {"video", "draw_distance", "far"}, {"video", "detail", "high"},
    {"video", "texture_lod", "anisotropic"}, {"video", "widescreen", "auto"}, {"audio", "voices", "64"},
    {"audio", "engines", "all"},           {"game", "memory", "64"},
};

/* enhanced, and the whole track drawn without fog (which would hide it), the world's textures upscaled */
static const PcSettingValue maximum_values[] = {
    {"game", "frame_rate", "60"},          {"video", "draw_distance", "max"}, {"video", "detail", "high"},
    {"video", "fog", "off"},               {"video", "texture_lod", "anisotropic"},
    {"video", "textures", "upscaled"},     {"video", "widescreen", "auto"},   {"audio", "voices", "64"},
    {"audio", "engines", "all"},           {"game", "memory", "64"},
};

#define N(a) (int)(sizeof a / sizeof *a)
static const PcPreset presets[] = {
    {"n64", "N64 original", "The defaults: the N64's game, sound and 30 fps", n64_values, N(n64_values)},
    {"enhanced", "PC enhanced", "60 fps, far, high detail, every engine heard", enhanced_values,
     N(enhanced_values)},
    {"maximum", "PC maximum", "Enhanced, the whole track, no fog, upscaled", maximum_values, N(maximum_values)},
};

int pc_presets(const PcPreset **out)
{
    *out = presets;
    return N(presets);
}

const PcPreset *pc_preset_find(const char *name)
{
    for (int i = 0; name && i < N(presets); i++)
        if (!strcasecmp(name, presets[i].name))
            return &presets[i];
    return NULL;
}

int pc_preset_in_effect(const PcSettings *s, const PcPreset *p)
{
    static PcSettings want;   /* large: not on the stack */
    char a[64], b[64];
    want = *s;
    for (int i = 0; i < p->n; i++) {   /* each value as the settings write it: "60" and "60.0" alike */
        if (pc_settings_set(&want, p->values[i].section, p->values[i].key, p->values[i].value, NULL, 0) != 0)
            return 0;
        pc_settings_get(s, p->values[i].section, p->values[i].key, a, sizeof a);
        pc_settings_get(&want, p->values[i].section, p->values[i].key, b, sizeof b);
        if (strcasecmp(a, b))
            return 0;
    }
    return 1;
}
#undef N
