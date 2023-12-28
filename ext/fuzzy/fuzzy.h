#ifndef FUZZY_H
#define FUZZY_H

#include <stdbool.h>

typedef struct
{
    int just;
    bool nothing;
} Maybe;

Maybe go(const char *search, const char *target);

#endif