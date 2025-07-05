#include "turret.h"
#include <time.h>

#define Env Turret
#include "../env_binding.h"

static int my_init(Env* env, PyObject* args, PyObject* kwargs) {
    srand(time(NULL) + (uintptr_t)env);
    return 0;
}

static int my_log(PyObject* dict, Log* log) {
    assign_to_dict(dict, "score", log->score);
    assign_to_dict(dict, "episode_return", log->episode_return);
    assign_to_dict(dict, "episode_length", log->episode_length);
    assign_to_dict(dict, "hit_rate", log->hit_rate);
    assign_to_dict(dict, "waste_rate", log->waste_rate);
    assign_to_dict(dict, "n", log->n);
    return 0;
}
