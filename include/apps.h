typedef struct {
    const char* id;
    const char* name;
    void (*entry)(void);
} AppDefinition;
