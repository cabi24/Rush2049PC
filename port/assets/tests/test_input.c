/* port/assets/tests/test_input.c: the input records (gPlayers: 76 bytes on the N64, 80 on
 * the PC, where the profile pointer at +0x48 is 8 bytes) through the game's own code (docs/port/POINTERS.md 10):
 * the per-frame fill (UpdateRushControls), the clear (ClearRushControls) and the controller hand-over
 * (FindPlayerNum), which add 80-byte strides by hand, the controls set-up through the profile pointer
 * (SetControls), and the packet slot at +0x44, which stays 32 bits (StartListeners posts a packet, a heap
 * block, or -1; StopListeners activates it; UpdateListeners hands it on), and a new record of the player's save
 * data reached from its input record (NewCircuit: the profile, SaveSlot.data, the data pointer read whole). Every
 * check is on records 1 to 3 too, so a 76-byte stride reads or writes the wrong bytes. The pause screen's title
 * (animTitleText) names the player from its profile, whose name is at +0x20 on the PC (PROFILE_OFS(20)). The pause
 * screen's pages (the title, the language list animPauseText, the two-choice page animSureText) show and hide their
 * blit through its Hide, at +0x26 of the Blit's host layout: the N64's +0x1A is the low byte of X (LAYOUT.md 9).
 * game: src/blob/UpdateRushControls.c src/blob/ClearRushControls.c src/blob/FindPlayerNum.c
 * game: src/blob/SetControls.c src/blob/groups/game_sounds/StartListeners.c
 * game: src/blob/groups/listeners/AudRemoveListener.c src/blob/UpdateListeners.c src/blob/NewCircuit.c
 * game: src/blob/groups/animTitleText/animTitleText.c src/blob/groups/animPauseText/animPauseText.c
 * game: src/blob/groups/animSureText/animSureText.c src/blob/groups/state/halfont.c src/blob/TB_CenterText.c
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

Input gPlayers[4] __attribute__((aligned(16)));
ALIAS(gPlayers_save, gPlayers, 0x48);   /* record 0's profile pointer, as gen_data places it */
int16_t gNumPlayers, gNumPlayers;
uint32_t gContLevels[8], gContEdges[8], gAutoEdges[8];
struct { float h, v; } gContStick[8];
int32_t gGameState, gRace;
int8_t gOptions[41] = {[13] = 10};   /* the game options: [13] the effects volume, [31] the circuit's death option */
float unitmatrix_origin[3], unitmatrix[3][3];
int8_t gDefaultControls[10], gNewControls[8][10], gControlsSet[8];
uint32_t gControlTable[32];
void *gJustPlayers[8];
struct pc_model pc_models[PC_MODEL_SLOTS];
struct { uint8_t pad0[12]; float a[3], b[3]; uint8_t pad24[152 - 36]; } gCamera[4];
uint8_t audio_lock_queue[24];
struct { uint8_t indirect, doubly, pad[2]; uint32_t count; void *head, *tail; } gFreeListeners, gActiveListeners;

