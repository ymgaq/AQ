#pragma once

#include "board.h"
#include "sgf.h"
#include "cluster.h"
#include "search.h"

int CallGTP();

extern bool save_log;
extern bool need_time_control;
extern bool use_pondering;
