// copyright 2022 my ass

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <psapi.h>

void check_fail(int code, const char *name);

struct options {
    char *command;
};

struct options parse_args(int argc, char **argv);
void seed_random();

struct c_block {
    void *address;
    size_t size;
    struct c_block *next;
};

PROCESS_INFORMATION create_process(struct options opts);
void wait_for(PROCESS_INFORMATION process, struct options opts);
void corrupt(HANDLE process, struct options opts);
struct c_block *scan_memory(HANDLE process);
void corrupt_blocks(HANDLE process, struct c_block *head);
void corrupt_memory(char *memory, size_t size);

void debug_c_block_list(struct c_block *head);
void free_c_block_list(struct c_block *head);
