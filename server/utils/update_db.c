#include <stdio.h>
#include "database.h"

#define DB_SCHEMA_PATH "data/database/schema.sql"

int main() {
    if (db_init(DB_SCHEMA_PATH)) {
        printf("Update database successfully.\n");
        db_close();
    } else {
        printf("Failed to update database.\n");
    }
    return 0;
}