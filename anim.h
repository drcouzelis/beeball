#ifndef ANIM_H
#define ANIM_H


typedef struct ANIM ANIM;


/**
 * Send the frames per second that the game is running at.
 */
void init_animator(float fps);

/**
 * Create an empty animation. Set loop to true to loop
 * the animation. The speed is in frames per second.
 * Set the fps parameter to the number of frames per
 * second the game is running at.
 */
ANIM *create_anim(int loop, float speed);

/**
 * Add a frame to the animation. Returns a pointer
 * to the same animation.
 */
ANIM *add_frame(ANIM * anim, ALLEGRO_BITMAP * frame);

/**
 * Remove all frames from the animation and free
 * the memory. Returns a pointer to the same
 * animation.
 */
ANIM *destroy_frames(ANIM * anim);

/**
 * Free the memory of the animation. This will not free
 * the memory of the frames.
 */
void destroy_anim(ANIM * anim);

/**
 * Reset the animation to its starting frame.
 */
void reset_anim(ANIM *anim);

/**
 * Animate!
 */
void animate(ANIM * anim);

/**
 * Draw the animation to the current target bitmap.
 * For information on the target bitmap and the flags,
 * please see the Allegro documentation.
 */
void draw_anim(ANIM * anim, float x, float y, int flags);

/**
 * Animation width. This is the width of the
 * first frame.
 */
int anim_width(ANIM * anim);

/**
 * Animation height. This is the height of the
 * first frame.
 */
int anim_height(ANIM * anim);

/**
 * Returns true if the animation is at the
 * end of the last fram. An animation that
 * loops will always return false.
 */
int anim_done(ANIM * anim);

/**
 * Get the current frame.
 */
ALLEGRO_BITMAP *current_frame(ANIM * anim);


#endif
