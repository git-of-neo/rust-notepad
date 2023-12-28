#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#define BUILD_EXTENSION 1
#include "fuzzy.h"
#include <assert.h>
#include <stdio.h>

static void sqliteFuzzyScore(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv)
{
    assert(argc == 2);

    const unsigned char *s = sqlite3_value_text(argv[0]);
    const unsigned char *t = sqlite3_value_text(argv[1]);

    Maybe res = go((const char *)s, (const char *)t);

    if (res.nothing)
    {
        sqlite3_result_null(context);
    }
    else
    {
        sqlite3_result_int(context, res.just);
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif

    int fuzzy_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;

    rc = sqlite3_create_function(db, "fuzzy_score", 2, SQLITE_UTF8 | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC, 0, sqliteFuzzyScore, 0, 0);

    return rc;
}