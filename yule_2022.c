#include <yed/plugin.h>

#define LOG(...)                                                   \
do {                                                               \
    LOG_FN_ENTER();                                                \
    yed_log(__VA_ARGS__);                                          \
    LOG_EXIT();                                                    \
} while (0)


typedef struct {
    yed_glyph glyph;
    yed_attrs attrs;
} Cell;

typedef struct {
    const char **bg;
    const char **fg;
    const char **ch;
    int          width;
    int          height;
} Sprite;


#define SPRITE(_name) \
static const Sprite _name = { _name##_bg, _name##_fg, _name##_ch, _name##_width, _name##_height }

#include "santa1.sprite"
#include "santa2.sprite"
#include "cloud1.sprite"
#include "moon.sprite"
#include "chimney.sprite"
#include "game_over.sprite"


typedef struct {
    const Sprite *sprite;
    int           x;
    int           y;
} Moving;

#define SCREEN_H       (24)
#define SCREEN_W       (80)
#define MIN_UPDT_HZ    (30)
#define FRAME_HZ       (10)
#define SANTA_MAX_JUMP (10)
#define MAX_CHIMNIES   (2)


static yed_plugin        *Self;
static yed_event_handler  key_handler;
static yed_event_handler  draw_handler;
static Cell               screen[SCREEN_H][SCREEN_W];
static int                game_on;
static int                game_is_over;
static int                save_hz;
static u64                frame;
static u64                t;
static int                santa_jumping;
static int                santa_jump;
static int                points;
static Moving             chimnies[MAX_CHIMNIES]; /* sprite is NULL if not currently used */

static void ho_ho_hop(int n_args, char **args);
static void ekey(yed_event *event);
static void edraw(yed_event *event);
static void unload(yed_plugin *self);

int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();

    Self = self;

    yed_plugin_set_command(Self, "ho-ho-hop", ho_ho_hop);

    key_handler.kind  = EVENT_KEY_PRESSED;
    key_handler.fn    = ekey;
    draw_handler.kind = EVENT_PRE_DIRECT_DRAWS;
    draw_handler.fn   = edraw;

    yed_plugin_set_unload_fn(Self, unload);

    return 0;
}

static void clear_screen(void) {
    yed_attrs a;
    int       r;
    int       c;

    a = yed_parse_attrs("fg @231 bg @16");

    for (r = 1; r <= SCREEN_H; r += 1) {
        for (c = 1; c <= SCREEN_W; c += 1) {
            screen[r - 1][c - 1].glyph = G(' ');
            screen[r - 1][c - 1].attrs = a;
        }
    }
}

static void start_game(void) {
    int i;

    if (game_on) { return; }

    LOG("Starting Ho Ho Hop!");

    yed_plugin_add_event_handler(Self, key_handler);
    yed_plugin_add_event_handler(Self, draw_handler);

    save_hz = yed_get_update_hz();
    if (save_hz < MIN_UPDT_HZ) {
        yed_set_update_hz(MIN_UPDT_HZ);
    }

    for (i = 0; i < MAX_CHIMNIES; i += 1) {
        chimnies[i].sprite = NULL;
    }

    points        = 0;
    game_is_over  = 0;
    santa_jumping = 0;
    santa_jump    = 0;
    frame         = 0;
    t             = 0;
    game_on       = 1;
}

static void end_game(void) {
    if (!game_on) { return; }

    yed_set_update_hz(save_hz);

    yed_delete_event_handler(draw_handler);
    yed_delete_event_handler(key_handler);
    YEXE("redraw");
    game_on = 0;
    LOG("Game ended.");
}

static void ho_ho_hop(int n_args, char **args) {
    if (game_on) {
        yed_cerr("You're already playing!");
        return;
    }

    if (ys->term_rows < SCREEN_H || ys->term_cols < SCREEN_W) {
        yed_cerr("Your screen is too small. Must be at least %d by %d.", SCREEN_H, SCREEN_W);
        return;
    }

    start_game();
}

