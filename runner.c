#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#define RED     "\033[31m"
#define BLUE     "\033[34m"

#define RESET   "\033[0m"

#define ERROR(M, ...) fprintf(stderr, RESET RED "[ERROR]" RESET " " M "\n", ##__VA_ARGS__);
#define DEBUG(M, ...) fprintf(stderr, RESET BLUE "[DEBUG]" RESET " " M "\n", ##__VA_ARGS__);

enum __exist_opt { CHECK_ANY, CHECK_FILE, CHECK_DIR };
int exists(char* path, enum __exist_opt opt) {
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

#ifdef DO_DEBUG
    DEBUG("path=%s \t is_dir=%d \t is_file=%d", path, is_dir, is_file);
#endif

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

int add_watch_recursively(char *path) {
}

int build(char *build_script) {
}

int run(char *target_binary) {
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Invalid number of arguments");
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

    return EXIT_SUCCESS;
}
