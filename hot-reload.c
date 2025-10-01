#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#define RELOAD_TIME 10

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + NAME_MAX + 1))

#define RED     "\033[31m"
#define BLUE    "\033[34m"
#define GREEN   "\033[32m"

#define RESET   "\033[0m"

#ifdef DO_DEBUG
#define DEBUG(M, ...) \
    fprintf(stderr, RESET BLUE "[ERROR] " M RESET "\n", ##__VA_ARGS__)
#else
#define DEBUG(M, ...) ;
#endif

#define ERROR(M, ...) fprintf(stderr, RESET RED "[DEBUG] " M RESET "\n", ##__VA_ARGS__)
#define INFO(M, ...) fprintf(stderr, RESET GREEN "[INFO] " M RESET "\n", ##__VA_ARGS__)

enum __exist_opt { CHECK_ANY, CHECK_FILE, CHECK_DIR };
int exists(const char* path, enum __exist_opt opt) {
    int is_dir = 0, is_file = 0;
    struct stat buf;
    
    int retval = stat(path, &buf);
    if (retval == -1) {
        int err = errno;
        perror("stat");
        return err;
    }

    is_dir = S_ISDIR(buf.st_mode);
    if (!is_dir) is_file = S_ISREG(buf.st_mode);

    DEBUG("path=%s \t is_dir=%d \t is_file=%d", path, is_dir, is_file);

    if (opt == CHECK_ANY) {
        return is_dir | is_file;
    } else if (opt == CHECK_DIR) {
        return is_dir;
    } else if (opt == CHECK_FILE) {
        return is_file;
    } else {
        errno = EINVAL;
    }

    return -1;
}

int __add_watch_recursive(int fd, const char* path) {
    DIR *dir = opendir(path);
    if (!dir) {
        ERROR("%s is not a directory", path);
        exit(EXIT_FAILURE);
    };

    int wd = inotify_add_watch(fd, path,
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        ERROR("Failed to add watch for %s: %s\n", path, strerror(errno));
        return -1;
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
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    __add_watch_recursive(fd, path);

    return fd;
}

int run(char *file, int continuous) {
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

            execv(file, NULL);
            int err = errno;
            perror("execv");
            exit(errno);
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
    } while (continuous);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        INFO("Usage: " "hot-reload <dir to watch> <build script> <target binary>");
        return EXIT_FAILURE;
    }

    char *watch_dir = argv[1];
    char *build_script = argv[2];
    char *target_binary = argv[3];

    if (!exists(watch_dir, CHECK_DIR)) {
        ERROR("Directory \"%s\" does not exist", watch_dir);
        exit(EXIT_FAILURE);
    }

    if (!exists(build_script, CHECK_FILE)) {
        ERROR("File \"%s\" does not exist", build_script);
        exit(EXIT_FAILURE);
    }

    if (!exists(target_binary, CHECK_FILE)) {
        ERROR("File \"%s\" does not exist", target_binary);
        exit(EXIT_FAILURE);
    }

    int fd = add_watch_recursive(watch_dir);
    int modified = 1;

    pid_t child_proccess = -1;
    do {
        if (modified) {
            if (child_proccess != -1 && child_proccess != 0) {
                DEBUG("KILLING PREVIOUS BINARY", modified, child_proccess);
                kill(child_proccess, SIGKILL);
                signal(SIGCHLD, SIG_IGN);
                child_proccess = -1;
            }

            DEBUG("File modified. Running build_script");
            int retval = run(build_script, 0);
            if (retval == -1) {
                exit(EXIT_FAILURE);
            }
        }

        if (child_proccess == -1) {
            child_proccess = fork();
        }

        // Child process to run the application
        if (child_proccess == 0) {
            int retval = run(target_binary, 1);
            if (retval == -1) exit(EXIT_FAILURE);
            else exit(EXIT_SUCCESS);
        }

        modified = 0;

        char event_buf[BUF_LEN] = {0};
        int length = read(fd, event_buf, BUF_LEN);

        if (length < 0) {
            if (errno == EAGAIN) {
                usleep(100000); // poll interval (100ms)
                continue;
            } else {
                perror("read");
                exit(EXIT_FAILURE);
            }
        }

        struct inotify_event *event = (struct inotify_event *) &event_buf[0];
        // source file was modified. build the project again
        if (event->len) {
            modified = 1;
            DEBUG("File modified");
        }
    } while(1);

    return EXIT_SUCCESS;
}
