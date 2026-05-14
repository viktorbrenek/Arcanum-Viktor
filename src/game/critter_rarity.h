#ifndef ARCANUM_GAME_CRITTER_RARITY_H_
#define ARCANUM_GAME_CRITTER_RARITY_H_

#include <stdint.h>

#include "tig/color.h"

typedef enum CritterRarity {
    CRITTER_RARITY_NORMAL = 0,
    CRITTER_RARITY_MAGIC  = 1,
    CRITTER_RARITY_RARE   = 2,
    CRITTER_RARITY_UNIQUE = 3,
} CritterRarity;

// Bonus flags stored in OBJ_F_CRITTER_PAD_I_1 bits 2-7.
#define CRITTER_BONUS_ENRAGED      0x04  // +STAT_DAMAGE_BONUS
#define CRITTER_BONUS_SWIFT        0x08  // +STAT_SPEED
#define CRITTER_BONUS_ARMORED      0x10  // +STAT_AC_ADJUSTMENT
#define CRITTER_BONUS_REGENERATING 0x20  // +STAT_HEAL_RATE
#define CRITTER_BONUS_BRUTISH      0x40  // +STAT_STRENGTH
#define CRITTER_BONUS_WARY         0x80  // +STAT_PERCEPTION

CritterRarity critter_rarity_get(int64_t obj);
int critter_rarity_bonus_get(int64_t obj);
void critter_rarity_roll(int64_t obj);
tig_color_t critter_rarity_color(CritterRarity rarity);
void critter_rarity_generate_name(int64_t obj, const char* base_name,
    char* out_buf, int buf_size);

#endif /* ARCANUM_GAME_CRITTER_RARITY_H_ */
