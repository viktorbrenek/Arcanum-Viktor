#ifndef ARCANUM_UI_ITEM_TOOLTIP_H_
#define ARCANUM_UI_ITEM_TOOLTIP_H_

#include "game/context.h"

bool item_tooltip_init(void);
void item_tooltip_exit(void);
void item_tooltip_show(int64_t item_obj, const char* item_name);
void item_tooltip_hide(void);

#endif /* ARCANUM_UI_ITEM_TOOLTIP_H_ */
