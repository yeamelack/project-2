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
    if(child_status == NULL)
    {
        return;
    }
    for(int i = 0 ; i < curr_batch_size; i++)
    {
        if(child_status[i] == 1)
        {
            kill(pids[i], SIGKILL);
        }
    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, char *input, int batch_idx) {
    #ifdef PIPE
        // TODO: Setup pipe
        if(int pipe(int fd[2]) == -1){
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
        FILE *output = fopen(buffer, "w");
        if(output == -1){
            perror("opening file failed");
            exit(EXIT_FAILURE);
        }
        if(dup2(output, 1) == -1){
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        fclose(output);


        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC
            execl(executable_path, buffer, input, NULL);


        #elif REDIR
            // TODO: Redirect STDIN to input/<input>.in file
            if(dup2(fd, 1) == -1){ // double check should be correct
                perror(EXIT_FAILURE);
            }
            close(fd); // closing the file descriptor


        #elif PIPE
            // TODO: Pass read end of pipe to child process
            // Double check
            close(fd[1]); // closing the write end of the pipr in the child that was opened in the parent
            read(fd[0], buffer, sizeof(buffer)); // Reading from the pipe here 
            close(fd[0]); // closing the read of the pipe when we are finished in the child process

        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        #ifdef PIPE
            // TODO: Send input to child process via pipe
            close(fd[0]);
            write(fd[1], input, sizeof(input)+1);
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
        if (pid == EINTR){
            status;
            pid = waitpid(pids[j], &status, 0);
        }

        // TODO: Determine if the child process finished normally, segfaulted, or timed out
        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);
        
        if (pid > 0){
            if (exited > 0 && signaled <= 0){
                if (exit_status == 0){
                    results[tested - curr_batch_size + j].status = CORRECT;
                }
                else if (exit_status == 1 && <= 0){
                    results[tested - curr_batch_size + j].status = INCORRECT;
                }
            }
            else if (signaled == 11){
                results[tested - curr_batch_size + j].status = SEGFAULT;
            }
        }
        else if (signaled == 9) {
           
            results[tested - curr_batch_size + j].status = STUCK_OR_INFINITE;
        }
        
        
        // TODO: Also, update the results struct with the status of the child process
        results[tested - curr_batch_size + j].status = exit_status;

        // NOTE: Make sure you are using the output/<executable>.<input> file to determine the status
        //       of the child process, NOT the exit status like in Project 1.


        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param);

        // Mark the process as finished
        child_status[j] = -1;
    }
    struct itimerval end_timer;
        if (setitimer(ITIMER_REAL, &end_timer, NULL) == -1) {
            perror("setitimer");
            exit(EXIT_FAILURE);
        }

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

            // TODO: Cancel the timer if all child processes have finished
            if (child_status == NULL) {
                cancel_timer();  // Implement this function (src/utils.c)
            }

            // TODO Unlink all output files in current batch (output/<executable>.<input>)
            remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)

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