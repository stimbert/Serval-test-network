typedef struct ArrayHostent{
    struct in_addr address;
    char hostname[100];
    int index;
    struct ArrayHostent *next;
} ArrayHostent;



ArrayHostent * findServers();

ArrayHostent* initArray();
ArrayHostent* insertArray(ArrayHostent *a, struct hostent element);
int freeArray(ArrayHostent *a);
