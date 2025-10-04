#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <limits.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + NAME_MAX + 1))

#define RED     "\033[31m"
#define BLUE    "\033[34m"
#define GREEN   "\033[32m"

#define RESET   "\033[0m"

#ifdef DO_DEBUG
#define DEBUG(M, ...) \
    fprintf(stderr, RESET BLUE "[DEBUG] " M RESET "\n", ##__VA_ARGS__)
#else
#define DEBUG(M, ...) ;
#endif

#define ERROR(M, ...) fprintf(stderr, RESET RED "[ERROR] " M RESET "\n", ##__VA_ARGS__)
#define INFO(M, ...) fprintf(stderr, RESET GREEN "[INFO] " M RESET "\n", ##__VA_ARGS__)

#define MALLOC_2D(arr, r, c) \
    arr = (char**) malloc(sizeof(char*)*r); \
    for (size_t i = 0 ; i < r ; i++) *(arr+i) = (char*) malloc(sizeof(char) * c);

#define FREE_2D(arr, r) \
    for (size_t i = 0 ; i < r ; i++) free(arr[i]); \
    free(arr);

#define VALIDATE_PATH(p) \
    if (strlen(p) >= PATH_MAX) ERROR("Path is too big");

enum __exist_opt { CHECK_ANY, CHECK_FILE, CHECK_DIR };

int exists(const char*, enum __exist_opt);
void __add_watch_recursive(int, const char*);
int add_watch_recursive(const char*);
int run(char*, int);
void kill_child(int*);
size_t split_string(const char*, char***, size_t, const char*);

#endif
