#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>


struct game
{
    struct display
    {
        int fps;
        int width, height;
        struct termios term_orig, term_raw;
        char *data;
    } display;

    struct timeval t;

    struct player
    {
        double v;
        double a;
        double x;
        double y;
    } player;

    struct world
    {
        struct wall
        {
            double x;
            double hole_center;
            double hole_radius;
        } *walls;
        size_t walls_n;
        size_t wall_first;
        double wall_v;
        long int wall_seed;
    } world;
};

static void deinit(struct game *);
static void draw(struct game *);
static void erase(struct game *);
static int getch(void);
static void init(struct game *);
static void input(struct game *, int);
static void pixel(struct game *, int, int, char);
static void player_crash(struct game *);
static void player_win(struct game *);
static void resize_and_clear(struct game *);
static void tick(struct game *);

static double time_delta(struct timeval *, struct timeval *);


void
deinit(struct game *g)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g->display.term_orig);
    printf("\e[?12l\e[?25h");  /* cnorm */
    printf("\e[?1049l");       /* leave secondary screen */
}

void
draw(struct game *g)
{
    double hole_world_center, hole_min, hole_max;
    int x, y;
    size_t i;

    pixel(g, g->player.x, g->display.height / 2 + g->player.y, '@');

    for (i = g->world.wall_first; i < g->world.walls_n; i++)
    {
        hole_world_center = g->display.height / 2 + g->world.walls[i].hole_center;
        hole_min = hole_world_center - g->world.walls[i].hole_radius;
        hole_max = hole_world_center + g->world.walls[i].hole_radius;

        for (y = 0; y < g->display.height; y++)
            if (y < hole_min || y > hole_max)
                pixel(g, g->world.walls[i].x, y, '#');
    }

    printf("\e[1;1H");  /* top left */
    for (y = 0; y < g->display.height; y++)
        for (x = 0; x < g->display.width; x++)
            putchar(g->display.data[y * g->display.width + x]);
    fflush(stdout);
}

int
getch(void)
{
    char c;
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    if (FD_ISSET(STDIN_FILENO, &fds))
    {
        read(STDIN_FILENO, &c, 1);
        return (int)c;
    }
    else
        return -1;
}

void
erase(struct game *g)
{
    memset(g->display.data, ' ', g->display.width * g->display.height);
}

void
init(struct game *g)
{
    size_t i;
    double process;
    int x;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
    {
        fprintf(stderr, "Are we really connected to a terminal?\n");
        exit(1);
    }

    tcgetattr(STDIN_FILENO, &g->display.term_orig);
    g->display.term_raw = g->display.term_orig;
    cfmakeraw(&g->display.term_raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &g->display.term_raw);

    printf("\e[?25l");    /* civis */
    printf("\e[?1049h");  /* enter secondary screen */

    g->display.fps = 60;
    g->display.width = g->display.height = 0;
    g->display.data = NULL;

    g->player.v = 0;
    g->player.a = 100;
    g->player.x = 5;
    g->player.y = 0;

    gettimeofday(&g->t, NULL);

    g->world.wall_v = 30;
    g->world.wall_first = 0;
    g->world.walls_n = 1024;
    g->world.walls = calloc(g->world.walls_n, sizeof(struct wall));
    if (g->world.walls == NULL)
    {
        fprintf(stderr, "Could not allocate memory for walls\n");
        exit(1);
    }

    srand48(g->world.wall_seed);
    x = 50;
    for (i = 0; i < g->world.walls_n; i++)
    {
        process = (double)(i + 1) / g->world.walls_n;
        process = sqrt(sqrt(process));

        g->world.walls[i].x = x;
        g->world.walls[i].hole_center = (drand48() * 2 - 1) * process * 20;
        g->world.walls[i].hole_radius = (1 - process) * 10 + process;

        x += (1 - process) * 50 + process * 5;
    }
}

void
input(struct game *g, int ch)
{
    if (ch == ' ')
    {
        g->player.y -= 2;
        g->player.v = -5;
    }
}

void
pixel(struct game *g, int x, int y, char c)
{
    if (x < 0 || x >= g->display.width || y < 0 || y >= g->display.height)
        return;

    g->display.data[y * g->display.width + x] = c;
}

void
player_crash(struct game *g)
{
    deinit(g);
    printf("You crashed. Score: %ld\n", g->world.wall_first);
    exit(0);
}

void
player_win(struct game *g)
{
    deinit(g);
    printf("You actually did it! You won the game.\n");
    exit(0);
}

void
resize_and_clear(struct game *g)
{
    struct winsize w;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (w.ws_col != g->display.width || w.ws_row != g->display.height)
    {
        if (g->display.data != NULL)
            free(g->display.data);

        g->display.data = malloc(w.ws_col * w.ws_row);
        if (g->display.data == NULL)
        {
            fprintf(stderr, "Could not allocate memory for display data\n");
            exit(1);
        }
    }

    g->display.width = w.ws_col;
    g->display.height = w.ws_row;

    erase(g);
}

void
tick(struct game *g)
{
    double dt;
    struct timeval t2;
    size_t i;
    double hole_world_center, hole_min, hole_max;
    double player_world_y;

    gettimeofday(&t2, NULL);
    dt = time_delta(&g->t, &t2);

    g->player.v += g->player.a * dt;
    g->player.y += g->player.v * dt;

    player_world_y = g->display.height / 2 + g->player.y;
    if (player_world_y < 0 || player_world_y >= g->display.height)
        player_crash(g);

    for (i = g->world.wall_first; i < g->world.walls_n; i++)
        g->world.walls[i].x -= g->world.wall_v * dt;

    while (g->world.walls[g->world.wall_first].x < 0)
    {
        g->world.wall_first++;
        if (g->world.wall_first == g->world.walls_n)
            player_win(g);
    }

    /* XXX This part is very similar to draw(). Refactor. */
    for (i = g->world.wall_first; i < g->world.walls_n; i++)
    {
        hole_world_center = g->display.height / 2 + g->world.walls[i].hole_center;
        hole_min = hole_world_center - g->world.walls[i].hole_radius;
        hole_max = hole_world_center + g->world.walls[i].hole_radius;

        if (g->world.walls[i].x >= g->player.x &&
            g->world.walls[i].x < g->player.x + 1)
            if (player_world_y < hole_min || player_world_y > hole_max)
                player_crash(g);
    }

    g->t = t2;
}

double
time_delta(struct timeval *t1, struct timeval *t2)
{
    return (t2->tv_sec - t1->tv_sec) +
           (double)(t2->tv_usec - t1->tv_usec) / 1000 / 1000;
}


int
main(int argc, char **argv)
{
    int ch;
    struct game g;

    if (argc > 1)
        g.world.wall_seed = strtol(argv[1], NULL, 10);
    else
        g.world.wall_seed = 0;

    init(&g);

    for (;;)
    {
        resize_and_clear(&g);

        ch = getch();
        if (ch == 0x03)  /* ^C */
            break;

        draw(&g);

        input(&g, ch);
        tick(&g);

        usleep(1.0 / g.display.fps * 1000 * 1000);
    }

    deinit(&g);

    return 0;
}
