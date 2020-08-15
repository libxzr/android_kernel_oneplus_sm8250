#include <linux/module.h>

bool is_miui;
module_param_named(is_miui, is_miui, bool, 0644);
