#ifndef PHYSICS_H
#define PHYSICS_H


/**
 * Initialize the physics functions.
 * Send the frames per second that the
 * game is running at.
 */
void init_physics(float fps);

/**
 * Returns the new value after adjusting it
 * by a certain amount.
 */
float accelerate(float value, float by);


#endif
