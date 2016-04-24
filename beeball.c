#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_ttf.h>

#include "anim.h"
#include "input.h"
#include "memory.h"
#include "physics.h"
#include "random.h"
#include "resource.h"


#define MAX_PADDLES 10
#define MAX_BALLS 20
#define MAX_HOLES 20
#define MAX_POWERUPS 20

#define PERCENT_POWERUPS_APPEAR 10
#define MAX_POWERUP_BOUNCES 4 /* Hits to the field border before disappearing */
#define LONG_POWERUP_EFFECT_TIME 20 /* In seconds */
#define MEDIUM_POWERUP_EFFECT_TIME 8 /* In seconds */
#define SHORT_POWERUP_EFFECT_TIME 5 /* In seconds */

#define BALL_SPEED 150
#define BALL_HYPER_SPEED ((BALL_SPEED) * 10)
#define POWERUP_SPEED 50

#define STRING_LENGTH 256

#define MILLIS_PER_SECOND 1000


const float FPS = 100;
const int CANVAS_W = 640;
const int CANVAS_H = 480;
const int BLOCK_SIZE = 20;


ALLEGRO_DISPLAY *display = NULL;


typedef enum DIRECTION {
    NORTH = 0,
    WEST,
    SOUTH,
    EAST
} DIRECTION;


typedef struct BOX {
    int u;
    int l;
    int d;
    int r;
} BOX;


typedef struct RECT {
    int x;
    int y;
    int w;
    int h;
} RECT;


typedef enum BLOCK_TYPE {
    BLOCK_EMPTY = 0,
    BLOCK_DAISY,
    BLOCK_ROSE,
    BLOCK_FERN,
    NUM_BLOCK_TYPES
} BLOCK_TYPE;


typedef struct BODY {
    BOX box;
    float x;
    float y;
    float velx;
    float vely;
} BODY;


typedef struct PADDLE {
    BODY body;
    ANIM *anim;
    ALLEGRO_BITMAP *shadow;
    char orientation; /* 'H' for horizontal, 'V' for vertical */
} PADDLE;


typedef struct HOLE {
    BODY body;
    ANIM *anim;
    ANIM *normal_anim;
    ANIM *chomp_anim;
} HOLE;


typedef enum POWERUP_TYPE {
    POWERUP_NONE = 0,
    POWERUP_DRILL,
    POWERUP_SCATTER,
    POWERUP_HYPER,
    POWERUP_BLAST,
    NUM_POWERUP_TYPES
} POWERUP_TYPE;


typedef struct POWERUP {
    BODY body;
    POWERUP_TYPE type;
    ANIM *anim;
    float effect_timer;
    int bounces;
} POWERUP;


typedef struct BALL {
    BODY body;
    int speed;
    
    ANIM *anim;
    ALLEGRO_BITMAP *shadow;
    float facing; /* The bee randomly rotates as he hits stuff */
    int paddlehit; /* Is true when the ball has been hit by a paddle */
    int dead;
    
    POWERUP_TYPE powerup_type;
    float powerup_timer;
} BALL;


typedef struct BLOCK {
    ALLEGRO_BITMAP *bitmap;
    int hits;
} BLOCK;


typedef struct MAP {
    int width; /* The width in blocks */
    int height; /* The height in blocks */
    BLOCK *blocks;
    int num_blocks; /* The number of blocks (not including spaces) */
} MAP;


typedef struct FIELD {
    char description[STRING_LENGTH];
    
    MAP *map;

    PADDLE *paddles[MAX_PADDLES];
    int num_paddles;

    HOLE *holes[MAX_HOLES];
    int num_holes;
    
    BALL *balls[MAX_BALLS];
    
    POWERUP *powerups[MAX_POWERUPS];

    ALLEGRO_EVENT_QUEUE *events;
    
    /* Default values for new balls in this field */
    float default_ball_x;
    float default_ball_y;
    float default_ball_velx;
    float default_ball_vely;
} FIELD;


typedef struct PLAYER {
    int lives;
} PLAYER;


typedef struct GAME {
    PLAYER *player;
    FIELD *field;
    
    float mousescale; /* Scale the mouse position to match the screen */
} GAME;


int north_edge(float y, BOX *box)
{
    return y - box->u;
}


int west_edge(float x, BOX *box)
{
    return x - box->l;
}


int south_edge(float y, BOX *box)
{
    return y + box->d;
}


int east_edge(float x, BOX *box)
{
    return x + box->r;
}


int y_from_north_edge(float y, BOX *box)
{
    return y + box->u;
}


int x_from_west_edge(float x, BOX *box)
{
    return x + box->l;
}


int y_from_south_edge(float y, BOX *box)
{
    return y - box->d;
}


int x_from_east_edge(float x, BOX *box)
{
    return x - box->r;
}


int field_width(FIELD *field)
{
    return field->map->width * BLOCK_SIZE;
}


int field_height(FIELD *field)
{
    return field->map->height * BLOCK_SIZE;
}


float seconds_to_millis(int seconds)
{
    return seconds * MILLIS_PER_SECOND;
}


/* ANSI C doesn't define PI */
#define MATH_PI 3.141592654


float to_radians(float degrees)
{
    /* Radians = degrees * (Pi / 180) */
    return degrees * (MATH_PI / (float)180);
}


float velx_from_angle(float angle, int speed)
{
    return (float)sin(to_radians(angle)) * speed;
}


float vely_from_angle(float angle, int speed)
{
    return -(float)cos(to_radians(angle)) * speed;
}


int change_in_x(DIRECTION dir)
{
    /* North, west, south, east */
    static int change_x[4] = {0, -1, 0, 1};
    
    return change_x[dir];
}


int change_in_y(DIRECTION dir)
{
    /* North, west, south, east */
    static int change_y[4] = {-1, 0, 1, 0};
    
    return change_y[dir];
}


void destroy_powerup(POWERUP *powerup)
{
    free_memory("POWERUP", powerup);
}


POWERUP *create_powerup(int x, int y, POWERUP_TYPE type)
{
    static int angles[4] = {45, 135, 225, 315};
    
    POWERUP *powerup = NULL;
    int angle = 0;
    
    powerup = alloc_memory("POWERUP", sizeof(POWERUP));
    
    powerup->body.box.u = 6;
    powerup->body.box.l = 6;
    powerup->body.box.d = 6;
    powerup->body.box.r = 6;
    powerup->body.x = x;
    powerup->body.y = y;
    
    angle = angles[random_number(0, 3)];
    powerup->body.velx = velx_from_angle(angle, POWERUP_SPEED);
    powerup->body.vely = vely_from_angle(angle, POWERUP_SPEED);
    
    powerup->type = type;
    
    /* After 4 bounces the powerup leaves the screen */
    powerup->bounces = 0;
    
    powerup->anim = create_anim(0, 0);
    
    if (type == POWERUP_BLAST) {
        powerup->effect_timer = seconds_to_millis(LONG_POWERUP_EFFECT_TIME);
        add_frame(powerup->anim, load_resource_image("powerup-blast.bmp"));
    } else if (type == POWERUP_DRILL) {
        powerup->effect_timer = seconds_to_millis(MEDIUM_POWERUP_EFFECT_TIME);
        add_frame(powerup->anim, load_resource_image("powerup-drill.bmp"));
    } else if (type == POWERUP_HYPER) {
        powerup->effect_timer = seconds_to_millis(SHORT_POWERUP_EFFECT_TIME);
        add_frame(powerup->anim, load_resource_image("powerup-hyper.bmp"));
    } else if (type == POWERUP_SCATTER) {
        powerup->effect_timer = seconds_to_millis(LONG_POWERUP_EFFECT_TIME);
        add_frame(powerup->anim, load_resource_image("powerup-scatter.bmp"));
    } else {
        fprintf(stderr, "WARNING: Unknown powerup type %d\n", type);
        destroy_powerup(powerup);
        return NULL;
    }
    
    return powerup;
}