void UpdateControls(void) {}
void ClearControls(void) {}
static int g_active[8];
void SetControllerActive(int32_t player, int32_t value) { g_active[player & 7] = value; }
void AudSetMasterVol(float blend, float amount, int32_t first, int32_t second) {}
static uint32_t g_packets[4] = {0x80321000u, 0x80321040u, 0x80321080u, 0x803210C0u};
static int g_npacket;
void *AudAddListener(void *a, void *b, void *c, void *d) { return (void *)(uintptr_t)g_packets[g_npacket++ & 3]; }
int32_t osRecvMesg(void *q, void *m, int32_t f) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t f) { return 0; }
void ValidateListener(int32_t *node) {}
static void *g_moved[8];
static int g_nmoved;
void RemoveLLE(void *list, int32_t *object) {}
void InsertLLE(void *list, int32_t *object, void *before) { g_moved[g_nmoved++ & 7] = object; }
static void *g_handed[8];
static int g_nhanded;
void AudUpdateListener(void *node, float *a, float *b, float *c, float *d) { g_handed[g_nhanded++ & 7] = node; }
uint8_t gCircuitPlayer;
int32_t gSeconds = 0x123456, gCircuitDiff[4] = {11, 22, 33, 44};
void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }
static void *g_written;
void PS_WriteFile(void **file, uintptr_t data, int32_t size) { g_written = (void *)data; }
void NewCircuit(int32_t mode);
/* A profile (SaveInit' SaveSlot on the PC, POINTERS.md 9.10): its file handle at +16, its data at +56. */
typedef struct { void *next, *prev; void **file; uint8_t pad24[56 - 24]; void **data; } SaveSlot;
_Static_assert(offsetof(SaveSlot, file) == PROFILE_OFS(8) && offsetof(SaveSlot, data) == PROFILE_OFS(0x2C),
               "the profile's host layout");
static void *g_saved[8];
static int g_nsaved;
void SavePlayerControls(void **profile, int8_t *controls) { g_saved[g_nsaved++ & 7] = profile; }

void UpdateRushControls(void);
void ClearRushControls(void);
void FindPlayerNum(uint32_t controller, int32_t force);
void SetControls(int32_t reset);
void StartListeners(void);
void StopListeners(void);
void UpdateListeners(void);

/* The fill, then the clear: four records, and the fields after them untouched. */
static void test_fill_and_clear(void)
{
    int p;

    memset(gPlayers, 0, sizeof gPlayers);
    for (p = 0; p < 4; p++) {
        gPlayers[p].controller = (uint8_t)(p + 1);
        gPlayers[p].profile = &gJustPlayers[p];
        gContLevels[p + 1] = 0x100u + (uint32_t)p, gContEdges[p + 1] = 0x200u + (uint32_t)p;
        gAutoEdges[p + 1] = 0x300u + (uint32_t)p;
        gContStick[p + 1].h = (float)p, gContStick[p + 1].v = -(float)p;
    }
    gPlayers[2].controller = 5;    /* no pad */
    gNumPlayers = 4;
    UpdateRushControls();
    for (p = 0; p < 4; p++) {
        Input *r = &gPlayers[p];
        if (p == 2) {
            CHECK(r->current == 0 && r->previous == 0 && r->pressed == 0 && r->horizontal == 0.0f);
        } else {
            CHECK(r->current == 0x100u + (uint32_t)p && r->previous == 0x200u + (uint32_t)p);
            CHECK(r->pressed == 0x300u + (uint32_t)p && r->horizontal == (float)p && r->vertical == -(float)p);
        }
        CHECK(r->controller == (p == 2 ? 5 : p + 1) && r->profile == &gJustPlayers[p]);
    }
    gPlayers[3].packet = 0x12345678u;
    ClearRushControls();
    for (p = 0; p < 4; p++)
        CHECK(gPlayers[p].current == 0 && gPlayers[p].pressed == 0 && gPlayers[p].vertical == 0.0f);
    CHECK(gPlayers[3].controller == 4 && gPlayers[3].profile == &gJustPlayers[3] && gPlayers[3].packet == 0x12345678u);
}

/* A pad taking over record 3 (forced), and record 1 only when it has no pad and the button is down. */
static void test_hand_over(void)
{
    gContEdges[3] = 1, gContLevels[3] = 0x77, gAutoEdges[3] = 0x88, gContStick[3].h = 0.5f, gContStick[3].v = 0.25f;
    gPlayers[3].controller = 9;
    FindPlayerNum(3, 1);
    CHECK(gPlayers[3].controller == 3 && gPlayers[3].current == 0x77 && gPlayers[3].previous == 1);
    CHECK(gPlayers[3].pressed == 0x88 && gPlayers[3].horizontal == 0.5f && gPlayers[3].vertical == 0.25f);
    CHECK(gPlayers[3].profile == &gJustPlayers[3] && g_active[3] == 1);
    gPlayers[1].controller = 5, gContEdges[1] = 1;
    FindPlayerNum(1, 0);
    CHECK(gPlayers[1].controller == 1 && gPlayers[1].previous == 1 && gPlayers[3].controller == 3);
}

