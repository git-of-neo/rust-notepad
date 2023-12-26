/*
    Fuzzy sort / Fuzzy scorer.

    Takes inspiration from:
     1. https://www.forrestthewoods.com/blog/reverse_engineering_sublime_texts_fuzzy_match/
     2. vscode's fuzzyscorer https://github.com/microsoft/vscode/blob/main/src/vs/base/test/common/fuzzyScorer.test.ts#L80
     3. fuzzysort.js https://github.com/farzher/fuzzysort
*/
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef BUILD_EXTENSION
#include <sqlite3ext.h>

void free2(void *thing)
{
    sqlite3_free(thing);
}

void *malloc2(int size)
{
    return sqlite3_malloc(size);
}
#else

void free2(void *thing)
{
    free(thing);
}

void *malloc2(int size)
{
    return malloc(size);
}
#endif

typedef struct
{
    int just;
    bool nothing;
} Maybe;

#define Nothing() ((Maybe){.nothing = true})
#define Just(value) ((Maybe){.just = (value), .nothing = false})
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const char EOS = '\0';
static const int sentinel = -1;

struct State
{
    const size_t nSearch;
    const size_t nTarget;
    const char *search;
    const char *target;
    const char *pLowerSearch;
    const char *pLowerTarget;
    int **memo;
};

static inline bool isBeginning(const char prev, const char curr)
{
    bool wasAlphanum = isalnum(prev);
    bool wasUpper = isupper(prev);
    bool isUpper = isupper(curr);
    return (isUpper && wasUpper) || !wasAlphanum;
}

static int fuzzyScore(struct State *state, int s, int t, int streak)
{
    if (s >= state->nSearch || t >= state->nTarget)
        return 0;
    if (state->memo[s][t] != sentinel)
        return state->memo[s][t];

    bool match = state->pLowerSearch[s] == state->pLowerTarget[t];
    int bestScore = fuzzyScore(state, s, t + 1, 0);

    // scoring loosely follows vscode fuzzyScorer's
    if (match)
    {
        bool isSameCase = state->search[s] == state->target[t];
        bool isStart = t == 0;

        int score = 1; // score for single match
        score += 5 * streak++;
        score += 1 * isSameCase; // same case
        score += 8 * isStart;
        score += 4 * (!isStart && isBeginning(state->target[t - 1], state->target[t]));

        bestScore = MAX(bestScore, score + fuzzyScore(state, s + 1, t + 1, streak));
    }

    state->memo[s][t] = bestScore;
    return state->memo[s][t];
}

char *strtolower(const char *s, const size_t length)
{
    char *pBuffer = (char *)malloc2((length + 1) * sizeof(char));
    for (int i = 0; i < length; i++)
    {
        pBuffer[i] = tolower(s[i]);
    }
    pBuffer[length] = EOS;
    return pBuffer;
}

Maybe go(const char *search, const char *target)
{
    const size_t nSearch = strlen(search);
    const size_t nTarget = strlen(target);
    const char *pLowerSearch = strtolower(search, nSearch);
    const char *pLowerTarget = strtolower(target, nTarget);

    char *pSearch = (char *)pLowerSearch;
    char *pTarget = (char *)pLowerTarget;

    /*
        Naive matching
    */
    for (;;)
    {
        if (*pSearch == *pTarget)
        {
            if (*(++pSearch) == EOS)
                break;
        }
        if (*(++pTarget) == EOS)
            return Nothing();
    }

    /*
        Init state
    */
    struct State state = {
        .search = search,
        .target = target,
        .nSearch = nSearch,
        .nTarget = nTarget,
        .pLowerSearch = pLowerSearch,
        .pLowerTarget = pLowerTarget};

    int **memo = (int **)malloc2(nSearch * sizeof(int *));
    for (int i = 0; i < nSearch; i++)
    {
        size_t size = nTarget * sizeof(int);
        memo[i] = (int *)malloc2(size);
        memset(memo[i], sentinel, size);
    }
    state.memo = memo;

    /*
        Score
    */
    int thisScore = fuzzyScore(&state, 0, 0, 0);

    /*
        Dealloc
    */
    for (int i = 0; i < nSearch; i++)
    {
        free2(state.memo[i]);
    }
    free2(state.memo);
    free2((void *)pLowerSearch);
    free2((void *)pLowerTarget);

    return Just(thisScore);
}

void formatMaybe(const Maybe *monad, char *pBuffer)
{
    if (monad->nothing)
    {
        sprintf(pBuffer, "Nothing");
    }
    else
    {
        sprintf(pBuffer, "Just %d", monad->just);
    }
}

int main(void)
{
    char pBuffer[255];
    Maybe res = go("fs", "Fuzzy Search");
    formatMaybe(&res, pBuffer);
    printf("%s\n", pBuffer); // expected : 14

    res = go("fs", "uzzyF Search");
    formatMaybe(&res, pBuffer);
    printf("%s\n", pBuffer); // expected : 6

    res = go("FS", "Fuzzy Search");
    formatMaybe(&res, pBuffer);
    printf("%s\n", pBuffer); // expected : 16

    res = go("test", "test");
    formatMaybe(&res, pBuffer);
    printf("%s\n", pBuffer); // expected : 46

    res = go("doesn't exist", "target");
    formatMaybe(&res, pBuffer);
    printf("%s\n", pBuffer); // expected : Nothing

    return 0;
}