POWERUP *remove_powerup(FIELD *field, POWERUP *powerup)
{
    int i = 0;
    
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (field->powerups[i] == powerup) {
            field->powerups[i] = NULL;
            return powerup;
        }
    }
    
    return powerup;
}


int check_border_collision(BODY *body, FIELD *field, DIRECTION dir)
{
    BOX *box = NULL;
    int newx = 0;
    int newy = 0;
    
    int collision = 0;
    
    box = &(body->box);
    newx = body->x + change_in_x(dir);
    newy = body->y + change_in_y(dir);
    
    if (north_edge(newy, box) < 0) {
        collision = 1;
    } else if (west_edge(newx, box) < 0) {
        collision = 1;
    } else if (south_edge(newy, box) > field_height(field)) {
        collision = 1;
    } else if (east_edge(newx, box) > field_width(field)) {
        collision = 1;
    }
    
    return collision;
}


void reverse_direction(BODY *body, DIRECTION dir)
{
    /* North, west, south, east */
    static int change_velx[4] = {1, -1, 1, -1};
    static int change_vely[4] = {-1, 1, -1, 1};
    
    /* Flip the velocity */
    body->velx *= change_velx[dir];
    body->vely *= change_vely[dir];
}


int move_powerup(POWERUP *powerup, FIELD *field, DIRECTION dir)
{
    int moved = 1;

    /* The position the powerup is trying to move to */
    int newx = powerup->body.x + change_in_x(dir);
    int newy = powerup->body.y + change_in_y(dir);

    /**
     * Check border collision
     */
    if (powerup->bounces < MAX_POWERUP_BOUNCES) {
        if (check_border_collision(&(powerup->body), field, dir)) {
            /* The powerup hit the border */
            reverse_direction(&(powerup->body), dir);
            moved = 0;
            powerup->bounces++;
        }
    }
    
    /**
     * If there was no collision, change the position
     */
    if (moved) {
        powerup->body.x = newx;
        powerup->body.y = newy;
    }
    
    return moved;
}


int out_of_bounds(FIELD *field, BODY *body)
{
    BOX *box = &(body->box);
    
    if (west_edge(body->x, box) > field_width(field) ||
        south_edge(body->y, box) > field_height(field) ||
        east_edge(body->x, box) < 0 ||
        north_edge(body->y, box) < 0
    ) {
        
        return 1;
    }
    
    return 0;
}


void update_powerup(POWERUP *powerup, FIELD *field)
{
    float newx = 0;
    float newy = 0;
    int keep_checking = 1;
    
    int stepx = 1;
    int stepy = 1;
    float dx = 0;
    float dy = 0;

    if (!powerup) {
        return;
    }
    
    animate(powerup->anim);
    
    newx = accelerate(powerup->body.x, powerup->body.velx);
    newy = accelerate(powerup->body.y, powerup->body.vely);
    
    /**
     * Find the difference between the current position
     * and the new position
     */
    dx = fabs(newx - powerup->body.x);
    dy = fabs(newy - powerup->body.y);
    
    if (dy != 0 && dx > dy) {
        stepx = (float)dx / dy;
    } else if (dx != 0) {
        stepy = (float) dy / dx;
    }
    
    while (keep_checking) {
        
        for (dx = 0; keep_checking && dx < stepx; dx++) {
            if ((int)powerup->body.x != (int)newx) {
                if ((int)powerup->body.x < (int)newx) {
                    keep_checking = move_powerup(powerup, field, EAST);
                } else if ((int)powerup->body.x > (int)newx) {
                    keep_checking = move_powerup(powerup, field, WEST);
                }
            }
        }
        
        if ((int) powerup->body.x == (int) newx && (int) powerup->body.y == (int) newy) {
            powerup->body.x = newx;
            powerup->body.y = newy;
            keep_checking = 0;
        }
        
        if (!keep_checking) {
            break;
        }

        for (dy = 0; keep_checking && dy < stepy; dy++) {
            if ((int)powerup->body.y != (int)newy) {
                if ((int)powerup->body.y < (int)newy) {
                    keep_checking = move_powerup(powerup, field, SOUTH);
                } else if ((int)powerup->body.y > (int)newy) {
                    keep_checking = move_powerup(powerup, field, NORTH);
                }
            }
        }
        
        if ((int) powerup->body.x == (int) newx && (int) powerup->body.y == (int) newy) {
            powerup->body.x = newx;
            powerup->body.y = newy;
            keep_checking = 0;
        }
    }
    
    /* If the powerup has left the screen then destroy it */
    if (out_of_bounds(field, &(powerup->body))) {
        destroy_powerup(remove_powerup(field, powerup));
    }
}


void draw_powerup(POWERUP *powerup)
{
    int x = 0;
    int y = 0;
    
    if (!powerup) {
        return;
    }
    
    x = powerup->body.x - (anim_width(powerup->anim) / 2);
    y = powerup->body.y - (anim_height(powerup->anim) / 2);

    draw_anim(powerup->anim, x, y, 0);
}


/**
 * Orientation is either H for horizontal or V for vertical.
 */
PADDLE *create_paddle(int x, int y, char orientation)
{
    PADDLE *paddle = NULL;
    
    paddle = alloc_memory("PADDLE", sizeof(PADDLE));

    paddle->body.x = x;
    paddle->body.y = y;
    paddle->orientation = orientation;

    paddle->anim = create_anim(0, 0);

    if (paddle->orientation == 'H') {
        add_frame(paddle->anim, load_resource_image("hpaddle.bmp"));
        paddle->shadow = load_resource_image("hpaddle-shadow.bmp");
    } else {
        add_frame(paddle->anim, load_resource_image("vpaddle.bmp"));
        paddle->shadow = load_resource_image("vpaddle-shadow.bmp");
    }
    
    if (paddle->orientation == 'H') {
        paddle->body.box.u = 3;
        paddle->body.box.l = 20;
        paddle->body.box.d = 3;
        paddle->body.box.r = 20;
    } else {
        paddle->body.box.u = 20;
        paddle->body.box.l = 3;
        paddle->body.box.d = 20;
        paddle->body.box.r = 3;
    }

    return paddle;
}


void destroy_paddle(PADDLE * paddle)
{
    if (paddle) {
        destroy_anim(paddle->anim);
    }

    free_memory("PADDLE", paddle);
}


void bound_in_field(BODY *body, FIELD *field)
{
    BOX *box = &(body->box);
    
    if (north_edge(body->y, box) < 0) {
        body->y = y_from_north_edge(0, box);
    } else if (west_edge(body->x, box) < 0) {
        body->x = x_from_west_edge(0, box);
    } else if (south_edge(body->y, box) > field_height(field)) {
        body->y = y_from_south_edge(field_height(field), box);
    } else if (east_edge(body->x, box) > field_width(field)) {
        body->x = x_from_east_edge(field_width(field), box);
    }
}


int is_collision(BODY *body1, BODY *body2)
{
    int n1 = north_edge(body1->y, &(body1->box));
    int w1 = west_edge(body1->x, &(body1->box));
    int s1 = south_edge(body1->y, &(body1->box));
    int e1 = east_edge(body1->x, &(body1->box));
    
    int n2 = north_edge(body2->y, &(body2->box));
    int w2 = west_edge(body2->x, &(body2->box));
    int s2 = south_edge(body2->y, &(body2->box));
    int e2 = east_edge(body2->x, &(body2->box));
    
    /**
     * If one is to the right of two or if one is below two or
     * if two is to the right of one or if two is below one...
     */
    if ((w1 > e2) || (n1 > s2) || (w2 > e1) || (n2 > s1)) {
        
        /* No collision */
        return 0;
    }
    
    /* Collision */
    return 1;
}


int box_collision(int x1, int y1, BOX *box, int x2, int y2)
{
    int n = 0;
    int w = 0;
    int s = 0;
    int e = 0;
    
    n = north_edge(y1, box);
    w = west_edge(x1, box);
    s = south_edge(y1, box);
    e = east_edge(x1, box);

    if (x2 > w && x2 < e && y2 > n && y2 < s) {
        /* Collision */
        return 1;
    }

    /* No collision */
    return 0;
}


