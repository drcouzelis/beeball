#ifndef INPUT_H
#define INPUT_H


/**
 * See the Allegro documentation for a list of key codes.
 *
 * http://www.allegro.cc/manual/5/keyboard.html
 */


void init_input();

/**
 * Returns true if the key has been pressed since
 * the last time this function was called.
 */
int is_key_pressed(char code);

/**
 * Returns true if the key is being held down.
 */
int is_key_held(char code);


#endif
