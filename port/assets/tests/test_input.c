/* port/assets/tests/test_input.c: the input records (D_8014A118 = input_rec0, gPlayers: 76 bytes on the N64, 80 on
 * the PC, where the profile pointer at +0x48 is 8 bytes) through the game's own code (docs/port/POINTERS.md 10):
 * the per-frame fill (process_inputs), the clear (entity_audio_update) and the controller hand-over
 * (random_seed_init), which add 80-byte strides by hand, the controls set-up through the profile pointer
 * (drone_target_update), and the packet slot at +0x44, which stays 32 bits (func_800D6530 posts a packet, a heap
 * block, or -1; func_800D5374 activates it; main_menu_render hands it on), and a new record of the player's save
 * data reached from its input record (func_800FDF88: the profile, SaveSlot.data, the data pointer read whole). Every
 * check is on records 1 to 3 too, so a 76-byte stride reads or writes the wrong bytes. The pause screen's title
 * (func_8010B5D0) names the player from its profile, whose name is at +0x20 on the PC (PROFILE_OFS(20)). The pause
 * screen's pages (the title, the language list func_8010B7FC, the two-choice page func_8010B9C8) show and hide their
 * blit through its Hide, at +0x26 of the Blit's host layout: the N64's +0x1A is the low byte of X (LAYOUT.md 9).
 * game: src/blob/process_inputs.c src/blob/entity_audio_update.c src/blob/random_seed_init.c
 * game: src/blob/drone_target_update.c src/blob/groups/vsync/func_800D6530.c
 * game: src/blob/groups/main_menu_input/object_activate.c src/blob/main_menu_render.c src/blob/func_800FDF88.c
 * game: src/blob/groups/func_8010B5D0/fb5d0.c src/blob/groups/func_8010B7FC/fb7fc.c
 * game: src/blob/groups/func_8010B9C8/fb9c8.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* An input record on the PC: the N64 fields in place, the profile pointer at +0x48 pointer-sized. */
typedef struct {
    uint8_t index, controller, pad2[2];
    uint32_t previous, current, pressed;
    float horizontal, vertical;
    uint32_t controls[10];
    uint8_t pad40[4];
    uint32_t packet;                 /* +0x44: a packet's window address, or -1 */
    void **profile;                  /* +0x48 */
} Input;
_Static_assert(sizeof(Input) == 80 && offsetof(Input, profile) == 0x48, "the input record's host layout");

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)
#define NF8(p, off) (((uint8_t *)(p))[off])

Input D_8014A118[4] __attribute__((aligned(16)));
ALIAS(input_rec0, D_8014A118, 0);
ALIAS(D_8014A160, D_8014A118, 0x48);   /* record 0's profile pointer, as gen_data places it */
int16_t active_player_count, D_8014A108;
uint32_t D_80156978[8], D_80156998[8], D_80143A00[8];
struct { float h, v; } D_80156958[8];
int32_t D_801174B4, D_8014A110;
int8_t D_80146115 = 10;
float D_801141B0[3], D_801141A4[3], D_80114198[3];
int8_t D_8013C228[10], D_8013C068[8][10], D_80139300[8];
uint32_t D_801118E4[32];
void *D_80146150[8];
struct { uint8_t pad0[0x22C]; float pos[3]; uint8_t pad238[0x808 - 0x238]; } D_8014A250[4];
struct { uint8_t pad0[12]; float a[3], b[3]; uint8_t pad24[152 - 36]; } gCamera[4];
uint8_t D_80142728[24];
struct { uint8_t indirect, doubly, pad[2]; uint32_t count; void *head, *tail; } D_80146170, D_80146188;

void controller_poll(void) {}
void audio_occlusion(void) {}
static int g_active[8];
void SetControllerActive(int32_t player, int32_t value) { g_active[player & 7] = value; }
void speed_set(float blend, float amount, int32_t first, int32_t second) {}
static uint32_t g_packets[4] = {0x80321000u, 0x80321040u, 0x80321080u, 0x803210C0u};
static int g_npacket;
void *func_800D63EC(void *a, void *b, void *c, void *d) { return (void *)(uintptr_t)g_packets[g_npacket++ & 3]; }
int32_t osRecvMesg(void *q, void *m, int32_t f) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t f) { return 0; }
void func_800D52CC(int32_t *node) {}
static void *g_moved[8];
static int g_nmoved;
void func_8009211C(void *list, int32_t *object) {}
void func_80091FBC(void *list, int32_t *object, void *before) { g_moved[g_nmoved++ & 7] = object; }
static void *g_handed[8];
static int g_nhanded;
void main_menu_input(void *node, float *a, float *b, float *c, float *d) { g_handed[g_nhanded++ & 7] = node; }
uint8_t D_801543D4;
int32_t D_8002EB90 = 0x123456, D_801117B4[4] = {11, 22, 33, 44};
int8_t D_80146127;
void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }
static void *g_written;
void PS_WriteFile(void **file, uintptr_t data, int32_t size) { g_written = (void *)data; }
void func_800FDF88(int32_t mode);
/* A profile (menu_save_options' SaveSlot on the PC, POINTERS.md 9.10): its file handle at +16, its data at +56. */
typedef struct { void *next, *prev; void **file; uint8_t pad24[56 - 24]; void **data; } SaveSlot;
_Static_assert(offsetof(SaveSlot, file) == PROFILE_OFS(8) && offsetof(SaveSlot, data) == PROFILE_OFS(0x2C),
               "the profile's host layout");