void update_paddle_with_mouse(PADDLE *paddle, GAME *game, ALLEGRO_EVENT *event)
{
    /**
     * Use these offsets to make the paddle match up
     * with the actual mouse pointer.
     */
    int x = (CANVAS_W - field_width(game->field)) / 2;
    int y = (CANVAS_H - field_height(game->field)) / 2;
    
    /* Update mouse input */
    if (event->type == ALLEGRO_EVENT_MOUSE_AXES) {
        if (paddle->orientation == 'H') {
            paddle->body.x = (event->mouse.x * game->mousescale) - x;
        } else {
            paddle->body.y = (event->mouse.y * game->mousescale) - y;
        }
        
        /* Make sure the paddle stays inside the screen */
        bound_in_field(&(paddle->body), game->field);
    }
}


void update_paddle_with_keyboard(PADDLE *paddle, FIELD *field)
{
    static float paddle_vel = 250;

    float velx = 0;
    float vely = 0;
    
    if (is_key_held(ALLEGRO_KEY_RIGHT) && is_key_held(ALLEGRO_KEY_LEFT)) {
        velx = 0;
    } else if (is_key_held(ALLEGRO_KEY_RIGHT)) {
        velx = paddle_vel;
    } else if (is_key_held(ALLEGRO_KEY_LEFT)) {
        velx = -paddle_vel;
    }

    if (is_key_held(ALLEGRO_KEY_UP) && is_key_held(ALLEGRO_KEY_DOWN)) {
        vely = 0;
    } else if (is_key_held(ALLEGRO_KEY_UP)) {
        vely = -paddle_vel;
    } else if (is_key_held(ALLEGRO_KEY_DOWN)) {
        vely = paddle_vel;
    }

    if (paddle->orientation == 'H') {
        paddle->body.x = accelerate(paddle->body.x, velx);
    } else {
        paddle->body.y = accelerate(paddle->body.y, vely);
    }

    /* Make sure the paddle stays inside the screen */
    bound_in_field(&(paddle->body), field);
}


void draw_paddle(PADDLE *paddle)
{
    int x = 0;
    int y = 0;
    
    if (!paddle) {
        return;
    }
    
    x = paddle->body.x - (anim_width(paddle->anim) / 2);
    y = paddle->body.y - (anim_height(paddle->anim) / 2);

    draw_anim(paddle->anim, x, y, 0);
}


HOLE *create_hole(float x, float y)
{
    HOLE *hole = alloc_memory("HOLE", sizeof(HOLE));
    
    hole->normal_anim = create_anim(0, 0);
    add_frame(hole->normal_anim, load_resource_image("hole1.bmp"));
    
    hole->chomp_anim = create_anim(1, 8);
    add_frame(hole->chomp_anim, load_resource_image("hole2.bmp"));
    add_frame(hole->chomp_anim, load_resource_image("hole3.bmp"));
    add_frame(hole->chomp_anim, load_resource_image("hole1.bmp"));
    
    /* Set the default animation */
    hole->anim = hole->normal_anim;
    
    hole->body.x = x;
    hole->body.y = y;
    hole->body.velx = 0;
    hole->body.vely = 0;

    hole->body.box.u = 13;
    hole->body.box.l = 13;
    hole->body.box.d = 13;
    hole->body.box.r = 13;
    
    return hole;
}


void destroy_hole(HOLE *hole)
{
    if (hole) {
        destroy_anim(hole->normal_anim);
        destroy_anim(hole->chomp_anim);
    }
    
    free_memory("HOLE", hole);
}


int distance_between(BODY *body1, BODY *body2)
{
    int a = abs(body2->x - body1->x);
    int b = abs(body2->y - body1->y);

    /**
     * The hypotenuse is the square root of side one squared plus
     * side two squared.
     */
    return (int) sqrt((a * a) + (b * b));
}


int body_width(BODY *body)
{
    return body->box.l + body->box.r;
}


int body_height(BODY *body)
{
    return body->box.u + body->box.d;
}


void update_hole(HOLE *hole, FIELD *field)
{
    int i = 0;
    
    /**
     * The distance between a ball and a beetrap (hole)
     * before it starts chomping.
     */
    int dist = body_width(&(hole->body)) * 2;
    
    for (i = 0; i < MAX_BALLS; i++) {
        
        if (!field->balls[i]) {
            continue;
        }
        
        /* The ball is invincible in hyper mode */
        if (field->balls[i]->powerup_type == POWERUP_HYPER) {
            continue;
        }
        
        /* If the ball is close to the hole */
        if (distance_between(&(hole->body), &(field->balls[i]->body)) < dist) {
            
            /* Start chomping! */
            if (hole->anim != hole->chomp_anim) {
                hole->anim = hole->chomp_anim;
                reset_anim(hole->anim);
            }
        } else {
            hole->anim = hole->normal_anim;
        }
        
        /**
         * If hole and ball collision
         * then eat the ball!
         */
        
        if (is_collision(&(field->balls[i]->body), &(hole->body))) {
            field->balls[i]->dead = 1;
        }
    }
    
    animate(hole->anim);
}


void draw_hole(HOLE *hole)
{
    int x = 0;
    int y = 0;

    x = hole->body.x - (anim_width(hole->anim) / 2);
    y = hole->body.y - (anim_height(hole->anim) / 2);

    draw_anim(hole->anim, x, y, 0);
}


MAP *create_map(int width, int height)
{
    MAP *map = NULL;
    BLOCK *block = NULL;
    
    int x = 0;
    int y = 0;
    
    map = alloc_memory("MAP", sizeof(MAP));
    
    map->width = width;
    map->height = height;

    map->blocks = calloc_memory("GRID", width * height, sizeof(BLOCK));
    assert(map->blocks);
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            block = &(map->blocks[(y * map->width) + x]);
            block->bitmap = NULL;
            block->hits = 0;
        }
    }
    
    map->num_blocks = 0;
    
    return map;
}


void destroy_map(MAP * map)
{
    if (map) {
        free_memory("BLOCKS", map->blocks);
    }

    free_memory("MAP", map);
}


void set_block_bitmap(MAP * map, int x, int y, ALLEGRO_BITMAP *bitmap)
{
    if (x < 0 || y < 0 || x > map->width - 1 || y > map->height - 1) {
        return;
    }
    
    map->blocks[(y * map->width) + x].bitmap = bitmap;
}


void set_block_hits(MAP * map, int x, int y, int hits)
{
    if (x < 0 || y < 0 || x > map->width - 1 || y > map->height - 1) {
        return;
    }
    
    map->blocks[(y * map->width) + x].hits = hits;
}


int get_block_hits(MAP * map, int x, int y)
{
    if (x < 0 || y < 0 || x > map->width - 1 || y > map->height - 1) {
        return 0;
    }
    
    return map->blocks[(y * map->width) + x].hits;
}


void add_powerup(FIELD *field, POWERUP *powerup)
{
    int i = 0;
    
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (field->powerups[i] == NULL) {
            field->powerups[i] = powerup;
            return;
        }
    }
    
    fprintf(stderr, "WARNING: Failed to add powerup, too many powerups on screen.\n");
}


void drop_a_powerup(FIELD *field, int x, int y)
{
    int actualx = (x * BLOCK_SIZE) + (BLOCK_SIZE / 2);
    int actualy = (y * BLOCK_SIZE) + (BLOCK_SIZE / 2);
    
    POWERUP_TYPE type = random_number(POWERUP_NONE + 1, NUM_POWERUP_TYPES - 1);
    
    add_powerup(field, create_powerup(actualx, actualy, type));
}


int random_percent(int percent)
{
    return random_number(1, 100) <= percent;
}


