#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

#define RELOAD_TIME 10

struct arg_opts {
    char **exclude_list;
    char **include_list;
    size_t exclude_size;
    size_t include_size;
};

int parse_args(const int argc, char *argv[], struct arg_opts *opts) {
    int opt;
    
    // Initialize with default values
    opts->exclude_list = 0;
    opts->include_list = 0;
    opts->exclude_size = -1;
    opts->include_size = -1;

    while ((opt = getopt(argc, argv, "e:i:")) != -1) {
        switch (opt) {
            case 'e':
                opts->exclude_size = split_string(optarg, &opts->exclude_list, 
                                                  strlen(optarg), ",");
                for (size_t i = 0 ; i < opts->exclude_size ; i++) 
                    VALIDATE_PATH(opts->exclude_list[i]);

                if (opts->include_size != -1)
                    FREE_2D(opts->include_list, opts->include_size);

                opts->include_list = 0;
                opts->include_size = -1;
                break;

            case 'i':
                opts->include_size = split_string(optarg, &opts->include_list,
                                                  strlen(optarg), ",");
                for (size_t i = 0 ; i < opts->include_size ; i++) 
                    VALIDATE_PATH(opts->include_list[i]);

                if (opts->exclude_size != -1)
                    FREE_2D(opts->exclude_list, opts->exclude_size);

                opts->exclude_list = 0;
                opts->exclude_size = -1;
                break;

            case '?':
                ERROR("Unknown option: %c\n", optopt);
                exit(EXIT_FAILURE);
                break;

            default:
                break;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc != 4 && argc != 6) {
        INFO("Usage: " "hot-reload <dir to watch> <build script> <target binary> [-n <count>]");
        return EXIT_FAILURE;
    }

    char *watch_dir = argv[1];
    char *build_script = argv[2];
    char *target_binary = argv[3];
    
    // Parse optional -n flag
    int max_runs = -1; // -1 means infinite (default behavior)
    if (argc == 6) {
        if (strcmp(argv[4], "-n") == 0) {
            max_runs = atoi(argv[5]);
            if (max_runs <= 0) {
                ERROR("Invalid count for -n flag: must be a positive integer");
                return EXIT_FAILURE;
            }
            INFO("Will run for maximum %d rebuild cycles", max_runs);
        } else {
            ERROR("Unknown flag: %s", argv[4]);
            INFO("Usage: " "hot-reload <dir to watch> <build script> <target binary> [-n <count>]");
            return EXIT_FAILURE;
        }
    }

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

    const int fd = add_watch_recursive(watch_dir);
    int modified = 1;
    int run_count = 0;

    pid_t child_proccess = -1;
    do {
        if (modified) {
            if (child_proccess != -1 && child_proccess != 0) {
                DEBUG("KILLING PREVIOUS BINARY", modified, child_proccess);
                kill_child(&child_proccess);
            }

            DEBUG("File modified. Running build_script");
            int retval = run(build_script, 0);
            if (retval == -1) {
                exit(EXIT_FAILURE);
            }
            
            // Increment run counter after successful rebuild
            run_count++;
            if (max_runs > 0) {
                INFO("Rebuild cycle %d/%d completed", run_count, max_runs);
            } else {
                INFO("Rebuild cycle %d completed", run_count);
            }
            
            // Check if we've reached the maximum number of runs
            if (max_runs > 0 && run_count >= max_runs) {
                INFO("Reached maximum run count (%d). Exiting.", max_runs);
                if (child_proccess != -1 && child_proccess != 0) {
                    kill(child_proccess, SIGKILL);
                    signal(SIGCHLD, SIG_IGN);
                }
                break;
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
