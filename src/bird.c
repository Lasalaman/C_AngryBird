/**
 * @file bird.c
 * @brief 小鳥欄位初始化與狀態更新
 */

#include "bird.h"

#include <stdlib.h>
#include <string.h>

static float default_radius_for_species(BirdSpecies species)
{
    (void)species;
    return 16.0f;
}

static float default_mass_for_species(BirdSpecies species)
{
    switch (species) {
    case BIRD_SPECIES_BLUE:
        return 0.8f;
    case BIRD_SPECIES_YELLOW:
        return 0.9f;
    case BIRD_SPECIES_BLACK:
        return 1.5f;
    case BIRD_SPECIES_RED:
    default:
        return 1.0f;
    }
}

void bird_init(Bird *out, uint32_t id, BirdSpecies species, float spawn_x, float spawn_y)
{
    if (out == NULL) {
        return;
    }

    out->id = id;
    out->species = species;
    out->position.x = spawn_x;
    out->position.y = spawn_y;
    out->velocity.x = 0.0f;
    out->velocity.y = 0.0f;
    out->radius = default_radius_for_species(species);
    out->mass = default_mass_for_species(species);
    out->life_state = LIFE_STATE_ACTIVE;
    out->is_alive = true;
    out->has_been_launched = false;
}

Bird *bird_create(uint32_t id, BirdSpecies species, float spawn_x, float spawn_y)
{
    Bird *bird = (Bird *)malloc(sizeof(Bird));
    if (bird == NULL) {
        return NULL;
    }

    bird_init(bird, id, species, spawn_x, spawn_y);
    return bird;
}

void bird_destroy(Bird *bird)
{
    free(bird);
}

void bird_mark_destroyed(Bird *bird)
{
    if (bird == NULL) {
        return;
    }

    bird->is_alive = false;
    bird->life_state = LIFE_STATE_DESTROYED;
}

void bird_copy(Bird *dest, const Bird *src)
{
    if (dest == NULL || src == NULL) {
        return;
    }

    memcpy(dest, src, sizeof(Bird));
}