void hit_block(FIELD *field, MAP *map, int x, int y, int destroy_touching)
{
    if (x < 0 || y < 0 || x > map->width - 1 || y > map->height - 1) {
        return;
    }
    
    map->blocks[(y * map->width) + x].hits--;
    
    /* Destroy the blocks that are touching this one */
    if (destroy_touching) {
        hit_block(field, map, x - 1, y, 0);
        hit_block(field, map, x + 1, y, 0);
        hit_block(field, map, x, y - 1, 0);
        hit_block(field, map, x, y + 1, 0);
    }
    
    /* Check to see if the block has any hits left */
    if (map->blocks[(y * map->width) + x].hits <= 0) {
        
        /* Clear the block */
        map->blocks[(y * map->width) + x].hits = 0;
        map->blocks[(y * map->width) + x].bitmap = NULL;
        map->num_blocks--;
        
        /**
         * When a block is destroyed, randomly decide if
         * you should create a power-up powerup.
         */
        if (random_percent(PERCENT_POWERUPS_APPEAR)) {
            drop_a_powerup(field, x, y);
        }
    }
}


BLOCK *get_block(MAP *map, int x, int y)
{
    if (x < 0 || y < 0 || x > map->width - 1 || y > map->height - 1) {
        return NULL;
    }
    
    return &(map->blocks[(y * map->width) + x]);
}



void draw_map(MAP * map)
{
    ALLEGRO_BITMAP *bitmap = NULL;
    int hits = 0;
    int x = 0;
    int y = 0;
    
    for (y = 0; y < map->height; y++) {
        for (x = 0; x < map->width; x++) {
            bitmap = map->blocks[(y * map->width) + x].bitmap;
            hits = map->blocks[(y * map->width) + x].hits;
            if (bitmap != NULL && hits > 0) {
                al_draw_bitmap(bitmap, x * BLOCK_SIZE, y * BLOCK_SIZE, 0);
            }
        }
    }
}


BALL *create_ball(float x, float y, float angle)
{
    BALL *ball = NULL;

    ball = alloc_memory("BALL", sizeof(BALL));
    
    ball->body.x = x;
    ball->body.y = y;
    ball->body.velx = velx_from_angle(angle, BALL_SPEED);
    ball->body.vely = vely_from_angle(angle, BALL_SPEED);
    
    ball->body.box.u = 6;
    ball->body.box.l = 6;
    ball->body.box.d = 6;
    ball->body.box.r = 6;
    
    ball->speed = BALL_SPEED;
    
    ball->anim = create_anim(1, 15);
    add_frame(ball->anim, load_resource_image("bee1.bmp"));
    add_frame(ball->anim, load_resource_image("bee2.bmp"));
    
    ball->shadow = load_resource_image("bee-shadow.bmp");
    
    ball->facing = 0;
    ball->paddlehit = 0;
    
    ball->dead = 0;
    
    ball->powerup_timer = 0;
    ball->powerup_type = POWERUP_NONE;

    return ball;
}


void destroy_ball(BALL * ball)
{
    if (ball) {
        destroy_anim(ball->anim);
    }
    
    free_memory("BALL", ball);
}


float random_direction()
{
    static float flags[4] = {
        0, /* 0 degrees */
        ALLEGRO_PI / 2, /* 90 degrees */
        ALLEGRO_PI, /* 180 degrees */
        (ALLEGRO_PI / 2) * 3 /* 270 degrees */
    };

    return flags[random_number(0, 3)];
}


int to_map_position(int position)
{
    return position / BLOCK_SIZE;
}


void bounce_off_paddle(BALL *ball, PADDLE *paddle)
{
    /**
     * Get the distance between the center of the paddle and ball.
     * Get the ratio by dividing the speed of the ball by the distance.
     * Multiply the x and y deltas by the ratio to get...
     * DA NEW VELOCITIES!!!
     * So simple.
     */
    
    float deltax = 0;
    float deltay = 0;
    float vel = 0;
    float ratio = 1;
    
    deltax = ball->body.x - paddle->body.x;
    deltay = ball->body.y - paddle->body.y;
    
    /**
     * Prevent the ball from getting stuck
     * horizontally or vertically.
     */
    
    if (deltay == 0) {
        deltay = -0.1;
    }
    
    if (deltax == 0) {
        deltax = 0.1;
    }
    
    /*vel = (float) hypot(deltax, deltay);*/
    vel = (float) sqrt((deltax * deltax) + (deltay * deltay));
    
    if (vel != 0) {
        ratio = ball->speed / vel;
    } else {
        ratio = 0;
    }
    
    ball->body.velx = deltax * ratio;
    ball->body.vely = deltay * ratio;
}


