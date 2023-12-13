#include <sqlite3ext.h> 
SQLITE_EXTENSION_INIT1

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define min(a,b) (((a)<(b))?(a):(b))

// reference : https://en.wikipedia.org/wiki/Levenshtein_distance
int levenshteinDistance(const char* s, const char* t) {
    int **dp;
    int m = strlen(s), n = strlen(t), i, j, res;

    dp = (int **) sqlite3_malloc((m+1) * sizeof(int*));
    for (i=0; i<=m; i++){
        dp[i] = (int*) sqlite3_malloc((n+1) * sizeof(int));
    }

    dp[0][0] = 0;
    for (i=1; i<=m; i++){
        dp[i][0] = i;
    }

    for (i=1; i<=n; i++){
        dp[0][i] = i;
    }

    for (i=1; i<=m; i++){
        for (j=1; j<=n; j++){
            dp[i][j] = min(
                min(
                    dp[i-1][j] + 1, 
                    dp[i][j-1] + 1
                )
                ,
                dp[i-1][j-1] + (s[i-1] == t[j-1] ? 0 : 1)
            );
        }
    }

    res = dp[m][n];
    for (int  i=0; i<m; i++) sqlite3_free(dp[i]);
    sqlite3_free(dp);

    return res;
}


static void sqliteLevenshtein(
sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
    assert(argc == 2);
    const unsigned char* s = sqlite3_value_text(argv[0]);
    const unsigned char* t = sqlite3_value_text(argv[1]);

    int res = levenshteinDistance((const char*) s, (const char*) t);
    sqlite3_result_int(context, res);
}

#ifdef _WIN32
__declspec(dllexport)
#endif

int fuzzy_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void) pzErrMsg;

    rc = sqlite3_create_function(
        db, 
        "levenshtein", 2, 
        SQLITE_UTF8 | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC, 
        0, sqliteLevenshtein, 0, 0);

    return rc;
}
