#ifndef FLUILD_H
#define FLUILD_H

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define CC "gcc"

//////////////////////////////////////
//             SSTRING
//////////////////////////////////////
// this is a copy of sstring.h in my library called easy_c_data_structure
typedef struct {
    const char *data;
    size_t      size;
} SStringView;

typedef SStringView SStringAll;

typedef struct {
    char  *data;
    size_t size;
    size_t cap;
} SString;

void SString_alloc(SString *str, size_t cap) {
    str->data = malloc(cap * sizeof(*str->data));
    str->cap  = cap;
    str->size = 0;
}
void SString_dup_cstr(SString *str, const char *cstr) {
    str->data = strdup(cstr);
    str->cap  = strlen(cstr);
    str->size = 0;
}
void SString_free(SString *str) { free(str->data); }
void SString_extend(SString *str, size_t size) {
    if (size > str->cap) {
        str->data = realloc(str->data, size);
        str->cap  = size;
    }
}
void SString_append(SString *str, char data) {
    if (str->size + 1 >= str->cap) { SString_extend(str, str->cap << 1); }
    str->data[str->size++] = data;
}
bool SString_getc_until(SString *str, FILE *file, int char_or_eof) {
    int cur_char; // can be EOF so int (because sizeof(EOF)==sizeof(int)>sizeof(char))
    while (true) {
        cur_char = getc(file);
        if (cur_char == char_or_eof) { return true; }
        if (cur_char == EOF) { return false; }
        SString_append(str, cur_char);
    }
}

void SString_vprintf(SString *str, const char *format, va_list args) {
    size_t size = vsnprintf(NULL, 0, format, args);
    if (size > str->cap) { SString_extend(str, size); }
    vsprintf(str->data, format, args);
}
void SString_printf(SString *str, const char *format, ...) {
    va_list args;
    va_start(args, format);

    SString_vprintf(str, format, args);

    va_end(args);
}

void SStringView_init_cstr(SStringView *view, const char *cstr) {
    view->data = cstr;
    view->size = strlen(cstr);
}
void SStringView_init_whole(SStringView *view, const SStringAll *str) {
    view->data = str->data;
    view->size = str->size;
}
void SStringView_init_substr(SStringView *view, const SStringAll *str, size_t offset, size_t size) {
    view->data = str->data + offset;
    view->size = size;
}

void SString_append_str(SString *str, const SStringAll *to_append) {
    for (size_t i = 0; i < to_append->size; i++) { SString_append(str, to_append->data[i]); }
}
void SString_append_cstr(SString *str, const char *cstr) {
    SStringView view;
    SStringView_init_cstr(&view, cstr);
    SString_append_str(str, &view);
}

void SString_join(SString *res, const SStringAll *strs, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        SString_append_str(res, strs + i);
        SString_append(res, ' ');
    }
    if (size) { --res->size; }
}

long long SStringAll_findc(const SStringAll *str, char chr, size_t offset) {
    for (size_t i = offset; i < str->size; i++) {
        if (str->data[i] == chr) { return i; }
    }
    return -1;
}

size_t SStringAll_countc(const SStringAll *str, char chr, size_t offset) {
    long long size = 0;
    for (size_t i = offset; i < str->size; i++) {
        if (str->data[i] == chr) { size++; }
    }
    return size;
}
//////////////////////////////////////
//            END SSTRING
//////////////////////////////////////

//////////////////////////////////////
//             LOGGING
//////////////////////////////////////
#define USE_PRINTF_FORMAT(format_pos, first_va_arg)                                                \
    __attribute__((format(printf, format_pos, first_va_arg)))

USE_PRINTF_FORMAT(1, 0) char *vasprintf(const char *format, va_list args) {
    int size = vsnprintf(NULL, 0, format, args) + 1;
    if (size < 0) { return NULL; }

    char *buf = malloc(size);
    if (!buf) { return NULL; }

    int err = vsnprintf(buf, size, format, args);
    return (err < 0 || size < err) ? NULL : buf;
}

USE_PRINTF_FORMAT(1, 2) char *asprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    char *result = vasprintf(format, args);

    va_end(args);
    return result;
}

typedef enum {
    FLUILD_LOG_ERROR,
    FLUILD_LOG_INFO,
    FLUILD_LOG_END,
} FluildLogType;

static const char *const FLUILD_LOG_FORMATS[] = {"ERROR", "INFO"};
static pthread_mutex_t   fluild_mut           = 0;

void fluild_log_use_mutex() { pthread_mutex_init(&fluild_mut, NULL); }
void fluild_log_destroy_mutex() {
    pthread_mutex_destroy(&fluild_mut);
    fluild_mut = 0;
}

bool vfluild_log(FluildLogType log_type, const char *format, va_list args) {
    assert(log_type < FLUILD_LOG_END && "invalid log type");

    char *log_format = asprintf("%s: %s\n", FLUILD_LOG_FORMATS[log_type], format);
    if (!log_format) { return false; }

    if (fluild_mut) { pthread_mutex_lock(&fluild_mut); }
    int result = vprintf(log_format, args);
    if (fluild_mut) { pthread_mutex_unlock(&fluild_mut); }

    free(log_format);
    return result >= 0;
}
void fluild_log(FluildLogType log_type, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfluild_log(log_type, format, args);

    va_end(args);
}

//////////////////////////////////////
//            END LOGGING
//////////////////////////////////////

//////////////////////////////////////
//               ARGS
//////////////////////////////////////

