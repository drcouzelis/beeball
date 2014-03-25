#include <allegro5/allegro.h>
#include <stdio.h>
#include "anim.h"


#define ANIM_MAX_FRAMES 8


static float animator_fps = 60; /* A default value of 60 frame per second */


struct ANIM {
    ALLEGRO_BITMAP *frames[ANIM_MAX_FRAMES];
    int size;                   /* Number of frames */
    int pos;                    /* The current frame number */
    float fudge;

    float speed;                /* In frames per second */
    int loop;                   /* Set true to loop forever */
    int done;                   /* Is true if finished animating */

    int w;
    int h;
};


void init_animator(float fps)
{
    animator_fps = fps;
}


ANIM *create_anim(int loop, float speed)
{
    ANIM *anim;
    int i;

    anim = malloc(sizeof(ANIM));

    if (anim != NULL) {
        for (i = 0; i < ANIM_MAX_FRAMES; i++) {
            anim->frames[i] = NULL;
        }
        anim->size = 0;
        anim->pos = 0;
        anim->fudge = 0;
        anim->speed = speed / animator_fps;
        anim->loop = loop;
        anim->done = 1;
    }

    return anim;
}


ANIM *add_frame(ANIM * anim, ALLEGRO_BITMAP * frame)
{
    if (anim == NULL || frame == NULL) {
        return anim;
    }

  /**
   * Don't add the frame if there's no more room.
   */
    if (anim->size >= ANIM_MAX_FRAMES - 1) {
        fprintf(stderr, "ANIM: Failed to add frame to animation.\n");
        fprintf(stderr, "Please increase ANIM_MAX_FRAMES.\n");
    }

    anim->frames[anim->size] = frame;
    anim->size++;

  /**
   * Set the width and height to the size of
   * the first frame.
   */
    if (anim->size == 1) {
        anim->w = al_get_bitmap_width(frame);
        anim->h = al_get_bitmap_height(frame);
    }

    anim->pos = 0;
    anim->fudge = 0;
    anim->done = 0;

    return anim;
}


ANIM *destroy_frames(ANIM * anim)
{
    int i;

    if (anim != NULL) {
        for (i = 0; i < anim->size; i++) {
            free(anim->frames[i]);
        }
        anim->size = 0;
    }

    anim->pos = 0;
    anim->fudge = 0;
    anim->done = 1;

    return anim;
}


void destroy_anim(ANIM * anim)
{
    free(anim);
}


void reset_anim(ANIM *anim)
{
    if (anim != NULL) {
        anim->pos = 0;
        anim->fudge = 0;
        anim->done = 0;
    }
}


void animate(ANIM * anim)
{
    if (anim == NULL) {
        return;
    }

    if (anim->done) {
        return;
    }

  /**
   * Every time you "animate", add the speed
   * to the fudge.
   */
    anim->fudge += anim->speed;

  /**
   * When the fudge is greater than one, it's
   * time to go to the next frame.
   */
    while (anim->fudge > 1 && !anim->done) {

        anim->pos++;

        if (anim->pos >= anim->size) {
            if (anim->loop) {
                anim->pos = 0;
            } else {
                anim->done = 1;
            }
        }

        anim->fudge -= 1;
    }
}


void draw_anim(ANIM * anim, float x, float y, int flags)
{
    if (anim) {
        al_draw_bitmap(anim->frames[anim->pos], x, y, 0);
    }
}


int anim_width(ANIM * anim)
{
    if (anim != NULL) {
        return anim->w;
    }

    return 0;
}


int anim_height(ANIM * anim)
{
    if (anim != NULL) {
        return anim->h;
    }

    return 0;
}


int anim_done(ANIM * anim)
{
    if (anim != NULL) {
        return anim->done;
    }

  /**
   * A non-existant animation is done animating.
   */
    return 1;
}

ALLEGRO_BITMAP *current_frame(ANIM * anim)
{
    if (anim != NULL) {
        return anim->frames[anim->pos];
    }

    return NULL;
}
