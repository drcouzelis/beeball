#include <allegro5/allegro.h>

#include "input.h"
#include "utilities.h"


/* Hold the state of the keyboard */
static ALLEGRO_KEYBOARD_STATE kbdstate;

static int key_held[ALLEGRO_KEY_MAX];
static FLAG key_init = OFF;


void init_keys()
{
  int i;
  for (i = 0; i < ALLEGRO_KEY_MAX; i++) {
    key_held[i] = 0;
  }
  key_init = ON;
}


int is_key_pressed(char code)
{
    unsigned int k = code;

    if (!key_init) {
      init_keys();
    }
    
    al_get_keyboard_state(&kbdstate);

    if (!key_held[k] && al_key_down(&kbdstate, k)) {
        key_held[k] = 1;
        return 1;
    }

    if (key_held[k] && !al_key_down(&kbdstate, k)) {
        key_held[k] = 0;
        return 0;
    }

    return 0;
}


int is_key_held(char code)
{
    unsigned int k = code;

    if (!key_init) {
      init_keys();
    }
    
    al_get_keyboard_state(&kbdstate);

    if (al_key_down(&kbdstate, k)) {
        key_held[k] = 1;
        return 1;
    }

    key_held[k] = 0;

    return 0;
}