static void *g_saved[8];
static int g_nsaved;
void func_800CCE5C(void **profile, int8_t *controls) { g_saved[g_nsaved++ & 7] = profile; }

void process_inputs(void);
void entity_audio_update(void);
void random_seed_init(uint32_t controller, int32_t force);
void drone_target_update(int32_t reset);
void func_800D6530(void);
void func_800D5374(void);
void main_menu_render(void);

/* The fill, then the clear: four records, and the fields after them untouched. */
static void test_fill_and_clear(void)
{
    int p;

    memset(D_8014A118, 0, sizeof D_8014A118);
    for (p = 0; p < 4; p++) {
        D_8014A118[p].controller = (uint8_t)(p + 1);
        D_8014A118[p].profile = &D_80146150[p];
        D_80156978[p + 1] = 0x100u + (uint32_t)p, D_80156998[p + 1] = 0x200u + (uint32_t)p;
        D_80143A00[p + 1] = 0x300u + (uint32_t)p;
        D_80156958[p + 1].h = (float)p, D_80156958[p + 1].v = -(float)p;
    }
    D_8014A118[2].controller = 5;    /* no pad */
    active_player_count = 4;
    process_inputs();
    for (p = 0; p < 4; p++) {
        Input *r = &D_8014A118[p];
        if (p == 2) {
            CHECK(r->current == 0 && r->previous == 0 && r->pressed == 0 && r->horizontal == 0.0f);
        } else {
            CHECK(r->current == 0x100u + (uint32_t)p && r->previous == 0x200u + (uint32_t)p);
            CHECK(r->pressed == 0x300u + (uint32_t)p && r->horizontal == (float)p && r->vertical == -(float)p);
        }
        CHECK(r->controller == (p == 2 ? 5 : p + 1) && r->profile == &D_80146150[p]);
    }
    D_8014A118[3].packet = 0x12345678u;
    entity_audio_update();
    for (p = 0; p < 4; p++)
        CHECK(D_8014A118[p].current == 0 && D_8014A118[p].pressed == 0 && D_8014A118[p].vertical == 0.0f);
    CHECK(D_8014A118[3].controller == 4 && D_8014A118[3].profile == &D_80146150[3] && D_8014A118[3].packet == 0x12345678u);
}

/* A pad taking over record 3 (forced), and record 1 only when it has no pad and the button is down. */
static void test_hand_over(void)
{
    D_80156998[3] = 1, D_80156978[3] = 0x77, D_80143A00[3] = 0x88, D_80156958[3].h = 0.5f, D_80156958[3].v = 0.25f;
    D_8014A118[3].controller = 9;
    random_seed_init(3, 1);
    CHECK(D_8014A118[3].controller == 3 && D_8014A118[3].current == 0x77 && D_8014A118[3].previous == 1);
    CHECK(D_8014A118[3].pressed == 0x88 && D_8014A118[3].horizontal == 0.5f && D_8014A118[3].vertical == 0.25f);
    CHECK(D_8014A118[3].profile == &D_80146150[3] && g_active[3] == 1);
    D_8014A118[1].controller = 5, D_80156998[1] = 1;
    random_seed_init(1, 0);
    CHECK(D_8014A118[1].controller == 1 && D_8014A118[1].previous == 1 && D_8014A118[3].controller == 3);
}

/* The controls through the profile pointer: record 2's profile is passed whole. */
static void test_controls(void)
{
    int p, i;

    for (p = 0; p < 4; p++)
        D_8014A118[p].controller = (uint8_t)p;
    for (i = 0; i < 10; i++)
        D_8013C068[2][i] = (int8_t)i, D_801118E4[i] = 0x1000u << i;
    D_80139300[2] = 1;
    D_8014A108 = 3;
    g_nsaved = 0;
    drone_target_update(0);
    CHECK(g_nsaved == 1 && g_saved[0] == (void *)&D_80146150[2] && D_80139300[2] == 0);
    CHECK(D_8014A118[2].controls[0] == 0x1000u && D_8014A118[2].controls[9] == 0x1000u << 9);
}

