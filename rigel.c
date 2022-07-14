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
    struct options opts = parse_args(argc, argv);

    printf("[!] Your computer's power level is %d\n", RAND_MAX);

    PROCESS_INFORMATION process = create_process(opts);
    wait_for(process, opts);


    return 0;
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
    struct c_block *blocks = scan_memory(process.hProcess);
    debug_c_block_list(blocks);

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

    MODULEINFO exe_module = get_module(process, opts);
    printf("Entry point is %p\n", exe_module.EntryPoint);

    size_t bytes_read;
    char *exe_memory = read_memory(process, exe_module, &bytes_read);

    corrupt_memory(exe_memory, bytes_read);
    write_memory(process, exe_module, exe_memory, bytes_read);

    free(exe_memory);
}

char *read_memory(HANDLE process, MODULEINFO mod, size_t *out_bytes_read) {
    int return_code;
    size_t max_read = 1 << 20;
    char *memory = malloc(max_read);

    printf(" need to read %zd bytes\n", max_read);
    
    size_t read;
    return_code = ReadProcessMemory(
        process,
        mod.EntryPoint,
        memory,
        max_read,
        &read
    );
    printf("actually read %zd bytes\n", read);

    if (return_code == 0 && GetLastError() == 299) {
        printf("[!] partial read\n");
    } else {
        check_fail(return_code, "ReadProcessMemory");
    }

    *out_bytes_read = read;
    return memory;
}

void write_memory(HANDLE process, MODULEINFO mod, char *memory, size_t size) {
    int return_code;
    size_t written;

    int _old;
    return_code = VirtualProtectEx(
        process,
        mod.EntryPoint,
        size,
        PAGE_EXECUTE_WRITECOPY,
        &_old
    );

    check_fail(return_code, "VirtualProtectEx");

    return_code = WriteProcessMemory(
        process,
        mod.EntryPoint,
        memory,
        size,
        &written
    );
    printf("wrote back %zd bytes\n", written);

    if (return_code == 0 && GetLastError() == 299) {
        printf("[!] partial write\n");
    } else {
        check_fail(return_code, "WriteProcessMemory");
    }
}

void corrupt_memory(char *memory, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (rand() % 10000 == 0) {
        }
    }
}

MODULEINFO get_module(HANDLE process, struct options opts) {
    int return_code;

    HMODULE *modules = malloc((1 << 8) * sizeof(HMODULE));
    int modules_bytes;

    return_code = EnumProcessModules(
        process,
        modules,
        (1 << 8) * sizeof(HMODULE),
        &modules_bytes
    );

    size_t modules_read = modules_bytes / sizeof(HMODULE);

    check_fail(return_code, "EnumProcessModules");
    printf("read %zd modules\n", modules_read);


    for (int i = 0; i < modules_read; i++) {
        char *filename = malloc(1 << 8);

        return_code = GetModuleFileNameExA(
            process,
            modules[i],
            filename,
            1 << 8
        );

        check_fail(return_code, "GetModuleFileNameExA");

        if (!strncmp(filename, opts.command, strlen(opts.command))) {
            printf("module found: %s\n", filename);

            MODULEINFO module;
            ZeroMemory(&module, sizeof(module));

            return_code = GetModuleInformation(
                process,
                modules[i],
                &module,
                sizeof(module)
            );

            check_fail(return_code, "GetModuleInformation");

            free(filename);
            free(modules);

            return module;
        }

        free(filename);
    }
    
    free(modules);
    
    printf("[!] couldn't find the executable module. ur fucked\n");
    ExitProcess(1);
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

        if (memory.State == MEM_COMMIT) {
            printf("[!scan] found a block of committed memory %zd large at %p\n",
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
