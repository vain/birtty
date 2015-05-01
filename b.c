#include <ncurses.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>


struct game
{
    int fps;

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
    } world;
};

static void game_draw(struct game *, int, int);
static void game_init(struct game *);
static void game_input(struct game *, int);
static void game_make_walls(struct game *);
static void game_tick(struct game *);

static double time_delta(struct timeval *, struct timeval *);


void
game_draw(struct game *g, int width, int height)
{
    double hole_world_center, hole_min, hole_max;
    int y;
    size_t i;

    (void)width;

    mvaddch(height / 2 + g->player.y, g->player.x, '@');

    for (i = 0; i < g->world.walls_n; i++)
    {
        hole_world_center = height / 2 + g->world.walls[i].hole_center;
        hole_min = hole_world_center - g->world.walls[i].hole_radius;
        hole_max = hole_world_center + g->world.walls[i].hole_radius;

        for (y = 0; y < height; y++)
        {
            if (y < hole_min || y > hole_max)
            {
                mvaddch(y, g->world.walls[i].x, '|');
            }
        }
    }

    //mvprintw(0, 0, "a = %f", g->player.a);
    //mvprintw(1, 0, "v = %f", g->player.v);
    //mvprintw(2, 0, "y = %f", g->player.y);
}

void
game_init(struct game *g)
{
    g->fps = 60;

    g->player.v = 0;
    g->player.a = 40;
    g->player.x = 5;
    g->player.y = 0;

    gettimeofday(&g->t, NULL);
    g->world.walls_n = 4;  /* XXX increase to 1024 */
    g->world.walls = calloc(g->world.walls_n, sizeof(struct wall));
    if (g->world.walls == NULL)
        abort();
    game_make_walls(g);
    g->world.wall_v = 10;
}

void
game_input(struct game *g, int ch)
{
    if (ch == ' ')
    {
        g->player.y -= 2;
        g->player.v = -5;
    }
}

void
game_make_walls(struct game *g)
{
    g->world.wall_first = 0;

    g->world.walls[0].x = 50;
    g->world.walls[0].hole_center = -10;
    g->world.walls[0].hole_radius = 4;

    g->world.walls[1].x = 70;
    g->world.walls[1].hole_center = 2;
    g->world.walls[1].hole_radius = 4;

    g->world.walls[2].x = 90;
    g->world.walls[2].hole_center = 0;
    g->world.walls[2].hole_radius = 4;

    g->world.walls[3].x = 110;
    g->world.walls[3].hole_center = 10;
    g->world.walls[3].hole_radius = 4;
}

void
game_tick(struct game *g)
{
    double dt;
    struct timeval t2;
    size_t i;

    gettimeofday(&t2, NULL);
    dt = time_delta(&g->t, &t2);

    g->player.v += g->player.a * dt;
    g->player.y += g->player.v * dt;

    for (i = 0; i < g->world.walls_n; i++)
        g->world.walls[i].x -= g->world.wall_v * dt;

    while (g->world.walls[0].x < 0)
    {
        for (i = 0; i < g->world.walls_n - 1; i++)
            g->world.walls[i] = g->world.walls[i + 1];
        bzero(&g->world.walls[g->world.walls_n - 1], sizeof(struct wall));
    }

    g->t = t2;
}

double
time_delta(struct timeval *t1, struct timeval *t2)
{
    return (t2->tv_sec - t1->tv_sec) + (double)(t2->tv_usec - t1->tv_usec) / 1000 / 1000;
}


int
main()
{
    int ch;
    int width, height;
    struct game g;

    initscr();
    noecho();
    raw();
    timeout(0);
    curs_set(0);

    game_init(&g);

    for (;;)
    {
        erase();
        getmaxyx(stdscr, height, width);

        ch = getch();
        if (ch == 0x03)  /* ^C */
            break;

        game_draw(&g, width, height);
        game_input(&g, ch);
        game_tick(&g);

        refresh();
        usleep(1.0 / g.fps * 1000 * 1000);
    }

    endwin();

    return 0;
}