void play_block_hit_sound()
{
    static ALLEGRO_SAMPLE *sound = NULL;
    
    if (sound == NULL) {
        sound = al_load_sample("sounds/block.wav");
    }
    
    al_play_sample(sound, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
}


void play_paddle_hit_sound()
{
    static ALLEGRO_SAMPLE *sound = NULL;
    
    if (sound == NULL) {
        sound = al_load_sample("sounds/paddle.wav");
    }
    
    al_play_sample(sound, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
}


void play_powerup_collected_sound()
{
    static ALLEGRO_SAMPLE *sound = NULL;
    
    if (sound == NULL) {
        sound = al_load_sample("sounds/powerup.wav");
    }
    
    al_play_sample(sound, 1.0, 0.0, 1.0, ALLEGRO_PLAYMODE_ONCE, NULL);
}


void random_ball_direction(BALL *ball)
{
    int angle = random_number(0, 359);
    
    ball->body.velx = velx_from_angle(angle, ball->speed);
    ball->body.vely = vely_from_angle(angle, ball->speed);
    
    /**
     * Change the direction the ball is facing.
     * It's cute.
     */
    ball->facing = random_direction();
}


void change_ball_facing(BALL *ball)
{
    /**
     * Change the direction the ball is facing.
     * It's cute.
     */
    ball->facing = random_direction();
}


int check_ball_and_block_collision(BALL *ball, FIELD *field, DIRECTION dir)
{
    BOX *box = NULL;
    int newx = 0;
    int newy = 0;
    
    MAP *map = NULL;
    int map_north = 0;
    int map_west = 0;
    int map_south = 0;
    int map_east = 0;
    
    BLOCK *a_hit_block = NULL;
    int collision = 0;
    int destroy_touching = 0;

    box = &(ball->body.box);
    newx = ball->body.x + change_in_x(dir);
    newy = ball->body.y + change_in_y(dir);
    
    map = field->map;
    map_north = to_map_position(north_edge(newy, box));
    map_west = to_map_position(west_edge(newx, box));
    map_south = to_map_position(south_edge(newy, box));
    map_east = to_map_position(east_edge(newx, box));
    
    if (ball->powerup_type == POWERUP_BLAST) {
        destroy_touching = 1;
    }
    
    /* Upper left corner */
    if (get_block_hits(map, map_west, map_north) > 0) {
        hit_block(field, map, map_west, map_north, destroy_touching);
        collision = 1;
        a_hit_block = get_block(map, map_west, map_north);
        play_block_hit_sound();
    }

    /* Upper right corner */
    if (get_block_hits(map, map_east, map_north) > 0) {
        if (get_block(map, map_east, map_north) != a_hit_block) {
            hit_block(field, map, map_east, map_north, destroy_touching);
            collision = 1;
            a_hit_block = get_block(map, map_east, map_north);
            play_block_hit_sound();
        }
    }

    /* Lower left corner */
    if (get_block_hits(map, map_west, map_south) > 0) {
        if (get_block(map, map_west, map_south) != a_hit_block) {
            hit_block(field, map, map_west, map_south, destroy_touching);
            collision = 1;
            a_hit_block = get_block(map, map_west, map_south);
            play_block_hit_sound();
        }
    }

    /* Lower right corner */
    if (get_block_hits(map, map_east, map_south) > 0) {
        if (get_block(map, map_east, map_south) != a_hit_block) {
            hit_block(field, map, map_east, map_south, destroy_touching);
            collision = 1;
            play_block_hit_sound();
        }
    }
    
    if (collision) {
        if (ball->powerup_type == POWERUP_SCATTER) {
            /* Bounce in a random angle */
            random_ball_direction(ball);
        } else if (ball->powerup_type != POWERUP_DRILL) {
            reverse_direction(&(ball->body), dir);
            change_ball_facing(ball);
        }
    }
    
    return collision;
}


int check_ball_and_paddle_collision(BALL *ball, FIELD *field, DIRECTION dir)
{
    int oldx = ball->body.x;
    int oldy = ball->body.y;
    int hit = 0;
    int i = 0;
    
    ball->body.x += change_in_x(dir);
    ball->body.y += change_in_y(dir);
    
    for (i = 0; i < field->num_paddles; i++) {

        /* Check if the ball and paddle collide */
        if (is_collision(&(ball->body), &(field->paddles[i]->body))) {
            
            /* Collision! */
            
            /**
             * If a ball has been hit by a paddle, don't let it get hit by
             * a paddle again until it hasn't touched one for an entire move.
             * This'll keep a ball from getting "stuck" inside a paddle.
             */
            
            hit = 1;
            
            if (!ball->paddlehit) {
                play_paddle_hit_sound();
                bounce_off_paddle(ball, field->paddles[i]);
                ball->facing = random_direction();
                
                ball->paddlehit = 1;
                
                return 1;
            }
        }
    }
    
    ball->paddlehit = hit;
    
    ball->body.x = oldx;
    ball->body.y = oldy;
    
    return 0;
}


void change_ball_speed(BALL *ball, int speed)
{
    /* Scale the velocities */
    float scale = (float)speed / ball->speed;
    
    ball->body.velx *= scale;
    ball->body.vely *= scale;
    
    ball->speed = speed;
}


void apply_powerup_to_ball(BALL *ball, POWERUP *powerup)
{
    ball->powerup_timer = powerup->effect_timer;
    ball->powerup_type = powerup->type;
    
    if (ball->powerup_type == POWERUP_HYPER) {
        /* Hyper mode speed! */
        change_ball_speed(ball, BALL_HYPER_SPEED);
    }

}


void check_paddle_and_powerup_collision(PADDLE *paddle, FIELD *field)
{
    POWERUP *powerup = NULL;
    
    int i = 0;
    int j = 0;
    
    for (i = 0; i < MAX_POWERUPS; i++) {
        
        powerup = field->powerups[i];
        
        if (powerup != NULL) {
            
            /* Check if the paddle is colliding with the powerup */
            if (is_collision(&(paddle->body), &(powerup->body))) {
                
                /**
                 * Got the powerup!
                 * Apply it to all of the balls.
                 */
                for (j = 0; j < MAX_BALLS; j++) {
                    if (field->balls[j]) {
                        apply_powerup_to_ball(field->balls[j], powerup);
                        play_powerup_collected_sound();
                    }
                }
                
                destroy_powerup(remove_powerup(field, powerup));
            }
        }
    }
}


/**
 * Return false if the ball collides with something and doesn't move.
 */
int move_ball(BALL * ball, FIELD * field, DIRECTION dir)
{
    int moved = 1;

    /* The position the ball is trying to move to */
    int newx = ball->body.x + change_in_x(dir);
    int newy = ball->body.y + change_in_y(dir);

    /**
     * Check border collision
     */
    if (check_border_collision(&(ball->body), field, dir)) {
        /* The ball hit the border */
        play_paddle_hit_sound();
        reverse_direction(&(ball->body), dir);
        change_ball_facing(ball);
        moved = 0;
    }
    
    /**
     * Check block collision
     */
    if (check_ball_and_block_collision(ball, field, dir)) {
        /* The ball hit a block or two */
        moved = 0;
    }
    
    /**
     * Check powerup collision
     */
    
    /**
     * POWERUPS ARE NOW COLLECTED BY THE PADDLES
     */
    
    /* You can't get another powerup if you already have a powerup */
    /*
    if (ball->powerup_type == POWERUP_NONE) {
        check_ball_and_powerup_collision(ball, field, dir);
    }
    */
    
    /**
     * Check paddle collision
     */
    if (check_ball_and_paddle_collision(ball, field, dir)) {
        /* The ball hit a paddle */
        moved = 0;
    }
    
    /**
     * If there was no collision, change the position
     */
    if (moved) {
        ball->body.x = newx;
        ball->body.y = newy;
    }
    
    return moved;
}


void update_ball(BALL * ball, FIELD * field)
{
    float newx = 0;
    float newy = 0;
    int keep_checking = 1;
    
    int stepx = 1;
    int stepy = 1;
    float dx = 0;
    float dy = 0;

    if (!ball) {
        return;
    }
    
    animate(ball->anim);
    
    if (ball->dead) {
        return;
    }
    
    /* If the ball has a powerup, decrease its timer */
    if (ball->powerup_type != POWERUP_NONE) {
        
        /* Update the powerup timer */
        ball->powerup_timer -= MILLIS_PER_SECOND / FPS;
        
        /* If time is up, reset the ball powerup */
        if (ball->powerup_timer <= 0) {
            if (ball->powerup_type == POWERUP_HYPER) {
                /* Reset to the normal ball speed */
                change_ball_speed(ball, BALL_SPEED);
            }
            ball->powerup_type = POWERUP_NONE;
        }
    }
    
    newx = accelerate(ball->body.x, ball->body.velx);
    newy = accelerate(ball->body.y, ball->body.vely);
    
    /**
     * Find the difference between the current position
     * and the new position
     */
    dx = fabs(newx - ball->body.x);
    dy = fabs(newy - ball->body.y);
    
    if (dy != 0 && dx > dy) {
        stepx = (float)dx / dy;
    } else if (dx != 0) {
        stepy = (float) dy / dx;
    }
    
    /**
     * TODO: Continue moving even if there was a collision.
     * The movement shouldn't stop, it just changes to a new direction.
     */
    while (keep_checking) {
        
        for (dx = 0; keep_checking && dx < stepx; dx++) {
            if ((int)ball->body.x != (int)newx) {
                if ((int)ball->body.x < (int)newx) {
                    keep_checking = move_ball(ball, field, EAST);
                } else if ((int)ball->body.x > (int)newx) {
                    keep_checking = move_ball(ball, field, WEST);
                }
            }
        }
        
        if ((int) ball->body.x == (int) newx && (int) ball->body.y == (int) newy) {
            ball->body.x = newx;
            ball->body.y = newy;
            keep_checking = 0;
        }
        
        if (!keep_checking) {
            break;
        }

        for (dy = 0; keep_checking && dy < stepy; dy++) {
            if ((int)ball->body.y != (int)newy) {
                if ((int)ball->body.y < (int)newy) {
                    keep_checking = move_ball(ball, field, SOUTH);
                } else if ((int)ball->body.y > (int)newy) {
                    keep_checking = move_ball(ball, field, NORTH);
                }
            }
        }
        
        if ((int) ball->body.x == (int) newx && (int) ball->body.y == (int) newy) {
            ball->body.x = newx;
            ball->body.y = newy;
            keep_checking = 0;
        }
    }
}


void draw_ball(BALL * ball)
{
    int xhalf = 0;
    int yhalf = 0;
    int x = 0;
    int y = 0;
    ALLEGRO_BITMAP *frame = NULL;

    if (!ball) {
        return;
    }
    
    xhalf = anim_width(ball->anim) / 2;
    yhalf = anim_height(ball->anim) / 2;
    x = ball->body.x;
    y = ball->body.y;
    frame = current_frame(ball->anim);

    /**
     * The first set of points will be drawn onto the target
     * bitmap at the second set of points.
     */
    al_draw_rotated_bitmap(frame, xhalf, yhalf, x, y, ball->facing, 0);
}


void draw_background()
{
    ALLEGRO_BITMAP *background = NULL;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;

    background = load_resource_image("background.bmp");
    width = al_get_bitmap_width(background);
    height = al_get_bitmap_height(background);

    for (y = 0; y <= CANVAS_H; y += height) {
        for (x = 0; x <= CANVAS_W; x += width) {
            al_draw_bitmap(background, x, y, 0);
        }
    }
}


void draw_border(FIELD *field)
{
    ALLEGRO_BITMAP *bn = NULL;
    ALLEGRO_BITMAP *bs = NULL;
    ALLEGRO_BITMAP *bw = NULL;
    ALLEGRO_BITMAP *be = NULL;
    int width = 0;
    int height = 0;
    int i = 0;
    
    bn = load_resource_image("border-north.bmp");
    bs = load_resource_image("border-south.bmp");
    bw = load_resource_image("border-west.bmp");
    be = load_resource_image("border-east.bmp");

    width = field->map->width * BLOCK_SIZE;
    height = field->map->height * BLOCK_SIZE;

    /* Draw the north border */
    for (i = 0; i <= width; i += al_get_bitmap_width(bn)) {
        al_draw_bitmap(bn, i, 0, 0);
    }

    /* Draw the east border */
    for (i = 0; i <= height; i += al_get_bitmap_height(be)) {
        al_draw_bitmap(be, width - al_get_bitmap_width(be), i, 0);
    }

    /* Draw the south border */
    for (i = 0; i <= width; i += al_get_bitmap_width(bs)) {
        al_draw_bitmap(bs, i, height - al_get_bitmap_height(bs), 0);
    }

    /* Draw the west border */
    for (i = 0; i <= height; i += al_get_bitmap_height(bw)) {
        al_draw_bitmap(bw, 0, i, 0);
    }
}


FIELD *create_field()
{
    FIELD *field = NULL;
    int i = 0;
    
    field = alloc_memory("FIELD", sizeof(FIELD));
    
    field->map = NULL;

    for (i = 0; i < MAX_PADDLES; i++) {
        field->paddles[i] = NULL;
    }

    field->num_paddles = 0;

    for (i = 0; i < MAX_BALLS; i++) {
        field->balls[i] = NULL;
    }

    for (i = 0; i < MAX_HOLES; i++) {
        field->holes[i] = NULL;
    }

    field->num_holes = 0;
    
    for (i = 0; i < MAX_POWERUPS; i++) {
        field->powerups[i] = NULL;
    }
    
    field->events = al_create_event_queue();
    al_register_event_source(field->events, al_get_mouse_event_source());
    
    field->default_ball_x = -1;
    field->default_ball_y = -1;
    field->default_ball_velx = -1;
    field->default_ball_vely = -1;

    return field;
}


void destroy_field(FIELD * field)
{
    int i = 0;

    if (!field) {
        return;
    }

    destroy_map(field->map);

    for (i = 0; i < MAX_BALLS; i++) {
        destroy_ball(field->balls[i]);
    }

    for (i = 0; i < field->num_paddles; i++) {
        destroy_paddle(field->paddles[i]);
    }

    for (i = 0; i < field->num_holes; i++) {
        destroy_hole(field->holes[i]);
    }

    for (i = 0; i < MAX_POWERUPS; i++) {
        destroy_powerup(field->powerups[i]);
    }

    al_destroy_event_queue(field->events);

    free_memory("FIELD", field);
}


void add_ball(FIELD * field, BALL * ball)
{
    int i = 0;
    
    if (!ball) {
        return;
    }
    
    if (field->default_ball_x < 0) {
        /* No previous default ball data has been stored */
        
        /**
         * Store the position and angle of this ball, to use it
         * to initialize new balls in this field.
         */
        field->default_ball_x = ball->body.x;
        field->default_ball_y = ball->body.y;
        field->default_ball_velx = ball->body.velx;
        field->default_ball_vely = ball->body.vely;
    }
        
    for (i = 0; i < MAX_BALLS; i++) {
        if (field->balls[i] == NULL) {
            field->balls[i] = ball;
            return;
        }
    }
    
    fprintf(stderr, "WARNING: Failed to add ball, too many balls on screen.\n");
}


BALL *remove_ball(FIELD *field, BALL *ball)
{
    int i = 0;
    
    for (i = 0; i < MAX_BALLS; i++) {
        if (field->balls[i] == ball) {
            field->balls[i] = NULL;
            return ball;
        }
    }
    
    return ball;
}


void update_field(FIELD * field, GAME *game)
{
    BALL *ball;
    ALLEGRO_EVENT event;
    int i = 0;
    
    /* Update paddle movement with player input */
    while (al_get_next_event(field->events, &event)) {
        for (i = 0; i < field->num_paddles; i++) {
            update_paddle_with_mouse(field->paddles[i], game, &event);
        }
    }

    for (i = 0; i < field->num_paddles; i++) {
        update_paddle_with_keyboard(field->paddles[i], field);
    }
    
    /* Update the powerups */
    for (i = 0; i < MAX_POWERUPS; i++) {
        update_powerup(field->powerups[i], field);
    }

    /* Collect powerups */
    for (i = 0; i < field->num_paddles; i++) {
        check_paddle_and_powerup_collision(field->paddles[i], field);
    }
    
    /* Move the balls */
    for (i = 0; i < MAX_BALLS; i++) {
        update_ball(field->balls[i], field);
        
        /* Remove dead balls */
        if (field->balls[i] && field->balls[i]->dead) {
            destroy_ball(remove_ball(field, field->balls[i]));
            
            game->player->lives--;
            
            /* If the player has any more tries left, create a new ball */
            if (game->player->lives > 0) {
                ball = create_ball(field->default_ball_x, field->default_ball_y, 0);
                ball->body.velx = field->default_ball_velx;
                ball->body.vely = field->default_ball_vely;
                add_ball(field, ball);
            }
        }
    }

    /* Update the mean old holes */
    for (i = 0; i < field->num_holes; i++) {
        update_hole(field->holes[i], field);
    }
}


void draw_field(FIELD * field)
{
    PADDLE *paddle;
    BALL *ball;
    int x = 0;
    int y = 0;
    int i = 0;
    
    static int shadow_increase = 1;
    static float shadow_offsetx = 4;
    static float shadow_offsety = 4;

    /* Redraw the background */
    draw_background();

    /* Draw the shadows */
    for (i = 0; i < field->num_paddles; i++) {
        paddle = field->paddles[i];
        x = paddle->body.x - (al_get_bitmap_width(paddle->shadow) / 2);
        y = paddle->body.y - (al_get_bitmap_height(paddle->shadow) / 2);
        al_draw_bitmap(paddle->shadow, x - 4, y + 4, 0);
    }
    
    /* Make the ball shadow "bounce" */
    if (shadow_increase) {
        shadow_offsetx += 0.4;
        shadow_offsety += 0.4;
    } else {
        shadow_offsetx -= 0.4;
        shadow_offsety -= 0.4;
    }
    if (shadow_offsetx >= 16 || shadow_offsetx <= 4) {
        shadow_increase = shadow_increase ? 0 : 1;
    }
    
    for (i = 0; i < MAX_BALLS; i++) {
        ball = field->balls[i];
        if (ball) {
            x = ball->body.x - (al_get_bitmap_width(ball->shadow) / 2);
            y = ball->body.y - (al_get_bitmap_height(ball->shadow) / 2);
            al_draw_bitmap(ball->shadow, x - (int)shadow_offsetx, y + (int)shadow_offsety, 0);
        }
    }
    
    /* Draw the holes */
    for (i = 0; i < field->num_holes; i++) {
        draw_hole(field->holes[i]);
    }

    /* Draw the demo map */
    draw_map(field->map);

    /* Draw the paddles */
    for (i = 0; i < field->num_paddles; i++) {
        draw_paddle(field->paddles[i]);
    }

    /* Draw the powerups */
    for (i = 0; i < MAX_POWERUPS; i++) {
        draw_powerup(field->powerups[i]);
    }

    /* Draw the balls */
    for (i = 0; i < MAX_BALLS; i++) {
        draw_ball(field->balls[i]);
    }

    /* Draw the border */
    draw_border(field);
}


void add_paddle(FIELD * field, PADDLE * paddle)
{
    if (field->num_paddles >= MAX_PADDLES) {
        fprintf(stderr, "Failed to add paddle.\n");
        return;
    }

    field->paddles[field->num_paddles] = paddle;
    field->num_paddles++;
}


void add_hole(FIELD * field, HOLE *hole)
{
    if (field->num_holes >= MAX_HOLES - 1) {
        fprintf(stderr, "Failed to add hole.\n");
        return;
    }

    field->holes[field->num_holes] = hole;
    field->num_holes++;
}


void set_map(FIELD *field, MAP *map)
{
    destroy_map(field->map);
    
    field->map = map;
}


PLAYER *create_player()
{
    PLAYER *player = NULL;

    player = alloc_memory("PLAYER", sizeof(PLAYER));
    
    player->lives = 5;
    
    return player;
}


void destroy_player(PLAYER *player)
{
    free_memory("PLAYER", player);
}


GAME *create_game()
{
    GAME *game = NULL;

    game = alloc_memory("GAME", sizeof(GAME));
    
    game->field = NULL;
    game->player = NULL;
    game->mousescale = 1;
    
    return game;
}


void destroy_game(GAME *game)
{
    if (game) {
        destroy_field(game->field);
        destroy_player(game->player);
    }
    
    free_memory("GAME", game);
}


void draw_wallpaper()
{
    ALLEGRO_BITMAP *bitmap = NULL;
    int x = 0;
    int y = 0;
    
    bitmap = load_resource_image("wallpaper.bmp");
    
    for (y = 0; y < CANVAS_H; y += al_get_bitmap_height(bitmap)) {
        for (x = 0; x < CANVAS_W; x += al_get_bitmap_width(bitmap)) {
            al_draw_bitmap(bitmap, x, y, 0);
        }
    }
}


int update_game(void *data)
{
    GAME *game = (GAME *)data;
    
    update_field(game->field, game);
    
    /* Press escape to quit */
    if (is_key_pressed(ALLEGRO_KEY_ESCAPE)) {
        return 0;
    }
    
    return 1;
}


void draw_game(void *data)
{
    static ALLEGRO_BITMAP *canvas = NULL;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    
    GAME *game = (GAME *)data;
    
    draw_wallpaper();
    
    /* Draw the field centered on the canvas */
    
    w = field_width(game->field);
    h = field_height(game->field);
    x = (CANVAS_W - w) / 2;
    y = (CANVAS_H - h) / 2;

    if (canvas == NULL || (al_get_bitmap_width(canvas) != w && al_get_bitmap_height(canvas) != h)) {
        al_destroy_bitmap(canvas);
        canvas = al_create_bitmap(w, h);
    }
    
    al_set_target_bitmap(canvas);
    draw_field(game->field);
    
    /* Put the display back to normal */
    al_set_target_bitmap(al_get_backbuffer(display));
    
    al_draw_bitmap(canvas, x, y, 0);
}


float cap_angle(float angle)
{
    while (angle < 0) {
        angle += 360;
    }
    
    while (angle >= 360) {
        angle -= 360;
    }
    
    return angle;
}


#define CHAR_FORMAT_SKIP_WHITESPACE "%*[ \n\t]%c"


PADDLE *load_paddle(FILE *file)
{
    int x = 0;
    int y = 0;
    char orientation = 0;
    
    /* X position */
    if (fscanf(file, "%d", &x) != 1) {
        fprintf(stderr, "Failed to load paddle x position.\n");
        return NULL;
    }
    
    /* Y position */
    if (fscanf(file, "%d", &y) != 1) {
        fprintf(stderr, "Failed to load paddle y position.\n");
        return NULL;
    }
    
    /* Orientation */
    if (fscanf(file, CHAR_FORMAT_SKIP_WHITESPACE, &orientation) != 1) {
        fprintf(stderr, "Failed to load paddle orientation.\n");
        return NULL;
    }
    
    if (orientation != 'H' && orientation != 'V') {
        fprintf(stderr, "Failed to read invalid paddle orientation value.\n");
        return NULL;
    }
    
    return create_paddle(x, y, orientation);
}


HOLE *load_hole(FILE *file)
{
    int x = 0;
    int y = 0;
    
    /* X position */
    if (fscanf(file, "%d", &x) != 1) {
        fprintf(stderr, "Failed to load hole x position.\n");
        return NULL;
    }
    
    /* Y position */
    if (fscanf(file, "%d", &y) != 1) {
        fprintf(stderr, "Failed to load hole y position.\n");
        return NULL;
    }
    
    return create_hole(x, y);
}


BALL *load_ball(FILE *file)
{
    int x = 0;
    int y = 0;
    int angle = 0;
    
    /* X position */
    if (fscanf(file, "%d", &x) != 1) {
        fprintf(stderr, "Failed to load ball x position.\n");
        return NULL;
    }
    
    /* Y position */
    if (fscanf(file, "%d", &y) != 1) {
        fprintf(stderr, "Failed to load ball y position.\n");
        return NULL;
    }
    
    /* Angle */
    if (fscanf(file, "%d", &angle) != 1) {
        fprintf(stderr, "Failed to load ball angle.\n");
        return NULL;
    }
    
    return create_ball(x, y, cap_angle(angle));
}


typedef struct BLOCK_ID {
    char type;
    char bitmap_filename[STRING_LENGTH];
} BLOCK_ID;


#define MAX_BLOCK_IDS 24


MAP *load_map(FILE *file, BLOCK_ID *block_ids)
{
    MAP *map = NULL;
    
    int width = 0;
    int height = 0;
    
    char type = 0;
    int hits = 0;
    ALLEGRO_BITMAP *bitmap = NULL;
    
    int x = 0;
    int y = 0;
    int i = 0;
    
    /* Width */
    if (fscanf(file, "%d", &width) != 1) {
        fprintf(stderr, "Failed to load map width.\n");
        return NULL;
    }
    
    /* Y position */
    if (fscanf(file, "%d", &height) != 1) {
        fprintf(stderr, "Failed to load map height.\n");
        return NULL;
    }
    
    map = create_map(width, height);
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            
            /* Block type */
            if (fscanf(file, CHAR_FORMAT_SKIP_WHITESPACE, &type) != 1) {
                fprintf(stderr, "Failed to load block type.\n");
                destroy_map(map);
                return NULL;
            }
            
            /* Block hit points */
            if (fscanf(file, "%d", &hits) != 1) {
                fprintf(stderr, "Failed to load block hits.\n");
                destroy_map(map);
                return NULL;
            }
            
            if (type == '0') {
                /* No block, empty space */
                continue;
            }
            
            /* Load the bitmap from the filename */
            for (bitmap = NULL, i = 0; i < MAX_BLOCK_IDS; i++) {
                if (block_ids[i].type == type) {
                    bitmap = load_resource_image(block_ids[i].bitmap_filename);
                }
            }
            
            set_block_bitmap(map, x, y, bitmap);
            set_block_hits(map, x, y, hits);
        }
    }
    
    return map;
}


