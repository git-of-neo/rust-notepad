#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

typedef struct CsvTable
{
    sqlite3_vtab base;
    char *zFilename;
    int nCols;
    sqlite3 *db;
} CsvTable;

typedef struct CsvCursor
{
    sqlite3_vtab_cursor base;
    FILE *pFile;
    char **azData;
    int rowId;
    bool isEof;
} CsvCursor;

typedef enum ResultTypes
{
    READFIELD_OK,
    READFIELD_EOR,
    READFIELD_EOF,
} ResultTypes;

/* Implementation */
static int CsvTable_Eof(sqlite3_vtab_cursor *pCursor)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    return cursor->isEof;
}

char fpeek(FILE *pFile)
{
    char c = fgetc(pFile);
    ungetc(c, pFile);
    return c;
}

void readArgument(sqlite3_str *s, const char *arg)
{
    // trim head
    bool go1;
    do
    {
        switch (*arg)
        {
        case ' ':
        case '\'':
        case '\"':
            arg++;
            break;
        default:
            go1 = false;
            break;
        };
    } while (go1);

    // remaining
    const char *prev = arg;
    while (*arg != '\0')
    {
        switch (*arg)
        {
        case ' ':
        case '\'':
        case '\"':
            arg++;
            break;
        default:
            while (prev != arg)
            {
                sqlite3_str_appendchar(s, 1, *prev);
            };
            sqlite3_str_appendchar(s, 1, *arg);
            prev++;
            arg++;
            break;
        }
    }
}

ResultTypes readfield(FILE *stream, sqlite3_str *z)
{
    char c2, c;
    char *res;
    bool go = true;
    ResultTypes r = READFIELD_OK;

    while (go)
    {
        c = fgetc(stream);
        switch (c)
        {
        case EOF:
            r = READFIELD_EOF;
            go = false;
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
    readArgument(strFilename, argv[argc - 1]);
    pTab->zFilename = sqlite3_str_finish(strFilename);
    printf("USING FILE : %s\n", pTab->zFilename);

    // define schema
    sqlite3_str *strSql = sqlite3_str_new(db);
    sqlite3_str_appendf(strSql, "CREATE TABLE x(");

    char *field;
    FILE *pFile;
    sqlite3_str *strField;
    int r;
    bool go = true;

    pFile = fopen(pTab->zFilename, "r");
    if (pFile == NULL)
    {
        *pzErr = sqlite3_mprintf("Cannot open file");
        goto cleanup;
    }

    int nCols = 0;

    while (go)
    {
        strField = sqlite3_str_new(db);
        r = readfield(pFile, strField);
        field = sqlite3_str_finish(strField);

        switch (r)
        {
        case READFIELD_EOF:
            go = false;
            break;
        case READFIELD_EOR:
            go = false;
        case READFIELD_OK:
            printf("field %d -> %s\n", nCols, field);

            if (field == NULL)
            {
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
    if (rc)
    {
        *pzErr = sqlite3_mprintf("Bad schema");
        goto cleanup;
    }

    // save meta data
    pTab->nCols = nCols;
    pTab->db = db;

    // cleanup
cleanup:
    sqlite3_free(zSchema);
    fclose(pFile);
    return rc;
};

static int CsvTable_Disconnect(sqlite3_vtab *pVtab)
{
    CsvTable *table = (CsvTable *)pVtab;
    sqlite3_free(table->zFilename);
    return SQLITE_OK;
};

static int CsvTable_Open(sqlite3_vtab *pVtab, sqlite3_vtab_cursor **ppCursor)
{
    CsvTable *table = (CsvTable *)pVtab;
    CsvCursor *cursor;
    size_t nBytes = sizeof(*cursor) + (sizeof(char *)) * table->nCols;
    cursor = sqlite3_malloc64(nBytes);
    if (cursor == 0)
        return SQLITE_NOMEM;
    memset(cursor, 0, nBytes);
    cursor->azData = (char **)&cursor[1];
    *ppCursor = &cursor->base;
    cursor->pFile = fopen(table->zFilename, "rb");
    cursor->rowId = 1;
    if (cursor->pFile == NULL)
        return SQLITE_ERROR;

    sqlite3_str *str = sqlite3_str_new(table->db);
    while (readfield(cursor->pFile, str) != READFIELD_EOR)
    {
    }

    return SQLITE_OK;
};

static int CsvTable_Close(sqlite3_vtab_cursor *pCursor)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    CsvTable *table = (CsvTable *)cursor->base.pVtab;

    for (int i = 0; i < table->nCols; i++)
    {
        sqlite3_free(cursor->azData[i]);
    }
    sqlite3_free(cursor);
    fclose(cursor->pFile);
    return SQLITE_OK;
};

static int CsvTable_Next(sqlite3_vtab_cursor *pCursor)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    CsvTable *table = (CsvTable *)cursor->base.pVtab;
    sqlite3 *db = table->db;
    int rc = SQLITE_OK;

    sqlite3_str *strField;
    FILE *pFile = cursor->pFile;
    ResultTypes r = READFIELD_OK;

    if (fpeek(pFile) == EOF)
    {
        cursor->isEof = true;
        return rc;
    }

    for (int i = 0; i < table->nCols; i++)
    {
        strField = sqlite3_str_new(db);
        r = readfield(pFile, strField);
        cursor->azData[i] = sqlite3_str_finish(strField);
    }

    cursor->rowId++;
    return rc;
};

static int CsvTable_Column(sqlite3_vtab_cursor *pCursor, sqlite3_context *ctx, int i)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    sqlite3_result_text(ctx, cursor->azData[i], -1, SQLITE_TRANSIENT);
    return SQLITE_OK;
};

static int CsvTable_RowId(sqlite3_vtab_cursor *pCursor, sqlite_int64 *pRowid)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    *pRowid = cursor->rowId;
    return SQLITE_OK;
};

static int CsvTable_BestIndex(
    sqlite3_vtab *tab,
    sqlite3_index_info *pIdxInfo)
{
    pIdxInfo->estimatedCost = 1000000;
    return SQLITE_OK;
}

static int CsvTable_Filter(
    sqlite3_vtab_cursor *pCursor,
    int idxNum, const char *idxStr,
    int argc, sqlite3_value **argv)
{
    return CsvTable_Next(pCursor);
}

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
    .xRowid = CsvTable_RowId,
    .xBestIndex = CsvTable_BestIndex,
    .xFilter = CsvTable_Filter};

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