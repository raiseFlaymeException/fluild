#include "fluild.h"

int main() {
    FluildFTime ftime;
    if (!FluildFTime_load(&ftime, FLUILD_DEFAULT_FILENAME_FTIME)) {
        puts("error");
        return 1;
    }

    for (size_t i = 0; i < ftime.size; ++i) {
        printf("filename: %.*s, hour: %d, minute: %d, second: %d\n", ftime.items[i].filename.size,
               ftime.items[i].filename.data, ftime.items[i].hour, ftime.items[i].minute,
               ftime.items[i].second);
    }

    FluildFTime_clear(&ftime);

    return 0;
}

int main2() {
    FluildCmd cmd;
    if (!FluildCmd_init(&cmd, FLUILD_CMD_DEFAULT_CAP)) { return 1; }
    if (!FluildCmd_append_many(&cmd, "echo", "hello")) { return 1; }

    SString result;
    SString_init(&result, 16);
    if (FluildCmd_exec(&cmd, &result) != 0) { return 1; }

    SString_append(&result, '\0');
    printf("result: %s", result.data);

    FluildCmd_deinit(&cmd);
    return 0;
}
