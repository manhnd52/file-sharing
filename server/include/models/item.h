typedef struct File {
    int id;
    char *name;
    int folder_id;
    int owner_id;
    char *storage_hash;
    long long size;     
    char *created_at; 
} File;

