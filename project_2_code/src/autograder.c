#include "utils.h"

// Batch size is determined at runtime now
pid_t *pids;

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int curr_batch_size;      // At most batch_size executables will be run at once
int total_params;         // Total number of parameters to test - (argc - 2)

// Contains status of child processes (-1 for done, 1 for still running)
int *child_status;


// TODO (Change 3): Timeout handler for alarm signal - kill remaining running child processes
void timeout_handler(int signum) {
    if(signum == SIGALRM){
        for(int i = 0 ; i < curr_batch_size; i++){
            if(child_status[i] == 1){
                kill(pids[i], SIGKILL);
            }
        }

    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, char *input, int batch_idx) {
    #ifdef PIPE
        // TODO: Setup pipe
        int fd[2];
        int p = pipe(fd);
        if(p == -1){
            perror("pipes failed");
            exit(EXIT_FAILURE);
        }

    #endif

    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file
        char buffer[255];
        sprintf(buffer, "output/%s.%s", executable_name, input);
        // printf("%s\n", buffer);
        FILE *output = fopen(buffer, "w");
        if(output == -1){
            perror("opening file failed");
            exit(EXIT_FAILURE);
        }
        int file_fd = fileno(output);
        if(file_fd == -1) {
            perror("getting file descriptor failed");
            exit(EXIT_FAILURE);
        }
        if(dup2(file_fd, 1) == -1){
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        fclose(output);


        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC
            execl(executable_path, buffer, input, NULL);


        #elif REDIR
            //TODO: Redirect STDIN to input/<input>.in file
            char name[255];
            sprintf(name, "input/%s.in", input);
            int in = open(name, O_RDONLY);
            if(in == -1) {
                perror("input file failed to open");
                exit(EXIT_FAILURE);
            }
            if(dup2(in, STDIN_FILENO) == -1){ 
                perror("dup2 failed");
                exit(EXIT_FAILURE);
            }
            execl(executable_path, executable_name, NULL);

        #elif PIPE
            // TODO: Pass read end of pipe to child process
            char fdstring[10];
            sprintf(fdstring, "%d", fd[0]);
            printf("fdstring: %s\n", fdstring);
            execl(executable_path, executable_name, fdstring , NULL);
        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        #ifdef PIPE
            // TODO: Send input to child process via pipe
            close(fd[0]); //maybe this is an issue
            int input_value = atoi(input);
            write(fd[1], &input_value, sizeof(int));
            close(fd[1]);

        #endif
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char *param, int param_idx) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {

        int status;
        pid_t pid = waitpid(pids[j], &status, 0);

        // TODO: What if waitpid is interrupted by a signal?
        if (pid == -1 && errno == EINTR){
            //CHECK THIS
            results[tested - curr_batch_size + j].status[param_idx] = STUCK_OR_INFINITE;
            // j++;
            printf("SOR ");
            pid = waitpid(pids[j], &status, 0);
        }

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        if(exited && !signaled){
            char *exe =  results[tested - curr_batch_size + j].exe_path;
            FILE *output = fopen(exe, "r");
            char buffer[10];
            int num;
            if(fgets(buffer, sizeof(buffer), output) != NULL){
                sscanf(buffer, "%d", &num);
            }
            fclose(output); 
            if (num == 1){
                results[tested - curr_batch_size + j].status[param_idx] = CORRECT;
                printf("%d ", pid);
                printf("correct\n");
            }
            else if (num == 0){
                results[tested - curr_batch_size + j].status[param_idx] = INCORRECT;
                printf("%d ", pid);
                printf("incorrect\n");
            }
        }

        else if(signaled && WTERMSIG(status) == SIGSEGV){
            results[tested - curr_batch_size + j].status[param_idx] = SEGFAULT;
                printf("%d ", pid);
                printf("segfault\n");
        }

        else if(signaled && WTERMSIG(status) == SIGKILL){
            results[tested - curr_batch_size + j].status[param_idx] = STUCK_OR_INFINITE;
                printf("%d ", pid);
                printf("stuck or infinite\n");
        }
        // else if (signaled == 9) {         
        //     *(results[tested - curr_batch_size + j].status) = STUCK_OR_INFINITE;
        //     printf("stuck or infinite 2\n");
        // }    
        // TODO: Also, update the results struct with the status of the child process
        // *(results[tested - curr_batch_size + j].status) = exit_status;

        // NOTE: Make sure you are using the output/<executable>.<input> file to determine the status
        //       of the child process, NOT the exit status like in Project 1.
        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param);
        // Mark the process as finished
        child_status[j] = -1;
    }
    // cehck
    // struct itimerval end_timer;
    // end_timer.it_value.tv_sec = 0;
    // end_timer.it_value.tv_usec = 0;
    // end_timer.it_interval.tv_sec = 0;
    // end_timer.it_interval.tv_usec = 0;
    // if (setitimer(ITIMER_REAL, &end_timer, NULL) == -1) {
    //     perror("setitimer");
    //     exit(EXIT_FAILURE);
    // }
    free(child_status);
}



int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    // TODO (Change 0): Implement get_batch_size() function
    int batch_size = get_batch_size();

    char **executable_paths = get_student_executables(testdir, &num_executables);
    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    #ifdef REDIR
        // TODO: Create the input/<input>.in files and write the parameters to them
        create_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    // MAIN LOOP: For each parameter, run all executables in batch size chunks
    for (int i = 2; i < argc; i++) {
        int remaining = num_executables;
	    int tested = 0;
        // Test the parameter on each executable
        while (remaining > 0) {

            // Determine current batch size - min(remaining, batch_size)
            curr_batch_size = remaining < batch_size ? remaining : batch_size;
            pids = malloc(curr_batch_size * sizeof(pid_t));

            // TODO: Execute the programs in batch size chunks
            for (int j = 0; j < curr_batch_size; j++) {
                execute_solution(executable_paths[tested], argv[i], j);
		        tested++;
            }

            // TODO (Change 3): Setup timer to determine if child process is stuck
            start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

            // TODO: Wait for the batch to finish and check results
            monitor_and_evaluate_solutions(tested, argv[i], i - 2);
            // write_results_to_file(results, curr_batch_size, argc -2);


            // TODO: Cancel the timer if all child processes have finished
            // CHECK THIS ////////  
            int count = 0;
            for (int i = 0; i < curr_batch_size; i++){
                if (child_status[i] == -1) {
                    count++;
                }
            }
            if (count == curr_batch_size){
                cancel_timer();  // Implement this function (src/utils.c)
            }
            ///////////////////////

            // TODO Unlink all output files in current batch (output/<executable>.<input>)

            // UNCOMMENT AFTER TESTING
            // remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)


            // Adjust the remaining count after the batch has finished
            remaining -= curr_batch_size;
        }
    }

    #ifdef REDIR
        // TODO: Unlink all input files for REDIR case (<input>.in)
        remove_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < num_executables; i++) {
    printf("Executable Path: %s\n", results[i].exe_path);
    printf("Parameters Tested:\n");
    for (int j = 0; j < total_params; j++) {
        printf("%d ", results[i].params_tested[j]);
    }
    printf("\n");
    printf("Status:\n");
    for (int j = 0; j < total_params; j++) {
        printf("%d ", results[i].status[j]);
    }
    printf("\n");
    }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);

    free(pids);

    return 0;
}
