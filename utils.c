#define _XOPEN_SOURCE 600
#include <pty.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>

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

void stream_input(int fd, const char* file) {
    FILE* input_file_fd = fopen(file, "r");
    if (!input_file_fd) {
        perror("stream_input.fopen@0,27");
        _exit(EINVAL);
    }

    char line[4096];
    while (fgets(line, sizeof(line), input_file_fd)) {
        ssize_t w = write(fd, line, strlen(line));
        if (w == -1) {
            // pty has terminated
            break;
        }
    }
    fclose(input_file_fd);

    // Clear any errors during the writes 
    // in case of early program termination
    errno = 0;
}

void handle_child_io(int master_fd, pid_t pid, int *status) {
    // nonblocking is optional; we'll use poll()
    struct pollfd fds[2];
    fds[0].fd = master_fd;     
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;  
    fds[1].events = POLLIN;

    char buf[4096];
    int child_exited = 0;

    while (!child_exited) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        // data from child (program output)
        if (fds[0].revents & POLLIN) {
            ssize_t n = read(master_fd, buf, sizeof(buf));
            if (n > 0) {
                // show to user's terminal
                ssize_t w = write(STDOUT_FILENO, buf, n);
                if (w == -1) {
                    perror("write");
                }
            } else if (n == 0) {
                // EOF from child pty
                // we'll wait for child exit below
                fds[0].events = 0;
            } else {
                if (errno != EIO && errno != EINTR) perror("read master_fd");
            }
        }

        // user typed something — forward to child
        if (fds[1].revents & POLLIN) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0) {
                ssize_t w = write(master_fd, buf, n);
                if (w == -1) {
                    perror("write");
                }

                // don't display the current typed input again
                w = read(master_fd, buf, sizeof(buf));
                if (w == -1) {
                    perror("read");
                }
            } else if (n == 0) {
                // user closed stdin (e.g., Ctrl-D)
                // optionally close master write side to signal EOF
                // but pty has single bidirectional fd; ignore
                fds[1].events = 0;
            } else {
                if (errno != EINTR) perror("read stdin");
            }
        }

        // check if child has exited
        pid_t w = waitpid(pid, status, WNOHANG);
        if (w == pid) {
            child_exited = 1;
            break;
        } else if (w == -1) {
            perror("[ FIXME ] wait_pid@handle_child_io");
            break;
        }
    }

    // read stdout from program display/capture
    while (1) {
        ssize_t n = read(master_fd, buf, sizeof(buf));
        if (n > 0) {
            write(STDOUT_FILENO, buf, n);
        } else {
            errno = 0;
            break;
        }
    }
}

int run(char *program, int use_input_file) {
    if (program == NULL || !strcmp(program, "\0")) {
        ERROR("please specify program");
        return -1;
    }

    do {
        // lazy clrscr() but idc
        system("clear");

        int master_fd;
        pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
        if (pid < 0) {
            perror("forkpty");
            return -1;
        }

        if (pid == 0) {
            // child: exec the target program (it sees a proper tty)
            char *args[] = {program, NULL};
            execvp(program, args);
            // if exec fails:
            perror("execvp");
            return -1;
        }
        INFO("Child running as pid %d", pid);

        if (use_input_file) {
            // stream input to pty
            stream_input(master_fd, "inputs.txt");
        }

        // handle io for child by 
        // displaying its stdout and passing input to stdin
        int status = 0;
        handle_child_io(master_fd, pid, &status);
        DEBUG("wait_pid exit status %d -> %d", status,  WEXITSTATUS(status));

        INFO("Child exited with exit status %d", WEXITSTATUS(status));
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            ERROR("%s exited with exit code %d. Exiting..", program, WEXITSTATUS(status));
            kill_child(&pid);
        }

        printf("\n...............................................\n");
        for (int i = RELOAD_TIME; i > 0; i--) {
            printf("%d\n", i);
            sleep(1);
        }
        printf("...............................................\n\n");
    } while (1);

    return 0;
}

[[deprecated]] int __run(char *file, int continuous) {
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
            if(err) {
                perror("execv");
                exit(err);
            }
            //redundant
            exit(0);
            // return 0;
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

int build(char *file) {
    if (!exists(file, CHECK_FILE)) {
        ERROR("File \"%s\" does not exist or is not a valid file", file);
        errno = ENOENT;
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // lazy clrscr() but idc
        system("clear");

        char *execv_args[] = {file, NULL};
        execv(file, execv_args);
        int err = errno;
        if(err) {
            perror("execv");
            exit(err);
        }
        exit(0);
        // return 0;
    }

    int wstatus;
    wait(&wstatus);
    INFO("Child exited with exit status %d", WEXITSTATUS(wstatus));
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        ERROR("%s exited with exit code %d. Exiting..", file, WEXITSTATUS(wstatus));
        exit(WEXITSTATUS(wstatus));
    }

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
        (void*)strncpy(*(split_array+counter), token, len);
        token = strtok(NULL, delim);
        counter += 1;
    }

    free(src_cpy);
    *dest = split_array;
    return counter;
}
