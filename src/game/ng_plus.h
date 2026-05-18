#ifndef ARCANUM_GAME_NG_PLUS_H_
#define ARCANUM_GAME_NG_PLUS_H_

#include <stdbool.h>

#define NG_PLUS_MAX_LEVEL 3

void ng_plus_init(void);
void ng_plus_exit(void);
void ng_plus_reload(void);
void ng_plus_save(void);

int  ng_plus_get_level(void);
int  ng_plus_get_last_level(void);
bool ng_plus_is_unlocked(void);
void ng_plus_unlock(void);
void ng_plus_set_level(int level);
void ng_plus_increment(void);

void ng_plus_char_save_level(int level);
int  ng_plus_char_get_level(void);

#endif /* ARCANUM_GAME_NG_PLUS_H_ */
