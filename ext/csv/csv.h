#include <stdio.h>
#include <stdbool.h>

#define CSV_ERR_SIZE 255

typedef enum
{
    NO_ERR,
    END_OF_FIELD,
    END_OF_FILE,
    END_OF_ROW,
} CsvReader_Err;

typedef struct
{
    char ok;
    CsvReader_Err err;
} CsvReader_Result;

typedef struct
{
    FILE *in;
    char zErrMsg[CSV_ERR_SIZE];
    bool isFailed;
} CsvReader;

void CsvReader_Init(CsvReader *r, const char *zFilePath);
void CsvReader_Cleanup(CsvReader *r);
CsvReader_Result CsvReader_Getc(CsvReader *r);