FIELD *load_field(FILE *file)
{
    FIELD *field = NULL;
    char line[STRING_LENGTH];
    
    char block_id = 0;
    
    BLOCK_ID block_ids[MAX_BLOCK_IDS];
    int num_block_ids = 0;
    
    int i = 0;
    
    field = create_field();

    for (i = 0; i < MAX_BLOCK_IDS; i++) {
        block_ids[i].type = 0;
        strcpy(block_ids[i].bitmap_filename, "");
    }
    
    /* Scan the rest of the file */
    while (fscanf(file, "%s", line) == 1) {
        
        if (strcmp(line, "BALL") == 0) {
            add_ball(field, load_ball(file));
        } else if (strcmp(line, "HOLE") == 0) {
            add_hole(field, load_hole(file));
        } else if (strcmp(line, "PADDLE") == 0) {
            add_paddle(field, load_paddle(file));
        } else if (strcmp(line, "BLOCK") == 0) {
            
            /**
             * Load block
             */
            
            /* Block ID type */
            if (fscanf(file, CHAR_FORMAT_SKIP_WHITESPACE, &block_id) != 1) {
                fprintf(stderr, "Failed to load block ID type.\n");
                destroy_field(field);
                return NULL;
            }
            
            /* Skip whitespace */
            fscanf(file, "%*[ \n\t]");
            
            /* Block ID bitmap filename */
            if (fgets(line, STRING_LENGTH, file) == NULL) {
                fprintf(stderr, "Failed to load block ID filename.\n");
                destroy_field(field);
                return NULL;
            }
            
            /* Get rid of newline */
            line[strlen(line) - 1] = '\0';
    
            block_ids[num_block_ids].type = block_id;
            strcpy(block_ids[num_block_ids].bitmap_filename, line);
            num_block_ids++;
            
        } else if (strcmp(line, "MAP") == 0) {
            set_map(field, load_map(file, block_ids));
        } else {
            fprintf(stderr, "Failed to load field powerup.\n");
            destroy_field(field);
            return NULL;
        }
    }
    
    return field;
}