static void ekey(yed_event *event) {
    if (IS_MOUSE(event->key)) { return; }

    switch (event->key) {
        case 'q':
            end_game();
            break;
        case 'r':
            end_game();
            start_game();
            break;
        case ENTER:
            if (!game_is_over && !santa_jumping) {
                santa_jumping = 1;
                santa_jump    = 1;
            }
            break;
        default:
            break;
    }
    event->cancel = 1;
}

static void print(int r, int c, const char *msg) {
    if (r > SCREEN_H) { return; }

    for (; c <= SCREEN_W; c += 1) {
        if (!*msg || iscntrl(*msg)) { break; }
        screen[r - 1][c - 1].glyph = G(*msg);
        screen[r - 1][c - 1].attrs.fg = 231;
        ATTR_SET_FG_KIND(screen[r - 1][c - 1].attrs.flags, ATTR_KIND_256);
        msg += 1;
    }
}


static void paint(void) {
    int r;
    int c;

    for (r = 1; r <= SCREEN_H; r += 1) {
        for (c = 1; c <= SCREEN_W; c += 1) {
            yed_set_cursor(r, c);
            yed_set_attr(screen[r - 1][c - 1].attrs);
            yed_screen_print_n_over(&(screen[r - 1][c - 1].glyph.c), 1);
        }
    }
}

static void draw_stars(void) {
    yed_attrs star_attrs;

    star_attrs = yed_parse_attrs("fg @228 bg @16");

#define STAR(_r, _c) \
    print((_r), (_c), "*"); screen[(_r) - 1][(_c) - 1].attrs = star_attrs;

    STAR(5,  5);
    STAR(5,  19);
    STAR(12, 5);
    STAR(2,  35);
    STAR(8,  41);
    STAR(10, 47);
    STAR(16, 56);
    STAR(4,  78);
}

static void draw_roof(void) {
    yed_attrs roof_attrs;
    int       roof_start;
    int       f;
    int       i;
    int       j;

    roof_attrs = yed_parse_attrs("fg @16 bg @235");
    roof_start = 15;
    for (j = 0; j < SCREEN_W; j += 1) {
        screen[roof_start - 1][j].attrs = roof_attrs;
    }
    print(roof_start, 1, "================================================================================");

    f = 7 - (frame % 8);
    for (i = roof_start + 1; i <= SCREEN_H; i += 1) {
        for (j = 0; j < SCREEN_W; j += 1) {
            screen[i - 1][j].glyph = G(f == ((j + i) % 8) ? '/' : '_');
            screen[i - 1][j].attrs = roof_attrs;
        }
    }
}

static void draw_sprite(const Sprite *sprite, int y, int x) {
    int r;
    int c;

    yed_attrs black        = yed_parse_attrs("fg @16  bg @16");
    yed_attrs white        = yed_parse_attrs("fg @231 bg @231");
    yed_attrs grey         = yed_parse_attrs("fg @250 bg @250");
    yed_attrs red          = yed_parse_attrs("fg @124 bg @124");
    yed_attrs yellow       = yed_parse_attrs("fg @178 bg @178");
    yed_attrs light_yellow = yed_parse_attrs("fg @228 bg @228");
    yed_attrs tan          = yed_parse_attrs("fg @180 bg @180");
    yed_attrs dark_blue    = yed_parse_attrs("fg @17  bg @17");

    for (r = 0; r < sprite->height; r += 1) {
        if (y + r >= SCREEN_H || y + r < 0) { continue; }

        for (c = 0; c < sprite->width; c += 1) {
            if (x + c >= SCREEN_W || x + c < 0) { continue; }

            if (sprite->ch[r][c] == 'X') { continue; }

            screen[y + r][x + c].glyph = G(sprite->ch[r][c]);

            yed_attrs bga = ZERO_ATTR;
            yed_attrs fga = ZERO_ATTR;

            switch (sprite->bg[r][c]) {
                case 'k': bga = black;        break;
                case 'w': bga = white;        break;
                case 'G': bga = grey;         break;
                case 'r': bga = red;          break;
                case 'y': bga = yellow;       break;
                case 'Y': bga = light_yellow; break;
                case 't': bga = tan;          break;
                case 'B': bga = dark_blue;    break;
            }
            switch (sprite->fg[r][c]) {
                case 'k': fga = black;        break;
                case 'w': fga = white;        break;
                case 'G': fga = grey;         break;
                case 'r': fga = red;          break;
                case 'y': fga = yellow;       break;
                case 'Y': fga = light_yellow; break;
                case 't': fga = tan;          break;
                case 'B': fga = dark_blue;    break;
            }

            if (bga.flags != 0) {
                screen[y + r][x + c].attrs.bg = bga.bg;
                ATTR_SET_BG_KIND(screen[y + r][x + c].attrs.flags, ATTR_BG_KIND(bga.flags));
            }

            if (fga.flags != 0) {
                screen[y + r][x + c].attrs.fg = fga.fg;
                ATTR_SET_FG_KIND(screen[y + r][x + c].attrs.flags, ATTR_BG_KIND(fga.flags));
            }
        }
    }

}

