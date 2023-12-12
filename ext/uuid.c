/*
    references : https://sqlite.org/src/file/ext/misc/uuid.c
*/
#include <sqlite3ext.h> 
SQLITE_EXTENSION_INIT1

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


time_t last_timestamp = 0;
unsigned char clock_seq[2];
bool clock_set = false;
unsigned char node[6];
bool node_set = false;

/* Insert your extension code here */

// adapted from https://sqlite.org/src/file/ext/misc/uuid.c
static void blobToStrNoDash(
    const unsigned char *inp, 
    unsigned char* oup
){
    const char digits[] = "0123456789ABCDEF";
    unsigned char x;
    for (int i=0; i<16; i++){
        x = inp[i];
        oup[0] = digits[x>>4];
        oup[1] = digits[x&0xF];
        oup+=2;
    }
    oup[0] = '\0';
}

static void uuid1(
sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
    unsigned char blob[16];
    unsigned char buffer[33];
    (void) argc;
    (void) argv;

    time_t timestamp = time(NULL);

    if (timestamp <= last_timestamp) {
        timestamp = ++last_timestamp;
    }
    last_timestamp = timestamp;

    if (!node_set) {
        sqlite3_randomness(6, node);
        node[0] = node[0] | 1; // flag as random bytes
        node_set = true;
    }

    if (!clock_set) {
        sqlite3_randomness(2, clock_seq);
        clock_seq[0] = (clock_seq[0] & 0x3F) | 0x80; // set variant
        clock_set = true;
    }

    memcpy(blob, &timestamp, 8); // set timestamp
    blob[6] = (blob[6] & 0x0F) | (0x10); // set version

    memcpy(blob + 8, clock_seq, 2); // set clock
    memcpy(blob + 10, node, 6); // set node

    blobToStrNoDash(blob, buffer);
    sqlite3_result_text(context, (char *)buffer, 32, SQLITE_TRANSIENT);
}

#ifdef _WIN32
__declspec(dllexport)
#endif

int uuid_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void) pzErrMsg;

    rc = sqlite3_create_function(db, "uuid1", 0, SQLITE_UTF8 | SQLITE_INNOCUOUS, 0, uuid1, 0, 0);

    return rc;
}