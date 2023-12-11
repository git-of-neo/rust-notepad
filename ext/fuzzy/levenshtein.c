#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define min(a,b) (((a)<(b))?(a):(b))

// reference : https://en.wikipedia.org/wiki/Levenshtein_distance
int levenshteinDistance(const char* s, const char* t) {
    int **dp;
    int m = strlen(s), n = strlen(t), i, j, res;

    dp = (int **) malloc((m+1) * sizeof(int*));
    for (i=0; i<=m; i++){
        dp[i] = (int*) malloc((n+1) * sizeof(int));
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
    for (int  i=0; i<m; i++) free(dp[i]);
    free(dp);

    return res;
}



