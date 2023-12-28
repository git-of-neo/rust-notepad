#include <stdio.h>
#include <stdbool.h>

#define CSV_ERR_SIZE 255
#define CSV_BUFFER_SIZE 1024

typedef enum
{
    NO_ERR,
    END_OF_FIELD,
    END_OF_FILE,
    END_OF_ROW,
    TERMINATED,
} CsvReader_Err;

typedef struct
{
    char ok;
    CsvReader_Err err;
} CsvReader_Result;

#define Err(e) ((CsvReader_Result){.err = e})
#define Ok(value) ((CsvReader_Result){.ok = (value), .err = NO_ERR})

typedef struct
{
    FILE *in;
    char zCol[CSV_BUFFER_SIZE];
    char zIn[CSV_BUFFER_SIZE];
    char zErrMsg[CSV_ERR_SIZE];
    bool isFailed;
} CsvReader;

static void CsvReader_SetErr(CsvReader *r, const char *msg)
{
    r->isFailed = true;
    snprintf(r->zErrMsg, CSV_ERR_SIZE, "%s", msg);
}

void CsvReader_Init(CsvReader *r, const char *zFilePath)
{
    FILE *ptr;
    ptr = fopen(zFilePath, "r");

    if (ptr == NULL)
    {
        CsvReader_SetErr(r, "Cannot open file");
        return;
    }

    r->in = ptr;
}

void CsvReader_Cleanup(CsvReader *r)
{
    fclose(r->in);
}

// consecutive peek leads could lead to undefined behaviour per document of ungetc
static char fpeek(FILE *pntr)
{
    char c = fgetc(pntr);
    ungetc(c, pntr);
    return c;
}

CsvReader_Result CsvReader_Getc(CsvReader *r)
{
    char c = fgetc(r->in);
    switch (c)
    {
    case ',':
        return Err(END_OF_FIELD);
    case '\r':
        if (fpeek(r->in) == '\n')
            fgetc(r->in);
    case '\n':
        return Err(END_OF_ROW);
    default:
        return Ok(c);
    }
}

// unsafe
static void getRow(CsvReader *r, char *zBuffer)
{
    int pntr = 0;
    for (;;)
    {
        CsvReader_Result res = CsvReader_Getc(r);
        switch (res.err)
        {
        case NO_ERR:
            zBuffer[pntr++] = res.ok;
            break;
        case END_OF_FIELD:
            zBuffer[pntr++] = ',';
            break;
        case END_OF_ROW:
            return;
        }
    }
}

int main()
{
    CsvReader reader;
    CsvReader_Init(&reader, "./files.csv");

    if (reader.isFailed)
    {
        printf("%s\n", reader.zErrMsg);
        return 1;
    }

    char zBuffer[255];
    getRow(&reader, zBuffer);
    printf("%s", zBuffer);

    CsvReader_Cleanup(&reader);
}