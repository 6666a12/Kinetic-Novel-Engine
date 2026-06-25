#pragma once

#include "graphics/Ifadable.h"
#include "graphics/screen_trans.h"
#include "text_area.h"
#include "button.h"
#include "slider.h"

#define UI_Component std::variant<buttonM, buttonS, slide, TextArea>