/* The packet slot: posted, activated (zero-extended; -1 is none) and handed on. */
static void test_packets(void)
{
    int p;

    D_8014A108 = 3, D_8014A110 = 2, D_801174B4 = 0, g_npacket = 0;
    D_8014A118[0].index = 0, D_8014A118[1].index = 1, D_8014A118[2].index = 0;
    func_800D6530();
    CHECK(D_8014A118[0].packet == 0x80321000u && D_8014A118[1].packet == 0xFFFFFFFFu);
    CHECK(D_8014A118[2].packet == 0x80321040u && D_8014A118[2].profile == &D_80146150[2]);
    g_nhanded = 0;
    main_menu_render();
    CHECK(g_nhanded == 2 && g_handed[0] == (void *)(uintptr_t)0x80321000u);
    CHECK(g_handed[1] == (void *)(uintptr_t)0x80321040u);
    g_nmoved = 0;
    func_800D5374();
    CHECK(g_nmoved == 2 && g_moved[0] == (void *)(uintptr_t)0x80321000u && g_moved[1] == (void *)(uintptr_t)0x80321040u);
    for (p = 0; p < 3; p++)
        CHECK(D_8014A118[p].packet == 0xFFFFFFFFu && D_8014A118[p].profile == &D_80146150[p]);
}

/* The player's new record: through record 2's profile pointer to its save data, in the window. */
static void test_save_record(uint8_t *w)
{
    static SaveSlot slot;
    static void *file, *block;

    block = w + 0x330000;
    memset(block, 0xAA, 0x800);
    slot.file = &file, slot.data = &block;
    D_80146150[5] = &slot;
    D_8014A118[2].profile = &D_80146150[5];
    D_801543D4 = 2;
    D_80146127 = 1;
    g_written = NULL;
    func_800FDF88(3);
    CHECK(g_written == (uint8_t *)block + 0x6F4);
    CHECK(NF8(block, 0x6F4 + 8) == 3 && NF8(block, 0x6F4 + 4) == 1 && NF8(block, 0x6F4 + 5) == 1);
    CHECK(NF8(block, 0x6F4 + 7) == 44 && *(int32_t *)((uint8_t *)block + 0x6F4 + 0xC) == 0x23456);
    CHECK(NF8(block, 0x6F4 + 0x4C) == 0xAA && NF8(block, 0x6F3) == 0xAA);
}

/* the pause screen's title (func_8010B5D0): what it formats and draws */
struct { uint32_t opaque[6]; } D_801461D0;
int16_t D_80117280[2] = {160, 40};
int32_t D_80149D98, D_801170FC, D_8015698C, D_801146AC[4];
int8_t D_80117350, D_80117354;
char D_801210CC[] = "PLAYER %d", D_801210E0[] = "%s %s";
static struct { uint16_t pad[0x70 / 2]; uint16_t lang; } g_lang;
static intptr_t g_strings[12];
static intptr_t g_hdr[0x3A4 / 4 + 1];   /* the text header: host pointers, so N64 byte offsets / 4 are indices */
struct { uint8_t pad0[4]; void *hdr; uint8_t pad8[4]; void *info; intptr_t *strings; } D_8017A4E0 = {
    {0}, g_hdr, {0}, &g_lang, g_strings};
static const char *g_named;
void fcvt_wrapper(char *buf, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    g_named = __builtin_va_arg(ap, const char *);
    __builtin_va_end(ap);
    strcpy(buf, "title");
}
int32_t slot_state_setup(int32_t font) { return 0; }
int16_t object_bytes_sum_global(void) { return 10; }   /* the line height */
/* sound_control.c's Blit: the layout every PC view has (POINTERS.md 9.3). */
typedef struct PauseBlit {
    const char *Name;
    void *Image, *Info;
    uint16_t TexIndex;
    int16_t X, Y;
    uint16_t Z;
    int16_t Width, Height;
    uint8_t Alpha, Flip;
    int8_t Hide, Init;
    int16_t Top, Bot, Left, Right, color;
    uint16_t reserved;
    void *AnimFunc;
    uint32_t AnimID;
    intptr_t AnimDTA;
    uint16_t BLIdx, reserved36;
    void *data, *child;
} PauseBlit;
_Static_assert(offsetof(PauseBlit, Hide) == 0x26 && offsetof(PauseBlit, X) == 0x1A, "the Blit's host offsets");
static int g_updates;
static PauseBlit g_seen;
void Input_ApplyPadConfig(void *item) { g_updates++, g_seen = *(PauseBlit *)item; }   /* UpdateBlit */
void render_helper(float a) {}
uint32_t object_manager_update(void *text, int32_t n) { return 40; }
void dispatch_handler(int32_t colour) {}
static const char *g_drawn;
void state_utility(int16_t x, int16_t y, void *text) { g_drawn = text; }
int32_t func_8010B5D0(void *item);
int32_t func_8010B7FC(void *item);
int32_t func_8010B9C8(void *item);

