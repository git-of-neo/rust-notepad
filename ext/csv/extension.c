#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include "csv.h"
#include <string.h>

struct StringBuilder
{
    char *zData;
    size_t nLen;
    size_t nCapacity;
};

static void StringBuilder_Init(struct StringBuilder *builder)
{
    builder->nLen = 0;
    builder->nCapacity = 100;
    builder->zData = sqlite3_malloc(builder->nCapacity * sizeof(char));
}

static void StringBuilder_Append(struct StringBuilder *builder, const char c)
{
    if (builder->nLen + 1 == builder->nCapacity)
    {
        builder->nCapacity = builder->nCapacity * 1.5;
        // TODO : handle OOM
        builder->zData = sqlite3_realloc(builder->zData, builder->nCapacity);
    }
    builder->zData[builder->nLen++] = c;
}

static void Stringbuilder_Extend(struct StringBuilder *builder, const char *zString)
{
    for (int i = 0; i < strlen(zString); i++)
        StringBuilder_Append(builder, zString[i]);
}

static const char *StringBuilder_Build(struct StringBuilder *builder)
{
    builder->zData[builder->nLen + 1] = '\0';
    return (const char *)builder->zData;
}

static void StringBuilder_Reset(struct StringBuilder *builder)
{
    StringBuilder_Init(builder);
}

static void StringBuilder_Cleanup(struct StringBuilder *builder)
{
    sqlite3_free(builder->zData);
}

typedef struct CsvTable
{
    sqlite3_vtab base;
    char *zFilename;
    int nCols;
} CsvTable;

typedef struct CsvCursor
{
    sqlite3_vtab_cursor base;
    CsvReader reader;
    struct StringBuilder builder;
    char **azData;
    int *aLen; // might be useless?
    bool isEof;
    bool isHeaderRead;
} CsvCursor;

/* Support these table functions */
static int CsvTable_Connect(sqlite3 *, void *, int, const char *const *, sqlite3_vtab **, char **);
static int CsvTable_Open(sqlite3_vtab *, sqlite3_vtab_cursor **);
static int CsvTable_Close(sqlite3_vtab_cursor *);
static int CsvTable_Next(sqlite3_vtab_cursor *);
static int CsvTable_Eof(sqlite3_vtab_cursor *);
static int CsvTable_Disconnect(sqlite3_vtab *);
static int CsvTable_Column(sqlite3_vtab_cursor *, sqlite3_context *, int);

/* Implementation */
static int CsvTable_Connect(sqlite3 *db, void *pAux, int argc, const char *const *argv, sqlite3_vtab **ppVTab, char **pzErr)
{
    const char *zFilename = argv[argc - 1];
    CsvReader reader;
    CsvReader_Init(&reader, zFilename);

    if (reader.isFailed)
    {
        sqlite3_free(*pzErr);
        *pzErr = sqlite3_mprintf("%s", reader.zErrMsg);
        return SQLITE_ERROR;
    }

    CsvTable *tab = sqlite3_malloc(sizeof(CsvTable));
    size_t nBytes = strlen(zFilename) * sizeof(char) + sizeof(char);
    tab->zFilename = sqlite3_malloc(nBytes);
    sqlite3_snprintf(nBytes, tab->zFilename, "%s", zFilename);

    struct StringBuilder builder;
    StringBuilder_Init(&builder);
    Stringbuilder_Extend(&builder, "CREATE TABLE ");
    Stringbuilder_Extend(&builder, tab->zFilename);
    StringBuilder_Append(&builder, '(');

    bool newField = false;
    bool go = true;
    int nCols = 0;
    while (go)
    {
        CsvReader_Result res = CsvReader_Getc(&reader);
        switch (res.err)
        {
        case NO_ERR:
            if (newField)
            {
                StringBuilder_Append(&builder, ',');
                newField = false;
            }
            StringBuilder_Append(&builder, res.ok);
            break;
        case END_OF_FIELD:
            Stringbuilder_Extend(&builder, " TEXT");
            newField = true;
            nCols++;
            break;
        case END_OF_ROW:
            StringBuilder_Append(&builder, ')');
            StringBuilder_Append(&builder, ';');
            go = false;
            break;
        }
    }
    tab->nCols = nCols;

    *ppVTab = (sqlite3_vtab *)tab;

    sqlite3_declare_vtab(db, StringBuilder_Build(&builder));

    StringBuilder_Cleanup(&builder);
    CsvReader_Cleanup(&reader);

    return SQLITE_OK;
};