void get_desktop_resolution(int adapter, int *w, int *h)
{
    /*
       ALLEGRO_MONITOR_INFO info;

       al_get_monitor_info(adapter, &info);

       *w = info.x2 - info.x1;
       *h = info.y2 - info.y1;
     */
    *w = 1680;
    *h = 1050;
}


ALLEGRO_TIMER *timer = NULL;


void run(int (*update)(void *data), void (*draw)(void *data), void *data)
{
    int keep_running = 1;
    int redraw = 1;
    
    ALLEGRO_EVENT_QUEUE *events = al_create_event_queue();
    ALLEGRO_EVENT event;
    
    al_register_event_source(events, al_get_timer_event_source(timer));
    
    while (keep_running) {
        al_wait_for_event(events, &event);

        if (event.type == ALLEGRO_EVENT_TIMER) {
            
            /* Update */
            keep_running = update(data);
            
            redraw = 1;
        }

        if (redraw && al_is_event_queue_empty(events)) {
            
            redraw = 0;
            
            /* Draw */
            draw(data);
            
            /* Update the screen */
            al_flip_display();
        }
    }
}


int update_title_screen(void *data)
{
    GAME *game = NULL;
    FILE *file = NULL;

    if (is_key_pressed(ALLEGRO_KEY_ENTER) || is_key_pressed(ALLEGRO_KEY_SPACE)) {
        
        /**
         * Initialize new game data to play
         */
        
        game = create_game();
        game->player = create_player();
        
        /* Load a field from a file */
        file = fopen("data/level01.dat", "r");
        game->field = load_field(file);
        fclose(file);
        
        game->mousescale = 1; /* / (float)scale;*/
        
        run(update_game, draw_game, game);
        
        /* Done playing, destroy the game */
        destroy_game(game);
        game = NULL;
    }
    
    /* Press escape to quit */
    if (is_key_pressed(ALLEGRO_KEY_ESCAPE)) {
        return 0;
    }
    
    return 1;
}


