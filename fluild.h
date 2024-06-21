#ifndef FLUILD_H
#define FLUILD_H

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#error "OS other than windows is not supported yet"
#endif

//////////////////////////////////////
//              SSTRING
//////////////////////////////////////
// this is a copy of sstring in my library called easy_c_data_structure
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
void SString_free(SString *str) { free(str->data); }
void SString_append(SString *str, char data) {
    if (str->size + 1 >= str->cap) {
        str->cap *= 2;
        str->data = realloc(str->data, str->cap);
    }
    str->data[str->size++] = data;
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

bool SString_getc_until(SString *str, FILE *file, int char_or_eof) {
    int cur_char; // can be EOF so int (because sizeof(EOF)>sizeof(char))
    while (true) {
        cur_char = getc(file);
        if (cur_char == char_or_eof) { return true; }
        if (cur_char == EOF) { return false; }
        SString_append(str, cur_char);
    }
}

void SString_append_str(SString *str, const SStringAll *to_append) {
    for (size_t i = 0; i < to_append->size; i++) { SString_append(str, to_append->data[i]); }
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

#define FLUILD_CMD_DEFAULT_CAP     16
#define FLUILD_CMD_STR_DEFAULT_CAP 64

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

void fluild_cmd_append_many(SString *cmd, ...) {
    va_list args;
    va_start(args, cmd);
    fluild_cmd_vappend_many(cmd, args);
    va_end(args);
}
#define fluild_cmd_append_many(...) fluild_cmd_append_many(__VA_ARGS__, NULL)

int fluild_cmd_system(SString *cmd) {
    SString_append(cmd, '\0');

    fluild_log(FLUILD_LOG_INFO, "CMD %s", cmd->data);
    int result = system(cmd->data);

    SString_free(cmd);
    return result;
}

int FluildCmd_exec(SString *cmd, SString *result) {
    SString_append(cmd, '\0');

    fluild_log(FLUILD_LOG_INFO, "CMD %s", cmd->data);

    FILE *proc = popen(cmd->data, "r");
    SString_free(cmd);

    if (!SString_getc_until(result, proc, EOF)) { return false; }
    return pclose(proc);
}

typedef struct {
    pthread_t thread;
    SString   cmd;
    int       ret;
} FluildThreadCmd;

static void *fluild_thread_cmd_system(void *_cmd) {
    FluildThreadCmd *cmd = _cmd;
    cmd->ret             = fluild_cmd_system(&cmd->cmd);
    return NULL;
}
void fluild_cmd_system_async(FluildThreadCmd *thread_handle) {
    pthread_create(&thread_handle->thread, NULL, fluild_thread_cmd_system, thread_handle);
}
int FluildThreadCmd_get_result(FluildThreadCmd *thread) {
    pthread_join(thread->thread, NULL);
    return thread->ret;
}

#define FLUILD_NS100_TO_S_DIV 10000000ULL

// (1901 - 1601) -> number of year between these two dates
// * 365         -> convert years to days
// + 89          -> add leap day
// * 24          -> convert to hours
// * 60          -> convert to minutes
// * 60          -> convert to seconds
#define FLUILD_SECONDS_BETWEEN_1601_1970                                                           \
    ((unsigned long long)((1970 - 1601) * 365 + 89) * 24 * 60 * 60)

// stolen from:
// https://www.gamedev.net/forums/topic/565693-converting-filetime-to-time_t-on-windows/
static time_t filetime_to_timet(const FILETIME *ft) {
    ULARGE_INTEGER ull;
    // put low part and hight part so quad part is the full 64 bit integer
    ull.LowPart  = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    // divide by number of second in 100 ns
    // then convert date from 01/01/1601 to 01/01/1970
    return ull.QuadPart / FLUILD_NS100_TO_S_DIV - FLUILD_SECONDS_BETWEEN_1601_1970;
}

time_t fluild_get_file_time(const char *filename) {
    HANDLE h =
        CreateFileA(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { return -1; }

    FILETIME last_write;
    if (!GetFileTime(h, NULL, NULL, &last_write)) { return -1; }
    if (!CloseHandle(h)) { return -1; }
    return filetime_to_timet(&last_write);
}

typedef struct {
    SStringView filename;
    int         hour;
    int         minute;
    int         second;
} FluildFTimeItem;

typedef struct {
    SString          full_file;
    FluildFTimeItem *items;
    size_t           size;
} FluildFTime;

#define FLUILD_FILE_TIME_DEFAULT_CAP  16
#define FLUILD_DEFAULT_FILENAME_FTIME "fluild.ftime"

bool FluildFTime_load(FluildFTime *ftime, const char *file_changed) {
    // TODO: use a real format for the file
    // current format:
    // filename1:hour1:minute1:second1\n
    // filename2:hour2:minute2:second2\n

    // read entire file
    SString_alloc(&ftime->full_file, FLUILD_FILE_TIME_DEFAULT_CAP);

    FILE *f = fopen(file_changed, "r");
    if (!f) { return false; }

    if (!SString_getc_until(&ftime->full_file, f, EOF)) { return false; }

    if (fclose(f) != 0) { return false; }

    // allocate items
    ftime->size  = SStringAll_countc((SStringAll *)&ftime->full_file, '\n', 0);
    ftime->items = malloc(ftime->size * sizeof(*ftime->items));

    // parse items
    long long offset_new_line = 0;
    for (size_t i = 0; i < ftime->size; ++i) {
        FluildFTimeItem *cur_item = &ftime->items[i];

        // find next_colon
        long long next_colon =
            SStringAll_findc((SStringAll *)&ftime->full_file, ':', offset_new_line);
        if (next_colon < 0) { return false; }

        // create filename:
        // ...\nfilename:...
        SStringView_init_substr((SStringAll *)&cur_item->filename, (SStringAll *)&ftime->full_file,
                                offset_new_line, next_colon - offset_new_line);

        // find next colon
        long long old_colon = next_colon + 1;
        next_colon = SStringAll_findc((SStringAll *)&ftime->full_file, ':', next_colon + 1);
        if (next_colon < 0) { return false; }

        // create hour
        // ...:hour:...
        SStringView buf;
        SStringView_init_substr(&buf, (SStringAll *)&ftime->full_file, old_colon,
                                next_colon - old_colon);
        cur_item->hour = strtol(buf.data, NULL, 10);

        // find next colon
        old_colon  = next_colon + 1;
        next_colon = SStringAll_findc((SStringAll *)&ftime->full_file, ':', next_colon + 1);
        if (next_colon < 0) { return false; }

        // create minute
        // ...:minute:...
        SStringView_init_substr(&buf, (SStringAll *)&ftime->full_file, old_colon,
                                next_colon - old_colon);
        cur_item->minute = strtol(buf.data, NULL, 10);

        // find next new line
        offset_new_line = SStringAll_findc((SStringAll *)&ftime->full_file, '\n', next_colon + 1);
        if (offset_new_line < 0) { return false; }

        // create second
        // ...:second\n...
        SStringView_init_substr(&buf, (SStringAll *)&ftime->full_file, next_colon + 1,
                                offset_new_line);
        cur_item->second = strtol(buf.data, NULL, 10);

        // consume the new line
        ++offset_new_line;
    }

    return true;
}
bool FluildFTime_save(const FluildFTime *ftime, const char *file_changed) {
    FILE *f = fopen(file_changed, "w");
    if (!f) { return false; }

    for (size_t i = 0; i < ftime->size; ++i) {
        FluildFTimeItem *cur_item = ftime->items + i;
        if (fprintf(f, "%.*s:%02d:%02d:%02d\n", cur_item->filename.size, cur_item->filename.data,
                    cur_item->hour, cur_item->minute, cur_item->second) == EOF) {
            return false;
        }
    }

    return fclose(f) == 0;
}
void FluildFTime_unload(FluildFTime *ftime) {
    // deinit the entire file so this will deinit all SStringView
    SString_free(&ftime->full_file);

    // free the items array
    free(ftime->items);
}
bool FluildFTime_update(FluildFTime *ftime, const char *filename) {
    time_t     timestamp = fluild_get_file_time(filename);
    struct tm *time      = localtime(&timestamp);

    size_t filename_size = strlen(filename);

    // linear search for filename
    // NOTE: fine for now because not many file
    for (size_t i = 0; i < ftime->size; ++i) {
        SStringView *cur_filename = &ftime->items[i].filename;
        if (filename_size == cur_filename->size &&
            memcmp(cur_filename->data, filename, filename_size) == 0) {
            ftime->items[i].hour   = time->tm_hour;
            ftime->items[i].minute = time->tm_min;
            ftime->items[i].second = time->tm_sec;
            return true;
        }
    }
    // not found so create
    ftime->items = realloc(ftime->items, ++ftime->size);
    if (!ftime->items) { return false; }

    ftime->items[ftime->size - 1].hour   = time->tm_hour;
    ftime->items[ftime->size - 1].minute = time->tm_min;
    ftime->items[ftime->size - 1].second = time->tm_sec;

    ftime->items[ftime->size - 1].filename.data = filename;
    ftime->items[ftime->size - 1].filename.size = filename_size;
    return true;
}
bool FluildFTime_file_changed(const FluildFTime *ftime, const char *filename) {
    time_t     timestamp = fluild_get_file_time(filename);
    struct tm *time      = localtime(&timestamp);

    size_t filename_size = strlen(filename);

    // linear search for filename
    // NOTE: fine for now because not many file
    for (size_t i = 0; i < ftime->size; ++i) {
        FluildFTimeItem *cur_item = &ftime->items[i];
        if (filename_size == cur_item->filename.size &&
            memcmp(cur_item->filename.data, filename, filename_size) == 0) {
            return !(cur_item->hour == time->tm_hour && cur_item->minute == time->tm_min &&
                     cur_item->second == time->tm_sec);
        }
    }
    return true;
}
bool _fluild_rebuild(FluildFTime *ftime, const char *this_file, int argc, char **argv) {
    if (FluildFTime_file_changed(ftime, this_file)) {
        // remove old fluid if it exist than replace it by the new
        remove("fluild.old.exe");
        rename("fluild.exe", "fluild.old.exe");
        fluild_log(FLUILD_LOG_INFO, "renaming fluild.exe to fluild.old.exe");

        // rebuild the file
        SString cmd_rebuild = {0};
        fluild_cmd_append_many(&cmd_rebuild, "gcc", "-o", "fluild.exe", this_file);
        if (fluild_cmd_system(&cmd_rebuild) != 0) { return false; }

        // update save and unload ftime so when we reload the file this won't rerun indefinitely
        if (!FluildFTime_update(ftime, this_file)) { return false; }
        if (!FluildFTime_save(ftime, FLUILD_DEFAULT_FILENAME_FTIME)) { return false; }
        FluildFTime_unload(ftime);

        // rerun the build
        SString cmd_rerun = {0};
        fluild_cmd_append(&cmd_rerun, "fluild.exe");
        // append all args of argv
        for (int i = 0; i < argc - 1; ++i) { fluild_cmd_append(&cmd_rerun, argv[i + 1]); }

        fluild_log(FLUILD_LOG_INFO, "reruning itself");
        exit(fluild_cmd_system(&cmd_rerun));
    }
    return true;
}
#define fluild_rebuild(ftime, argc, argv) _fluild_rebuild(ftime, __FILE__, argc, argv)

#endif // FLUILD_H
