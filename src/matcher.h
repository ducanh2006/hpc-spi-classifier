#pragma once
#include <stdint.h>
#include "common.h"

// Initialize matcher by reading rules from file
int matcher_init(const char *rule_file);

// Match 5-tuple against rules. Returns rule index or -1 if no match.
int match_rule(const five_tuple_t *tuple);