static int colliding(int l1, int t1, int w1, int h1,
                     int l2, int t2, int w2, int h2) {

    return !(   l1 + w1 < l2
             || l2 + w2 < l1
             || t1 + h1 < t2
             || t2 + h2 < t1);
}

static void handle_chimnies(int santa_y, int santa_x) {
    int i;

    for (i = 0; i < MAX_CHIMNIES; i += 1) {
        if (chimnies[i].sprite == NULL) {
            if (rand() % 10 == 0) {
                chimnies[i].sprite = &chimney;
                chimnies[i].x      = SCREEN_W + chimney.width;
                chimnies[i].y      = 19;
                break;
            }
        }

        if (chimnies[i].sprite != NULL) {
            if (colliding(santa_x + 4, santa_y, santa1.width - 8, santa1.height,
                          chimnies[i].x + 1, chimnies[i].y, chimney.width - 2, chimney.height)) {
                game_is_over = 1;
            }

            draw_sprite(chimnies[i].sprite, chimnies[i].y, chimnies[i].x);
            chimnies[i].x -= 5;

            if (chimnies[i].x <= -chimney.width) {
                chimnies[i].sprite = NULL;
                points += 1;
            }
        }
    }
}

static void handle_jumping(void) {
    if (santa_jumping) {
        santa_jump += 2 * santa_jumping;
        if (santa_jump >= SANTA_MAX_JUMP) {
            santa_jumping = -1;
        }

        if (santa_jump <= 0) {
            santa_jumping = santa_jump = 0;
        }
    }
}

static void game_frame(void) {
    int  santa_y;
    int  santa_x;
    char buff[1024];
    int  len;

    if (game_is_over) {
        draw_sprite(&game_over, (SCREEN_H - game_over.height) / 2, (SCREEN_W - game_over.width) / 2);
        return;
    }

    clear_screen();

    draw_stars();
    draw_roof();

    draw_sprite(&moon, 1, 70);
    draw_sprite(&cloud1, 0, SCREEN_W - ((frame / 3 + 48) % (SCREEN_W + cloud1.width)));
    draw_sprite(&cloud1, 3, SCREEN_W - ((frame + 3) % (SCREEN_W + cloud1.width)));
    draw_sprite(&cloud1, 10, SCREEN_W - ((frame / 2 + 15) % (SCREEN_W + cloud1.width)));

    santa_y = 7 - santa_jump;
    santa_x = 5;

    handle_chimnies(santa_y, santa_x);

    draw_sprite(frame & 1 || santa_jump ? &santa1 : &santa2, santa_y, santa_x);

    handle_jumping();

    print(1, 1,  "Press 'q' to quit, 'r' to restart. Press enter to hop.");
    len = snprintf(buff, sizeof(buff), "Score: %3d", points);
    print(1, SCREEN_W - len + 1, buff);


    frame += 1;
}

static void edraw(yed_event *event) {
    u64 now;

    now = measure_time_now_ms();

    if (now - t >= (1000 / FRAME_HZ)) {
        game_frame();
        t = now;
    }

    paint();
}

static void unload(yed_plugin *self) {
    end_game();
}
