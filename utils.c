#include "utils.h"

#define RELOAD_TIME 10
int exists(const char* path, const enum __exist_opt opt) {
    int is_dir = 0, is_file = 0;
    struct stat buf;

    const int retval = stat(path, &buf);
    if (retval == -1) {
        const int err = errno;
        perror("stat");
        return err;
    }

    is_dir = S_ISDIR(buf.st_mode);
    if (!is_dir) is_file = S_ISREG(buf.st_mode);

    DEBUG("path=%s \t is_dir=%d \t is_file=%d", path, is_dir, is_file);

    if (opt == CHECK_ANY) {
        return is_dir | is_file;
    }
    if (opt == CHECK_DIR) {
        return is_dir;
    }
    if (opt == CHECK_FILE) {
        return is_file;
    }
    errno = EINVAL;

    return -1;
}

void __add_watch_recursive(const int fd, const char* path) {
    DIR *dir = opendir(path);
    if (!dir) {
        ERROR("%s is not a directory", path);
        exit(EXIT_FAILURE);
    };

    int wd = inotify_add_watch(fd, path,
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        ERROR("Failed to add watch for %s: %s\n", path, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {

            char subpath[PATH_MAX];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
            __add_watch_recursive(fd, subpath);
        }
    }

    closedir(dir);
}

int add_watch_recursive(const char *path) {
    const int fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    __add_watch_recursive(fd, path);

    return fd;
}

int run(char *file, const int continuous) {
    if (!exists(file, CHECK_FILE)) {
        ERROR("File \"%s\" does not exist or is not a valid file", file);
        errno = ENOENT;
        return -1;
    }

    do {
        pid_t pid = fork();
        if (pid == 0) {
            // lazy clrscr() but idc
            system("clear");

            char *execv_args[] = {file, NULL};
            execv(file, execv_args);
            int err = errno;
            perror("execv");
            exit(err);
        }

        int wstatus;
        wait(&wstatus);
        INFO("Child exited with exit status %d", WEXITSTATUS(wstatus));
        if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
            ERROR("%s exited with exit code %d. Exiting..", file, WEXITSTATUS(wstatus));
            exit(WEXITSTATUS(wstatus));
        }

        if (!continuous) break;

        printf("\n...............................................\n");
        for (int i = RELOAD_TIME; i > 0; i--) {
            printf("%d\n", i);
            sleep(1);
        }
        printf("...............................................\n\n");
    } while (1);

    return EXIT_SUCCESS;
}

void kill_child(int *child_proccess) {
    kill(*child_proccess, SIGKILL);
    signal(SIGCHLD, SIG_IGN);
    *child_proccess = -1;
}

/* 
 * function to split a string into an array as per the delimiter
 * Expects the caller to call free() on the returned value
 */
size_t split_string(const char* src, char ***dest, size_t len, const char* delim) {
    if (src == NULL) {
        return 0;
    }

    char **split_array;
    MALLOC_2D(split_array, len, len);

    char *src_cpy = (char*) malloc(sizeof(char) * len);
    memcpy(src_cpy, src, len);

    size_t counter = 0;
    char *token = strtok(src_cpy, delim);
    while (token) {
        DEBUG("token = %s", token);
        char *_ = strncpy(*(split_array+counter), token, strlen(token));
        token = strtok(NULL, delim);
        counter += 1;
    }

    free(src_cpy);
    *dest = split_array;
    return counter;
}
