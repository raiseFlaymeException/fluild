#include "fluild.h"

void print_usage(const char *program) {
    printf("Usage:\n"
           "    %s <build|clean>\n"
           "Param:\n"
           "    build: build the hellos programs\n"
           "    clean: delete all the hellos programs\n",
           program);
}

int build_hello_async(size_t n, FluildThreadCmd *thread_cmd) {
    char *hello_exe = asprintf("hellos\\hello%zu.exe", n);
    char *hello_c   = asprintf("hellos\\hello%zu.c", n);

    // cmd must be zeroed
    fluild_cmd_append_many(&thread_cmd->cmd, "gcc", "-o", hello_exe, hello_c);

    free(hello_exe);
    free(hello_c);

    fluild_cmd_system_async(thread_cmd);
    return 0;
}

int build_hellos() {
    FluildThreadCmd threads[20] = {0};
    for (size_t i = 0; i < 20; ++i) {
        if (build_hello_async(i, threads + i) != 0) {
            fluild_log(FLUILD_LOG_ERROR, "error constructing cmd");
            return 1;
        }
    }
    for (size_t i = 0; i < 20; ++i) {
        if (FluildThreadCmd_get_result(threads + i, NULL) != 0) {
            fluild_log(FLUILD_LOG_ERROR, "compilation of hello%d.c return non zero exit code", i);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    fluild_log_use_mutex();

    if (!fluild_rebuild(argc, argv)) {
        fluild_log(FLUILD_LOG_ERROR, "can't rebuild this file");
        return 1;
    }

    char *prog = shift_args(&argc, &argv);
    assert(prog && "somehow argv[0] doesn't contain the program name");
    char *command = shift_args(&argc, &argv);

    if (!command) {
        print_usage(prog);
    } else if (streq(command, "build")) {
        if (build_hellos() != 0) {
            fluild_log(FLUILD_LOG_ERROR, "compilation of hellos return non zero exit code");
            return 1;
        }
    } else if (streq(command, "clean")) {
        for (size_t i = 0; i < 20; ++i) {
            char   *hello_exe = asprintf("hellos\\hello%zu.exe", i);
            SString cmd       = {0};
            fluild_cmd_append_many(&cmd, "del", hello_exe);
            if (fluild_cmd_exec(&cmd, NULL) != 0) {
                fluild_log(FLUILD_LOG_ERROR, "deleting %s failed", hello_exe);
                return 1;
            }
            free(hello_exe);
        }
    } else {
        print_usage(prog);
    }

    fluild_log_destroy_mutex();
    return 0;
}