static int CsvTable_Open(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor)
{
    CsvTable *tab = (CsvTable *)pVTab;
    CsvCursor *cursor = sqlite3_malloc(sizeof(CsvCursor));
    cursor->aLen = sqlite3_malloc(tab->nCols * sizeof(int));
    cursor->azData = sqlite3_malloc(tab->nCols * sizeof(char *));
    cursor->isEof = false;
    cursor->isHeaderRead = false;
    *ppCursor = &cursor->base;
    StringBuilder_Init(&cursor->builder);
    CsvReader_Init(&cursor->reader, tab->zFilename);
    return SQLITE_OK;
};

static int CsvTable_Close(sqlite3_vtab_cursor *pCursor)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    CsvTable *tab = (CsvTable *)cursor->base.pVtab;
    for (int i = 0; i < tab->nCols; i++)
    {
        sqlite3_free(cursor->azData[i]);
    }
    sqlite3_free(cursor->azData);
    sqlite3_free(cursor->aLen);
    StringBuilder_Cleanup(&cursor->builder);
    CsvReader_Cleanup(&cursor->reader);
    return SQLITE_OK;
}

static int CsvTable_Next(sqlite3_vtab_cursor *pCursor)
{
    CsvCursor *cursor = (CsvCursor *)cursor;
    CsvReader *reader = &cursor->reader;
    struct StringBuilder *builder = &cursor->builder;
    StringBuilder_Reset(builder);

    bool go = true;
    int nCols = 0;

    while (!cursor->isHeaderRead)
    {
        CsvReader_Result res = CsvReader_Getc(reader);
        switch (res.err)
        {
        case END_OF_ROW:
            cursor->isHeaderRead = true;
            break;
        case END_OF_FILE:
            cursor->isEof = true;
            return SQLITE_OK;
        default:
            continue;
        }
    }

    while (go)
    {
        CsvReader_Result res = CsvReader_Getc(reader);
        switch (res.err)
        {
        case NO_ERR:
            StringBuilder_Append(builder, res.ok);
            break;
        case END_OF_FIELD:
        case END_OF_ROW:
            sqlite3_free(cursor->azData[nCols]);
            size_t nBytes = builder->nLen * sizeof(char) + sizeof(char);
            cursor->azData[nCols] = sqlite3_malloc(nBytes);
            cursor->aLen[nCols] = builder->nLen;
            memcpy(cursor->azData[nCols], StringBuilder_Build(builder), nBytes);
            nCols++;
            go = res.err == END_OF_ROW;
            break;
        case END_OF_FILE:
            cursor->isEof = true;
            go = false;
            break;
        }
    }
    return SQLITE_OK;
}

static int CsvTable_Column(
    sqlite3_vtab_cursor *pCursor,
    sqlite3_context *ctx,
    int i)
{
    CsvCursor *cursor = (CsvCursor *)pCursor;
    sqlite3_result_text(ctx, cursor->azData[i], -1, SQLITE_TRANSIENT);
    return SQLITE_OK;
}

static int CsvTable_Eof(sqlite3_vtab_cursor *cursor)
{
    return ((CsvCursor *)cursor)->isEof;
}

static int CsvTable_Disconnect(sqlite3_vtab *pVTab)
{
    CsvTable *p = (CsvTable *)pVTab;
    sqlite3_free(p->zFilename);
    sqlite3_free(p);
    return SQLITE_OK;
}

static sqlite3_module CsvModule = {
    .iVersion = 0,
    .xCreate = CsvTable_Connect,
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

    int csv_init(
        sqlite3 *db,
        char **pzErrMsg,
        const sqlite3_api_routines *pApi)
{
    int rc;
    SQLITE_EXTENSION_INIT2(pApi);
    rc = sqlite3_create_module(db, "csv", &CsvModule, 0);

    return rc;
}