/* The controls through the profile pointer: record 2's profile is passed whole. */
static void test_controls(void)
{
    int p, i;

    for (p = 0; p < 4; p++)
        gPlayers[p].controller = (uint8_t)p;
    for (i = 0; i < 10; i++)
        gNewControls[2][i] = (int8_t)i, gControlTable[i] = 0x1000u << i;
    gControlsSet[2] = 1;
    gNumPlayers = 3;
    g_nsaved = 0;
    SetControls(0);
    CHECK(g_nsaved == 1 && g_saved[0] == (void *)&gJustPlayers[2] && gControlsSet[2] == 0);
    CHECK(gPlayers[2].controls[0] == 0x1000u && gPlayers[2].controls[9] == 0x1000u << 9);
}

/* The packet slot: posted, activated (zero-extended; -1 is none) and handed on. */
static void test_packets(void)
{
    int p;

    gNumPlayers = 3, gRace = 2, gGameState = 0, g_npacket = 0;
    gPlayers[0].index = 0, gPlayers[1].index = 1, gPlayers[2].index = 0;
    StartListeners();
    CHECK(gPlayers[0].packet == 0x80321000u && gPlayers[1].packet == 0xFFFFFFFFu);
    CHECK(gPlayers[2].packet == 0x80321040u && gPlayers[2].profile == &gJustPlayers[2]);
    g_nhanded = 0;
    UpdateListeners();
    CHECK(g_nhanded == 2 && g_handed[0] == (void *)(uintptr_t)0x80321000u);
    CHECK(g_handed[1] == (void *)(uintptr_t)0x80321040u);
    g_nmoved = 0;
    StopListeners();
    CHECK(g_nmoved == 2 && g_moved[0] == (void *)(uintptr_t)0x80321000u && g_moved[1] == (void *)(uintptr_t)0x80321040u);
    for (p = 0; p < 3; p++)
        CHECK(gPlayers[p].packet == 0xFFFFFFFFu && gPlayers[p].profile == &gJustPlayers[p]);
}

/* The player's new record: through record 2's profile pointer to its save data, in the window. */
static void test_save_record(uint8_t *w)
{
    static SaveSlot slot;
    static void *file, *block;

    block = w + 0x330000;
    memset(block, 0xAA, 0x800);
    slot.file = &file, slot.data = &block;
    gJustPlayers[5] = &slot;
    gPlayers[2].profile = &gJustPlayers[5];
    gCircuitPlayer = 2;
    gOptions[31] = 1;
    g_written = NULL;
    NewCircuit(3);
    CHECK(g_written == (uint8_t *)block + 0x6F4);
    CHECK(NF8(block, 0x6F4 + 8) == 3 && NF8(block, 0x6F4 + 4) == 1 && NF8(block, 0x6F4 + 5) == 1);
    CHECK(NF8(block, 0x6F4 + 7) == 44 && *(int32_t *)((uint8_t *)block + 0x6F4 + 0xC) == 0x23456);
    CHECK(NF8(block, 0x6F4 + 0x4C) == 0xAA && NF8(block, 0x6F3) == 0xAA);
}

/* the pause screen's title (animTitleText): what it formats and draws */
struct { uint32_t opaque[6]; } text_lock_queue;
int16_t pause_titlePos[2] = {160, 40};
int32_t pauseMenuSelect, gPauseState, playerInPause, playerTextColor[4];
int8_t pause_gControllerRemoved, pause_gYes;
char pause_controller_removed_format[] = "PLAYER %d", pause_player_title_format[] = "%s %s";
static struct { uint16_t pad[0x70 / 2]; uint16_t lang; } g_lang;
static intptr_t g_strings[12];
static intptr_t g_hdr[0x3A4 / 4 + 1];   /* the text header: host pointers, so N64 byte offsets / 4 are indices */
struct { uint8_t pad0[4]; void *hdr; uint8_t pad8[4]; void *info; intptr_t *strings; } gText = {
    {0}, g_hdr, {0}, &g_lang, g_strings};
