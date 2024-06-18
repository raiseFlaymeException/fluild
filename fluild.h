#ifndef FLUILD_H
#define FLUILD_H

#include <assert.h>
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

void SString_init(SString *str, size_t cap) {
    str->data = malloc(cap);
    str->cap  = cap;
    str->size = 0;
}
void SString_deinit(SString *str) { free(str->data); }
void SString_append(SString *str, char data) {
    if (str->size + 1 >= str->cap) {
        str->cap *= 2;
        str->data = realloc(str->data, str->cap);
    }
    str->data[str->size++] = data;
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

char *vasprintf(const char *format, va_list args) {
    int size = vsnprintf(NULL, 0, format, args) + 1;
    if (size < 0) { return NULL; }

    char *buf = malloc(size);
    if (!buf) { return NULL; }

    int err = vsnprintf(buf, size, format, args);
    return (err < 0 || size < err) ? NULL : buf;
}
char *asprintf(const char *format, ...) {
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

bool vfluild_log(FluildLogType log_type, const char *format, va_list args) {
    assert(log_type < FLUILD_LOG_END && "invalid log type");

    char *log_format = asprintf("%s: %s\n", FLUILD_LOG_FORMATS[log_type], format);
    if (!log_format) { return false; }

    int result = vprintf(log_format, args);

    free(log_format);
    return result >= 0;
}
void fluild_log(FluildLogType log_type, const char *format, ...) {
    va_list args;
    va_start(args, format);

    vfluild_log(log_type, format, args);

    va_end(args);
}

typedef struct {
    SStringView *cmd;
    size_t       size;
    size_t       cap;
} FluildCmd;

#define FLUILD_CMD_DEFAULT_CAP     16
#define FLUILD_CMD_STR_DEFAULT_CAP 64

bool FluildCmd_init(FluildCmd *fluild_cmd, size_t cap) {
    assert(cap >= 1 && "capacity must be greater than one");
    fluild_cmd->cmd = malloc(cap * sizeof(*fluild_cmd));
    if (!fluild_cmd->cmd) { return false; }

    fluild_cmd->cap  = cap;
    fluild_cmd->size = 0;

    return true;
}
void FluildCmd_deinit(FluildCmd *fluild_cmd) {
    if (fluild_cmd->cmd) { free(fluild_cmd->cmd); }
}
bool FluildCmd_append(FluildCmd *fluild_cmd, const char *cmd) {
    // TODO: check for special chars
    if (fluild_cmd->size + 1 >= fluild_cmd->cap) {
        fluild_cmd->cap *= 2;
        fluild_cmd->cmd = realloc(fluild_cmd->cmd, fluild_cmd->cap);
        if (!fluild_cmd->cmd) { return false; }
    }
    fluild_cmd->cmd[fluild_cmd->size].data   = cmd;
    fluild_cmd->cmd[fluild_cmd->size++].size = strlen(cmd);
    return true;
}
bool FluildCmd_vappend_many(FluildCmd *fluild_cmd, va_list args) {
    const char *to_append;
    while ((to_append = va_arg(args, const char *))) {
        if (!FluildCmd_append(fluild_cmd, to_append)) { return false; }
    }
    return true;
}
bool _FluildCmd_append_many(FluildCmd *fluild_cmd, ...) {
    va_list args;
    va_start(args, fluild_cmd);

    bool result = FluildCmd_vappend_many(fluild_cmd, args);

    va_end(args);
    return result;
}
#define FluildCmd_append_many(...) _FluildCmd_append_many(__VA_ARGS__, NULL)

int FluildCmd_system(const FluildCmd *fluild_cmd) {
    // TODO: make SString in easy_c_data_structure possible to fail when allocating
    // TODO: put SString_join in easy_c_data_structure
    SString str_cmd;
    SString_init(&str_cmd, FLUILD_CMD_STR_DEFAULT_CAP);

    SString_join(&str_cmd, fluild_cmd->cmd, fluild_cmd->size);
    SString_append(&str_cmd, '\0');

    fluild_log(FLUILD_LOG_INFO, "CMD %s", str_cmd.data);
    int result = system(str_cmd.data);

    SString_deinit(&str_cmd);
    return result;
}

int FluildCmd_exec(const FluildCmd *fluild_cmd, SString *result) {
    SString str_cmd;
    SString_init(&str_cmd, FLUILD_CMD_STR_DEFAULT_CAP);

    SString_join(&str_cmd, fluild_cmd->cmd, fluild_cmd->size);
    SString_append(&str_cmd, '\0');

    fluild_log(FLUILD_LOG_INFO, "CMD %s", str_cmd.data);

    FILE *cmd = popen(str_cmd.data, "r");
    SString_deinit(&str_cmd);

    if (!SString_getc_until(result, cmd, EOF)) { return false; }
    return pclose(cmd);
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
    size_t      hour;
    size_t      minute;
    size_t      second;
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
    SString_init(&ftime->full_file, FLUILD_FILE_TIME_DEFAULT_CAP);

    FILE *f = fopen(file_changed, "r");
    if (!f) { return false; }

    if (!SString_getc_until(&ftime->full_file, f, EOF)) { return false; }

    if (fclose(f) != 0) { return false; }

    // allocate items
    ftime->size  = SStringAll_countc((SStringAll *)&ftime->full_file, '\n', 0);
    ftime->items = malloc(ftime->size * sizeof(FluildFTimeItem));

    // parse items
    long long offset_new_line = 0;
    for (size_t i = 0; i < ftime->size; ++i) {
        // find next_colon
        long long next_colon =
            SStringAll_findc((SStringAll *)&ftime->full_file, ':', offset_new_line);
        if (next_colon < 0) { return false; }

        // create filename:
        // ...\nfilename:...
        SStringView_init_substr(&ftime->items[i].filename, (SStringAll *)&ftime->full_file,
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
        printf("hour: %.*s\n", buf.size, buf.data);
        ftime->items[i].hour = strtoll(buf.data, NULL, 10);

        // find next colon
        old_colon  = next_colon + 1;
        next_colon = SStringAll_findc((SStringAll *)&ftime->full_file, ':', next_colon + 1);
        if (next_colon < 0) { return false; }

        // create minute
        // ...:minute:...
        SStringView_init_substr(&buf, (SStringAll *)&ftime->full_file, old_colon,
                                next_colon - old_colon);
        ftime->items[i].minute = strtoll(buf.data, NULL, 10);

        // find next new line
        offset_new_line = SStringAll_findc((SStringAll *)&ftime->full_file, '\n', next_colon + 1);
        if (offset_new_line < 0) { return false; }

        // create second
        // ...:second\n...
        SStringView_init_substr(&buf, (SStringAll *)&ftime->full_file, next_colon + 1,
                                offset_new_line);
        ftime->items[i].second = strtoll(buf.data, NULL, 10);

        // consume the new line
        ++offset_new_line;
    }

    return true;
}
bool FluildFTime_save(FluildFTime *ftime, const char *file_changed) {
    FILE *f = fopen(file_changed, "w");
    if (!f) { return false; }

    for (size_t i = 0; i < ftime->size; ++i) {
        FluildFTimeItem *cur_item = ftime->items + i;
        if (fprintf(f, "%.*s:%02d:%02d%02d\n", cur_item->filename.size, cur_item->filename.data,
                    cur_item->hour, cur_item->minute, cur_item->second) == EOF) {
            return false;
        }
    }

    return fclose(f) == 0;
}
void FluildFTime_clear(FluildFTime *ftime) {
    // deinit the entire file so this will deinit all SStringView
    SString_deinit(&ftime->full_file);

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
bool FluildFTime_since_last(const char *filename, const char *file_changed) {
    (void)filename;
    (void)file_changed;
    assert(0 && "not implemented");
    return false;
}

#endif // FLUILD_H
