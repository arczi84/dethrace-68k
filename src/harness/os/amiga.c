#include "harness/config.h"
#include "harness/os.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static DIR* directory_iterator;
static char name_buf[1024];

void OS_InstallSignalHandler(char* program_name) {
}

void OS_RemoveSignalHandler(void) {
}

char* OS_GetFirstFileInDirectory(char* path) {
    directory_iterator = opendir(path);
    if (directory_iterator == NULL) {
        return NULL;
    }
    return OS_GetNextFileInDirectory();
}

char* OS_GetNextFileInDirectory(void) {
    struct dirent* entry;

    if (directory_iterator == NULL) {
        return NULL;
    }
    while ((entry = readdir(directory_iterator)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            return entry->d_name;
        }
    }
    closedir(directory_iterator);
    directory_iterator = NULL;
    return NULL;
}

FILE* OS_fopen(const char* pathname, const char* mode) {
    FILE* f;
    char directory[512];
    char filename[512];
    char resolved[1024];
    char* dirname_part;
    char* basename_part;
    DIR* dir;
    struct dirent* entry;

    f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }

    strncpy(directory, pathname, sizeof(directory) - 1);
    directory[sizeof(directory) - 1] = '\0';
    strncpy(filename, pathname, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    dirname_part = dirname(directory);
    basename_part = basename(filename);
    dir = opendir(dirname_part);
    if (dir == NULL) {
        return NULL;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcasecmp(basename_part, entry->d_name) == 0) {
            snprintf(resolved, sizeof(resolved), "%s/%s", dirname_part, entry->d_name);
            f = fopen(resolved, mode);
            break;
        }
    }
    closedir(dir);
    if (f == NULL && harness_game_config.verbose) {
        fprintf(stderr, "Failed to open \"%s\" (%s)\n", pathname, strerror(errno));
    }
    return f;
}

size_t OS_ConsoleReadPassword(char* buffer, size_t buffer_len) {
    size_t len;
    if (buffer_len == 0 || fgets(buffer, buffer_len, stdin) == NULL) {
        return 0;
    }
    len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n')) {
        buffer[--len] = '\0';
    }
    return len;
}

char* OS_Dirname(const char* path) {
    strncpy(name_buf, path, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    return dirname(name_buf);
}

char* OS_Basename(const char* path) {
    strncpy(name_buf, path, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    return basename(name_buf);
}

char* OS_GetWorkingDirectory(char* argv0) {
    return "PROGDIR:";
}

int OS_GetPrefPath(char* dest, char* app) {
    strcpy(dest, "PROGDIR:");
    return 0;
}

int OS_GetAdapterAddress(char* name, void* sockaddr_in) {
    return 0;
}

int OS_InitSockets(void) {
    return 0;
}

int OS_GetLastSocketError(void) {
    return errno;
}

void OS_CleanupSockets(void) {
}

int OS_SetSocketNonBlocking(int socket) {
    int flags = fcntl(socket, F_GETFL);
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

int OS_CloseSocket(int socket) {
    return close(socket);
}