static const char *g_named;
void TB_Sprintf(char *buf, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    g_named = __builtin_va_arg(ap, const char *);
    __builtin_va_end(ap);
    strcpy(buf, "title");
}
int32_t HAL_SetFontInternal(int32_t font) { return 0; }
int16_t TB_GetFontLineHeight(void) { return 10; }   /* the line height */
/* NewMultiBlit.c's Blit: the layout every PC view has (POINTERS.md 9.3). */
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
void UpdateBlit(void *item) { g_updates++, g_seen = *(PauseBlit *)item; }   /* UpdateBlit */
void HAL_SetTextDepth(float a) {}
uint32_t HAL_GetTextWidth(void *text, int32_t n) { return 40; }
void TB_SetTextColor(int32_t colour) {}
static const char *g_drawn;
void TB_DrawText(int16_t x, int16_t y, void *text) { g_drawn = text; }
int32_t animTitleText(void *item);
int32_t animPauseText(void *item);
int32_t animSureText(void *item);

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
    gJustPlayers[6] = &slot;
    gPlayers[1].profile = &gJustPlayers[6];
    playerInPause = 1;
    g_strings[0] = (intptr_t)"PAUSED";
    g_named = g_drawn = NULL;
    page_blit(&item, 0);
    CHECK(animTitleText(&item) == 1);
    CHECK(g_named == (char *)&slot + PROFILE_OFS(20) && strcmp(g_named, "ALICE") == 0 && g_drawn != NULL);
    CHECK(item.X == 0x1234 && item.Hide == 0 && g_updates == 0);   /* shown already: nothing to update */

    /* hidden: the title shows its blit (Hide, host +0x26), once, and leaves X (host +0x1A) as it was */
    page_blit(&item, 1);
    g_drawn = NULL;
    CHECK(animTitleText(&item) == 1 && g_drawn != NULL);
    CHECK(item.Hide == 0 && item.X == 0x1234 && item.Y == 0x56 && item.Name == g_page);
    CHECK(g_updates == 1 && g_seen.Hide == 0 && g_seen.X == 0x1234);
    CHECK(animTitleText(&item) == 1 && g_updates == 1);
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
    CHECK(animPauseText(&item) == 1);
    CHECK(item.Hide == 0 && item.X == 0x1234 && g_updates == 1 && g_seen.Hide == 0);
    CHECK(g_drawn != NULL && strcmp(g_drawn, "LAST") == 0);   /* strings[lang + 8], the last one drawn */
    g_lang.lang = 0;
}

/* the two-choice page: hidden while gPauseState < 7 (nothing drawn), shown from 7 on */
static void test_pause_choice(void)
{
    static PauseBlit item;

    g_hdr[0x360 / 4] = (intptr_t)"QUIT?";
    g_strings[0] = (intptr_t)"YES", g_strings[1] = (intptr_t)"NO";
    page_blit(&item, 0);
    gPauseState = 3;
    g_drawn = NULL;
    CHECK(animSureText(&item) == 1 && g_drawn == NULL);
    CHECK(item.Hide == 1 && item.X == 0x1234 && g_updates == 1 && g_seen.Hide == 1);
    CHECK(animSureText(&item) == 1 && g_updates == 1 && g_drawn == NULL);   /* hidden already */
    gPauseState = 7;
    CHECK(animSureText(&item) == 1 && g_drawn != NULL && strcmp(g_drawn, "NO") == 0);
    CHECK(item.Hide == 0 && item.X == 0x1234 && g_updates == 2 && g_seen.Hide == 0);
    gPauseState = 0;
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
