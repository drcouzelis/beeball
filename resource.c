#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "resource.h"


#define MAX_RESOURCE_PATHS 4
#define MAX_BITMAP_RESOURCES 50
#define MAX_RESOURCE_FILENAME_SIZE 256


typedef struct {
    char name[MAX_RESOURCE_FILENAME_SIZE];
    ALLEGRO_BITMAP *bitmap;
} BITMAP_RESOURCE;


static BITMAP_RESOURCE *bitmap_resources[MAX_BITMAP_RESOURCES];
static int num_bitmap_resources = 0;

static char resource_paths[MAX_RESOURCE_FILENAME_SIZE][MAX_RESOURCE_PATHS];
static int num_resource_paths = 0;


void init_resources()
{
    int i;

    for (i = 0; i < MAX_BITMAP_RESOURCES; i++) {
        bitmap_resources[i] = NULL;
    }
}


void stop_resources()
{
    int i;

    for (i = 0; i < num_bitmap_resources; i++) {
        if (bitmap_resources[i] != NULL) {
            al_destroy_bitmap(bitmap_resources[i]->bitmap);
            free(bitmap_resources[i]);
            bitmap_resources[i] = NULL;
        }
    }

    num_bitmap_resources = 0;
}


void add_resource_path(const char *path)
{
    if (num_resource_paths >= MAX_RESOURCE_PATHS) {
        fprintf(stderr, "RESOURCES: Failed to add path.\n");
        fprintf(stderr, "RESOURCES: Try increasing MAX_RESOURCE_PATHS.\n");
        return;
    }

  /**
   * Add the new path to the list of resource paths.
   */
    strncpy(resource_paths[num_resource_paths], path,
            MAX_RESOURCE_FILENAME_SIZE - 1);

    num_resource_paths++;
}


/**
 * Internal function.
 * Load a bitmap and set magic pink to transparent.
 */
ALLEGRO_BITMAP *load_bitmap_with_magic_pink(const char *filename)
{
    ALLEGRO_BITMAP *bitmap;

    bitmap = al_load_bitmap(filename);

    if (bitmap != NULL) {
        al_convert_mask_to_alpha(bitmap, al_map_rgb(255, 0, 255));
    }

    return bitmap;
}


/**
 * Internal function.
 * Create a BITMAP_RESOURCE structure.
 */
BITMAP_RESOURCE *load_bitmap_resource(const char *name,
                                      ALLEGRO_BITMAP * bitmap)
{
    BITMAP_RESOURCE *resource;

    resource = malloc(sizeof(BITMAP_RESOURCE));
    if (resource != NULL) {
        strcpy(resource->name, name);
        resource->bitmap = bitmap;
    }

    return resource;
}



ALLEGRO_BITMAP *load_resource_image(const char *name)
{
    ALLEGRO_BITMAP *bitmap;
    char fullpath[MAX_RESOURCE_FILENAME_SIZE];
    int i;

  /**
   * Try to find the resource in the list of
   * resources that have already been loaded.
   */
    for (i = 0; i < num_bitmap_resources; i++) {
        if (strncmp
            (bitmap_resources[i]->name, name,
             MAX_RESOURCE_FILENAME_SIZE) == 0) {
            /*printf("Found resource %s\n", name); */
            return bitmap_resources[i]->bitmap;
        }
    }

  /**
   * Try the current working directory.
   */
    bitmap = load_bitmap_with_magic_pink(name);
    if (bitmap != NULL) {
        /*printf("Found resource in current directory %s\n", name); */
        bitmap_resources[num_bitmap_resources] =
            load_bitmap_resource(name, bitmap);
        num_bitmap_resources++;
        return bitmap;
    }

  /**
   * Try finding it in the list of resource paths.
   */
    for (i = 0; i < num_resource_paths; i++) {
        strcpy(fullpath, "");
        strncat(fullpath, resource_paths[i], MAX_RESOURCE_FILENAME_SIZE);
        strncat(fullpath, name, MAX_RESOURCE_FILENAME_SIZE);
        bitmap = load_bitmap_with_magic_pink(fullpath);
        if (bitmap != NULL) {
            /*printf("Found resource in resource path %s %s\n", resource_paths[i], name); */
            bitmap_resources[num_bitmap_resources] =
                load_bitmap_resource(name, bitmap);
            num_bitmap_resources++;
            return bitmap;
        }
    }

    fprintf(stderr, "RESOURCES: Failed to load resource: \"%s\".\n", name);
    return NULL;
}
