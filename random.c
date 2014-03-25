#include <stdlib.h>
#include <time.h>
#include "random.h"


static int init_random_numbers = 0;


int random_number(int low, int high)
{
    if (!init_random_numbers) {
        srand(time(NULL));
        init_random_numbers = 1;
    }

    return (rand() % (high - low + 1)) + low;
}