char *shift_args(int *argc, char ***argv) { return ((*argc)--) ? *(*argv)++ : NULL; }

#define streq(a, b) (strcmp(a, b) == 0)

//////////////////////////////////////
//             END ARGS
//////////////////////////////////////

//////////////////////////////////////
//                CMD
//////////////////////////////////////

#define FLUILD_CMD_DEFAULT_CAP     16
#define FLUILD_CMD_STR_DEFAULT_CAP 64

typedef struct {
    pthread_t thread;
    SString   cmd;
    SString   res;
    int       ret;
} FluildThreadCmd;

void fluild_cmd_append(SString *cmd, const char *cstr) {
    if (!cmd->cap) { SString_alloc(cmd, FLUILD_CMD_STR_DEFAULT_CAP); }
    if (cmd->size) { SString_append(cmd, ' '); }

    SStringView str;
    SStringView_init_cstr(&str, cstr);
    SString_append_str(cmd, (SStringAll *)&str);
}

void fluild_cmd_vappend_many(SString *cmd, va_list args) {
    const char *to_append;
    while ((to_append = va_arg(args, const char *))) { fluild_cmd_append(cmd, to_append); }
}

void _fluild_cmd_append_many(SString *cmd, ...) {
    va_list args;
    va_start(args, cmd);
    fluild_cmd_vappend_many(cmd, args);
    va_end(args);
}
#define fluild_cmd_append_many(cmd, ...) _fluild_cmd_append_many(cmd, __VA_ARGS__, NULL)

int fluild_cmd_exec(SString *cmd, SString *result) {
    SString_append(cmd, '\0');

    fluild_log(FLUILD_LOG_INFO, "CMD %s", cmd->data);

    FILE *proc = popen(cmd->data, "r");
    SString_free(cmd);

    if (result) {
        if (!SString_getc_until(result, proc, EOF)) { return false; }
    }
    return pclose(proc);
}

static void *fluild_thread_cmd_exec(void *_cmd) {
    FluildThreadCmd *cmd = _cmd;
    cmd->ret             = fluild_cmd_exec(&cmd->cmd, &cmd->res);
    return NULL;
}
void fluild_cmd_system_async(FluildThreadCmd *thread_handle) {
    pthread_create(&thread_handle->thread, NULL, fluild_thread_cmd_exec, thread_handle);
}
int FluildThreadCmd_get_result(FluildThreadCmd *thread, SString *result) {
    pthread_join(thread->thread, NULL);
    if (result) { *result = thread->res; }
    return thread->ret;
}
//////////////////////////////////////
//            END CMD
//////////////////////////////////////

//////////////////////////////////////
//         REBUILD/FILE TIME
//////////////////////////////////////
#define FLUILD_NS100_TO_S_DIV 10000000ULL

// (1901 - 1601) -> number of year between these two dates
// * 365         -> convert years to days
// + 89          -> add leap day
// * 24          -> convert to hours
// * 60          -> convert to minutes
// * 60          -> convert to seconds
#define FLUILD_SECONDS_BETWEEN_1601_1970                                                           \
    ((unsigned long long)((1970 - 1601) * 365 + 89) * 24 * 60 * 60)

time_t fluild_get_file_time(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        fluild_log(FLUILD_LOG_ERROR, "error querying file time of %s", filename);
        return 0;
    }
    return st.st_mtime;
}
bool _fluild_rebuild(const char *this_file, int argc, char **argv) {
    char *program = *argv;
    assert(program && "somehow argv doen't contain the program name");

    time_t this_file_time = fluild_get_file_time(this_file);
    time_t fluild_h_time  = fluild_get_file_time(__FILE__);
    time_t fluild_time    = fluild_get_file_time(program);

    if (this_file_time > fluild_time || fluild_h_time > fluild_time) {
        fluild_log(FLUILD_LOG_INFO, "rebuilding %s", program);

        char *program_old = asprintf("%s.old", program);

        if (access(program_old, F_OK) == 0 && remove(program_old)) {
            fluild_log(FLUILD_LOG_ERROR, "failed to remove %s , error: %s", program_old,
                       strerror(errno));
            return false;
        }
        if (rename(program, program_old)) {
            fluild_log(FLUILD_LOG_ERROR, "failed to rename %s to %s, error: %s", program,
                       program_old, strerror(errno));
            return false;
        }

        SString cmd = {0};
        fluild_cmd_append_many(&cmd, CC, "-o", program, this_file);

        if (fluild_cmd_exec(&cmd, NULL)) {
            fluild_log(FLUILD_LOG_ERROR, "failed to compile %s", program);
            if (rename(program_old, program)) {
                fluild_log(FLUILD_LOG_ERROR, "failed to rename %s to %s, error: %s", program_old,
                           program, strerror(errno));
                return false;
            }

            free(program_old);
            return false;
        }
        free(program_old);

        fluild_log(FLUILD_LOG_INFO, "reuruning %s", program);

        char **args = malloc((argc + 1) * sizeof(*args));
        for (int i = 0; i < argc; ++i) { args[i] = argv[i]; }
        args[argc] = NULL;

        spawnv(P_WAIT, program, args);
        free(args);
        exit(0);
    }
    return true;
}
#define fluild_rebuild(argc, argv) _fluild_rebuild(__FILE__, argc, argv)

//////////////////////////////////////
//        END REBUILD/FILE TIME
//////////////////////////////////////

#endif // FLUILD_H