ALLEGRO_BITMAP *random_block_image()
{
    int num = random_number(BLOCK_EMPTY + 1, NUM_BLOCK_TYPES - 1);
    
    if (num == BLOCK_DAISY) {
        return load_resource_image("block-daisy.bmp");
    }
    if (num == BLOCK_FERN) {
        return load_resource_image("block-fern.bmp");
    }
    if (num == BLOCK_ROSE) {
        return load_resource_image("block-rose.bmp");
    }
    
    return NULL;
}


void draw_title_screen(void *data)
{
    static ALLEGRO_BITMAP *title = NULL;
    static ALLEGRO_BITMAP *background = NULL;

    int x = 0;
    int y = 0;

    if (!title) {
        title = load_resource_image("title.bmp");
    }
    
    if (!background) {
        background = al_create_bitmap(CANVAS_W, CANVAS_H);
        al_set_target_bitmap(background);
        for (y = 0; y < CANVAS_H; y += BLOCK_SIZE) {
            for (x = 0; x < CANVAS_W; x += BLOCK_SIZE) {
                al_draw_bitmap(random_block_image(), x, y, 0);
            }
        }
        al_set_target_bitmap(al_get_backbuffer(display));
    }
    
    al_draw_bitmap(background, 0, 0, 0);
    
    /* Draw the title screen centered on the canvas */
    x = (CANVAS_W - al_get_bitmap_width(title)) / 2;
    y = (CANVAS_H - al_get_bitmap_height(title)) / 2;
    al_draw_bitmap(title, x, y, 0);
}


int main(int argc, char **argv)
{
    ALLEGRO_EVENT_QUEUE *events = NULL;

    /* For screen scaling */
    ALLEGRO_TRANSFORM trans;
    float scale_x = 1;
    float scale_y = 1;
    int scale = 1;
    int screen_w = 0;
    int screen_h = 0;

    int monitor_w = CANVAS_W;
    int monitor_h = CANVAS_H;
    
    int status = 0;

    if (!al_init() || !al_init_image_addon() || !al_install_keyboard()
        || !al_install_mouse()
        /*|| !al_install_audio() || !al_init_acodec_addon() || !al_reserve_samples(4)*/
	) {
        fprintf(stderr, "Failed to initialize allegro.\n");
        goto catch;
    }
    
    /*show_memory_label();*/
    
    timer = al_create_timer(1.0 / FPS);
    
    if (!timer) {
        fprintf(stderr, "Failed to create timer.\n");
        goto catch;
    }
    
    al_init_font_addon();
    al_init_ttf_addon();

    get_desktop_resolution(ALLEGRO_DEFAULT_DISPLAY_ADAPTER, &monitor_w,
                           &monitor_h);

    /* Find the largest size the screen can be */
    scale_x = monitor_w / (float) CANVAS_W;
    scale_y = monitor_h / (float) CANVAS_H;
    
    if (scale_x < scale_y) {
        scale = (int) scale_x;
    } else {
        scale = (int) scale_y;
    }
    
    /* TEMP */
    scale = 2;
    
    screen_w = scale * CANVAS_W;
    screen_h = scale * CANVAS_H;

    /*al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW);*/

    /* Initialize the one and only global display for the game */
    display = al_create_display(screen_w, screen_h);
    
    if (!display) {
        fprintf(stderr, "Failed to create display.\n");
        goto catch;
    }

    /* Scale the screen to the largest size the monitor can handle */
    al_identity_transform(&trans);
    al_scale_transform(&trans, scale, scale);
    al_use_transform(&trans);
    
    /* Hide the mouse cursor */
    al_hide_mouse_cursor(display);

    events = al_create_event_queue();
    
    if (!events) {
        fprintf(stderr, "Failed to create events!\n");
        goto catch;
    }

    al_register_event_source(events, al_get_display_event_source(display));
    al_register_event_source(events, al_get_timer_event_source(timer));
    al_register_event_source(events, al_get_keyboard_event_source());

    /* Initialize the animation functions */
    init_animator(FPS);

    /* Initialize the physics functions */
    init_physics(FPS);

    /* Initialize the resource library */
    init_resources();
    add_resource_path("images/");

    /* Set the window title and icon */
    al_set_window_title(display, "Super Bumblebee Ball");
    al_set_display_icon(display, load_resource_image("icon.bmp"));

    al_start_timer(timer);

    /* START THE GAME */
    /*run(update_game, draw_game, game);*/
    run(update_title_screen, draw_title_screen, NULL);

finally:

    /**
     * Clean up.
     */
    stop_resources();
    
    check_memory();
    
    al_destroy_display(display);
    al_destroy_event_queue(events);
    al_destroy_timer(timer);

    return status;
    
catch:

    status = -1;
    
    goto finally;
}
