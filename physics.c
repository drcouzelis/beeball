#include "physics.h"


static float physics_fps = 60;  /* A default value of 60 frames per second */


void init_physics(float fps)
{
    physics_fps = fps;
}


float accelerate(float value, float by)
{
    return value + (by / physics_fps);
}
