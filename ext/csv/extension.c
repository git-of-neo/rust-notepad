#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <stdio.h>
#include <stdbool.h>

typedef struct CsvTable
{
    sqlite3_vtab base;
    char *zFilename;
    int nCols;
} CsvTable;

typedef struct CsvCursor
{
    sqlite3_vtab_cursor base;
    FILE *pFile;
    char *azData;
    int rowId;
    bool isEof;
} CsvCursor;

typedef enum ResultTypes {
    READFIELD_OK,
    READFIELD_EOR,
    READFIELD_EOF,
} ResultTypes;


/* Support these table functions */
static int CsvTable_Connect(sqlite3 *, void *, int, const char *const *, sqlite3_vtab **, char **);
static int CsvTable_Open(sqlite3_vtab *, sqlite3_vtab_cursor **);
static int CsvTable_Close(sqlite3_vtab_cursor *);
static int CsvTable_Next(sqlite3_vtab_cursor *);
static int CsvTable_Eof(sqlite3_vtab_cursor *);
static int CsvTable_Disconnect(sqlite3_vtab *);
static int CsvTable_Column(sqlite3_vtab_cursor *, sqlite3_context *, int);

/* Implementation */
static int CsvTable_Eof(sqlite3_vtab_cursor *pCursor) {
    CsvCursor *cursor = (CsvCursor *)pCursor;
    return cursor -> isEof;
}


char fpeek(FILE *pFile){
    char c = fgetc(pFile);
    ungetc(c, pFile);
    return c;
}

ResultTypes readfield(FILE *stream, sqlite3_str *z){
    char c2, c;
    char *res;
    bool go = true;
    ResultTypes r = READFIELD_OK;

    while(go){ 
        c = fgetc(stream);
        switch (c){
            case EOF:
                r = READFIELD_EOF;
                go=false;
                break;
            case '\r':
                c2 = fgetc(stream);
                if (c2 != '\n') 
                    ungetc(c2, stream);
            case '\n':
                r = READFIELD_EOR;
            case ',':
                go = false;
                break;
            default:
                sqlite3_str_appendchar(z, 1, c);
                break;
        }
    }

    return r;
}

static int CsvTable_Connect(sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVtab, char **pzErr)
{
    int rc = SQLITE_OK;
    char *zSchema;

    // set vtab
    CsvTable *pTab = sqlite3_malloc(sizeof(CsvTable));

    if (pTab == NULL)
    {
        return SQLITE_NOMEM;
    }

    *ppVtab = (sqlite3_vtab *)pTab;

    // save filename
    sqlite3_str *strFilename = sqlite3_str_new(db);
    sqlite3_str_appendf(strFilename, "%s", argv[argc - 1]);
    pTab->zFilename = sqlite3_str_finish(strFilename);
    printf("USING FILE : %s\n", pTab->zFilename);

    // define schema
    sqlite3_str *strSql = sqlite3_str_new(db);
    sqlite3_str_appendf(strSql, "CREATE TABLE x(");

    char *field;
    FILE *pFile;
    sqlite3_str *strField;
    int r;
    bool go =true;

    pFile = fopen(pTab->zFilename, "r");
    if (pFile ==NULL){
        *pzErr = sqlite3_mprintf("Cannot open file");
        goto cleanup;
    }

    int nCols = 0;

    while(go){
        strField = sqlite3_str_new(db);
        r = readfield(pFile, strField);
        field = sqlite3_str_finish(strField);

        switch (r){
            case READFIELD_EOF:
                go = false;
                break;
            case READFIELD_EOR:
                go = false;
            case READFIELD_OK:
                printf("field %d -> %s\n", nCols, field);

                if (field == NULL) {
                    sqlite3_free(field);
                    break;
                }

                if (nCols == 0)
                    sqlite3_str_appendf(strSql, "%s", field);
                else 
                    sqlite3_str_appendf(strSql, ", %s", field);
                break;
        };
        
        sqlite3_free(field);
        nCols++;
    };
    
    sqlite3_str_appendf(strSql, ")");
    zSchema = sqlite3_str_finish(strSql);

    printf("WITH SCHEMA : %s\n", zSchema);
    rc = sqlite3_declare_vtab(db, zSchema);
    if (rc) {
        *pzErr = sqlite3_mprintf("Bad schema");
        goto cleanup;
    }

    // save meta data
    pTab -> nCols = nCols;

    // cleanup
cleanup:
    sqlite3_free(zSchema);
    fclose(pFile);
    return rc;
};


static int CsvTable_Open(sqlite3_vtab *pVtab, sqlite3_vtab_cursor **ppCursor){
    return SQLITE_OK;
};

static int CsvTable_Close(sqlite3_vtab_cursor *pVtab){
    return SQLITE_OK;
    
};

static int CsvTable_Next(sqlite3_vtab_cursor *pVtab){
    return SQLITE_OK;
};

static int CsvTable_Disconnect(sqlite3_vtab *pVtab){
    return SQLITE_OK;
};

static int CsvTable_Column(sqlite3_vtab_cursor *pvTab, sqlite3_context *ctx, int i){
    return SQLITE_OK;

};

static sqlite3_module CsvModule = {
    .iVersion = 0,
    .xCreate = CsvTable_Connect,
    .xConnect = CsvTable_Connect,
    .xOpen = CsvTable_Open,
    .xClose = CsvTable_Close,
    .xNext = CsvTable_Next,
    .xEof = CsvTable_Eof,
    .xDisconnect = CsvTable_Disconnect,
    .xDestroy = CsvTable_Disconnect,
    .xColumn = CsvTable_Column,
};

#ifdef _WIN32
__declspec(dllexport)
#endif

    int sqlite3_csv_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi)
{
    int rc;
    SQLITE_EXTENSION_INIT2(pApi);
    rc = sqlite3_create_module(db, "csv", &CsvModule, 0);

    return rc;
}