static const char g_page[] = "PAGE";

static void page_blit(PauseBlit *b, int8_t hide)
{
    memset(b, 0, sizeof *b);
    b->Name = g_page, b->X = 0x1234, b->Y = 0x56, b->Width = 200, b->Height = 30, b->Alpha = 255, b->Hide = hide;
    g_updates = 0;
}

static void test_pause_title(void)
{
    static SaveSlot slot;
    static PauseBlit item;

    strcpy((char *)&slot + PROFILE_OFS(20), "ALICE");
    D_80146150[6] = &slot;
    D_8014A118[1].profile = &D_80146150[6];
    D_8015698C = 1;
    g_strings[0] = (intptr_t)"PAUSED";
    g_named = g_drawn = NULL;
    page_blit(&item, 0);
    CHECK(func_8010B5D0(&item) == 1);
    CHECK(g_named == (char *)&slot + PROFILE_OFS(20) && strcmp(g_named, "ALICE") == 0 && g_drawn != NULL);
    CHECK(item.X == 0x1234 && item.Hide == 0 && g_updates == 0);   /* shown already: nothing to update */

    /* hidden: the title shows its blit (Hide, host +0x26), once, and leaves X (host +0x1A) as it was */
    page_blit(&item, 1);
    g_drawn = NULL;
    CHECK(func_8010B5D0(&item) == 1 && g_drawn != NULL);
    CHECK(item.Hide == 0 && item.X == 0x1234 && item.Y == 0x56 && item.Name == g_page);
    CHECK(g_updates == 1 && g_seen.Hide == 0 && g_seen.X == 0x1234);
    CHECK(func_8010B5D0(&item) == 1 && g_updates == 1);
}

/* the language list: shown the same way, eight strings after the current language's */
static void test_pause_languages(void)
{
    static PauseBlit item;
    int i;

    for (i = 0; i < 12; i++)
        g_strings[i] = (intptr_t)"LANGUAGE";
    g_strings[9] = (intptr_t)"LAST";
    g_lang.lang = 1;
    page_blit(&item, 1);
    g_drawn = NULL;
    CHECK(func_8010B7FC(&item) == 1);
    CHECK(item.Hide == 0 && item.X == 0x1234 && g_updates == 1 && g_seen.Hide == 0);
    CHECK(g_drawn != NULL && strcmp(g_drawn, "LAST") == 0);   /* strings[lang + 8], the last one drawn */
    g_lang.lang = 0;
}

/* the two-choice page: hidden while D_801170FC < 7 (nothing drawn), shown from 7 on */
static void test_pause_choice(void)
{
    static PauseBlit item;

    g_hdr[0x360 / 4] = (intptr_t)"QUIT?";
    g_strings[0] = (intptr_t)"YES", g_strings[1] = (intptr_t)"NO";
    page_blit(&item, 0);
    D_801170FC = 3;
    g_drawn = NULL;
    CHECK(func_8010B9C8(&item) == 1 && g_drawn == NULL);
    CHECK(item.Hide == 1 && item.X == 0x1234 && g_updates == 1 && g_seen.Hide == 1);
    CHECK(func_8010B9C8(&item) == 1 && g_updates == 1 && g_drawn == NULL);   /* hidden already */
    D_801170FC = 7;
    CHECK(func_8010B9C8(&item) == 1 && g_drawn != NULL && strcmp(g_drawn, "NO") == 0);
    CHECK(item.Hide == 0 && item.X == 0x1234 && g_updates == 2 && g_seen.Hide == 0);
    D_801170FC = 0;
}

int main(void)
{
    uint8_t *w = test_rdram();   /* the packets are heap blocks: window addresses */

    if (!w) {
        fprintf(stderr, "test_input: the RDRAM window can't be mapped here: skipped\n");
        return 77;
    }
    memset(w + 0x321000, 0, 0x100);
    test_fill_and_clear();
    test_hand_over();
    test_controls();
    test_packets();
    test_save_record(w);
    test_pause_title();
    test_pause_languages();
    test_pause_choice();
    return TEST_DONE();
}
