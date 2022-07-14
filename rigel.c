// copyright 2022 my ass

#include "rigel.h"

struct options parse_args(int argc, char **argv) {
    if (argc != 2) {
        printf("fuck you: wrong number of arguments (wanted 1, got %d)", argc - 1);
        ExitProcess(1);
    }

    struct options opts = { .command = argv[1] };
    return opts;
}

int main(int argc, char **argv) {
    seed_random();
    struct options opts = parse_args(argc, argv);

    PROCESS_INFORMATION process = create_process(opts);
    wait_for(process, opts);


    return 0;
}

void seed_random() {
    SYSTEMTIME time;
    GetLocalTime(&time);

    srand(time.wMilliseconds + time.wSecond * 1000);
}

PROCESS_INFORMATION create_process(struct options opts) {
    STARTUPINFO startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process;
    ZeroMemory(&process, sizeof(process));

    printf("trying to make process \"%s\"\n", opts.command);

    int return_code = CreateProcessA(
        NULL,
        opts.command,
        NULL,
        NULL,
        0,
        0,
        NULL,
        NULL,
        &startup,
        &process
    );

    check_fail(return_code, "CreateProcessA");

    return process;
}

void wait_for(PROCESS_INFORMATION process, struct options opts) {
    while (WaitForSingleObject(process.hProcess, 5000) != WAIT_OBJECT_0) {
        SuspendThread(process.hThread);
        corrupt(process.hProcess, opts);
        ResumeThread(process.hThread);
    }

    printf("[!] RIGEL : IS MY WORK COMPLETE?\n");
    
    CloseHandle(process.hProcess);
}

void corrupt(HANDLE process, struct options opts) {
    int return_code;

    printf("[!]\n");
    printf("[!] RIGEL : CORRUPTION SPREADS.\n");
    printf("[!]\n");

    struct c_block *blocks = scan_memory(process);
    debug_c_block_list(blocks);

    corrupt_blocks(process, blocks);

    free_c_block_list(blocks);
}

void corrupt_blocks(HANDLE process, struct c_block *blocks) {
    int return_code;
    size_t _ignoredz;
    int _ignoredd;

    struct c_block *current = blocks;

    while (current != NULL) {
        return_code = VirtualProtectEx(
            process,
            current->address,
            current->size,
            PAGE_EXECUTE_WRITECOPY,
            &_ignoredd
        );

        if (return_code == 0) {
            printf("[!corrupt]: a VPE attempt at %p, size %zd failed with (%d). Skipping.\n",
                current->address,
                current->size,
                GetLastError()
            );
            
            /*
            current = current->next;
            continue;
            */
        }

        char *memory = malloc(current->size);
        size_t read;
        return_code = ReadProcessMemory(
            process,
            current->address,
            memory,
            current->size,
            &read
        );

        if (return_code == 0 && GetLastError() == 299) {
            printf("[ corrupt]: incomplete read occured\n");
        } else if (return_code == 0) {
            check_fail(return_code, "ReadProcessMemory");
        }

        printf("[ corrupt]: %zd bytes were read at %p (wanted %zd)\n",
            read,
            current->address,
            current->size
        );

        if (read == 0) {
            printf("[ corrupt]: have no bytes... continuing\n");

            current = current->next;
            continue;
        }

        corrupt_memory(memory, read);

        printf("[ corrupt]: %zd bytes of memory were corrupted\n", read); 

        size_t written;
        return_code = WriteProcessMemory(
            process,
            current->address,
            memory,
            read,
            &written
        );

        if (return_code == 0 && GetLastError() == 299) {
            printf("[ corrupt]: incomplete write occured\n");
        } else if (return_code == 0) {
            check_fail(return_code, "WriteProcessMemory");
        }

        printf("[ corrupt]: %zd bytes were written at %p (wanted %zd)\n",
            written,
            current->address,
            read
        );

        free(memory);
        current = current->next;
    }
}

void corrupt_memory(char *memory, size_t size) {
    if (size == 0) {
        return;
    }

    size_t index = rand() % size;
    printf("[ corrupt]: byte at offset %zd corrupted\n", index);
    memory[index]++;
}

// you have to free this entire structure (thanks obama)
struct c_block *scan_memory(HANDLE process) {
    SYSTEM_INFO system;
    GetSystemInfo(&system);

    char *stop = SIZE_MAX;

    struct c_block *head = NULL;
    struct c_block *tail = NULL;

    for (char *i = 0; i < stop; i += system.dwPageSize) {
        MEMORY_BASIC_INFORMATION memory;

        size_t bytes_queried = VirtualQueryEx(
            process,
            i,
            &memory,
            sizeof(memory)
        );

        if (bytes_queried == 0 && GetLastError() != 87) {
            printf("[!scan] unknown memory scanning error (%d) at %p\n", GetLastError(), i);
            continue;
        } else if (bytes_queried == 0) {
            printf("[!scan] reached end of available memory\n");
            break;
        }

        if (memory.State == MEM_COMMIT && memory.Type == MEM_PRIVATE) {
            printf("[!scan] found a block of private, committed memory %zd large at %p\n",
                memory.RegionSize,
                memory.BaseAddress
            );

            struct c_block *new_block = malloc(sizeof(struct c_block));
            new_block->address = memory.BaseAddress;
            new_block->size = memory.RegionSize;
            new_block->next = NULL;

            if (tail != NULL) {
                tail->next = new_block;
            }

            tail = new_block;

            if (head == NULL) {
                head = tail;
            }
        } else if (memory.State == MEM_FREE) {
            printf("[ scan] found a block of free memory %zd large at %p\n",
                memory.RegionSize,
                memory.BaseAddress
            );
        } else if (memory.State == MEM_RESERVE) {
            printf("[ scan] found a block of reserve memory %zd large at %p\n",
                memory.RegionSize,
                memory.BaseAddress
            );
        }

        i += memory.RegionSize;
    }

    return head;
}

void check_fail(int code, const char *name) {
    int error = GetLastError();
    
    if (code == 0) {
        printf("[!] RIGEL : SYSTEM CALL %s DEFIED ME WITH CODE (%d)\n", name, error);
        printf("[!] I KNOW NOT THE INVOCATION OF THIS FAILURE.\n");
        ExitProcess(error);
    }
}

void debug_c_block_list(struct c_block *head) {
    struct c_block *current = head;

    while (current != NULL) {
        printf("cblock: address %p, size %zd, next %p\n",
            current->address,
            current->size,
            current->next
        );

        current = current->next;
    }
}

void free_c_block_list(struct c_block *head) {
    if (head->next != NULL) {
        free_c_block_list(head->next);
    }

    free(head);
}
