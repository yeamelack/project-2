#include "utils.h"

// Run the (executable, parameter) pairs in batches of 8 to avoid timeouts due to 
// having too many child processes running at once
#define PAIRS_BATCH_SIZE 8

typedef struct {
    char *executable_path;
    int parameter;
    int status;
} pairs_t;

// Store the pairs tested by this worker and the results
pairs_t *pairs;

// Information about the child processes and their results
pid_t *pids;
int *child_status;     // Contains status of child processes (-1 for done, 1 for still running)

int curr_batch_size;   // At most PAIRS_BATCH_SIZE (executable, parameter) pairs will be run at once
long worker_id;        // Used for sending/receiving messages from the message queue


// TODO: Timeout handler for alarm signal - should be the same as the one in autograder.c
void timeout_handler(int signum) {
    if(child_status == NULL){
        return;
    }
    for(int i = 0 ; i < curr_batch_size; i++){
        if(child_status[i] == 1){
            kill(pids[i], SIGKILL);
        }
    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, int param, int batch_idx) {
 
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);

        // TODO: Redirect STDOUT to output/<executable>.<input> file
        char buffer[255];
        sprintf(buffer, "output/%s.%d", executable_name, param);
        printf("buffer is %s\n", buffer);
        FILE *output = fopen(buffer, "w");
        if(output == -1){
            perror("opening file failed");
            exit(EXIT_FAILURE);
        }
        int fd = fileno(output);
        if(dup2(fd, 1) == -1){
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }
        fclose(output);

        // TODO: Input to child program can be handled as in the EXEC case (see template.c)
        pids[batch_idx] = param;
        printf("%s, %s, %d\n", executable_path, buffer, param);
        
        execl(executable_path, buffer, param, NULL);

        
        perror("Failed to execute program in worker");
        exit(1);
    }
    // Parent process
    else if (pid > 0) {
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int finished) {
    // Keep track of finished processes for alarm handler
    //THIS CAUSES MALLOC ERROR CHECK
    child_status = malloc(finished * sizeof(int));
    for (int j = 0; j < finished; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < finished; j++) {
        char *current_exe_path = pairs[finished + j].executable_path;
        int current_param = pairs[finished + j].parameter;

        int status;
        pid_t pid = waitpid(pids[j], &status, 0);

        // TODO: What if waitpid is interrupted by a signal?
        if (pid == -1 && errno == EINTR){
            //CHECK THIS
            pairs[finished + j].status = STUCK_OR_INFINITE;
            pid = waitpid(pids[j], &status, 0);
        }

        int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        // TODO: Check if the process finished normally, segfaulted, or timed out and update the 
        //       pairs array with the results. Use the macros defined in the enum in utils.h for 
        //       the status field of the pairs_t struct (e.g. CORRECT, INCORRECT, SEGFAULT, etc.)
        //       This should be the same as the evaluation in autograder.c, just updating `pairs` 
        //       instead of `results`.
        if(exited && !signaled){
            char *exe =  pairs[finished + j].executable_path;
            FILE *output = fopen(exe, "r");
            char buffer[10];
            int num;
            if(fgets(buffer, sizeof(buffer), output) != NULL){
                sscanf(buffer, "%d", &num);
            }
            fclose(output); 
            if (num == 1){
                pairs[finished + j].status = CORRECT;
                printf("%d ", pid);
                printf("correct\n");
            }
            else if (num == 0){
                pairs[finished + j].status = INCORRECT;
                printf("%d ", pid);
                printf("incorrect\n");
            }
        }

        else if(signaled && WTERMSIG(status) == SIGSEGV){
            pairs[finished + j].status = SEGFAULT;
                printf("%d ", pid);
                printf("segfault\n");
        }

        else if(signaled && WTERMSIG(status) == SIGKILL){
            pairs[finished + j].status = STUCK_OR_INFINITE;
                printf("%d ", pid);
                printf("stuck or infinite\n");
        }

        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}


// Send results for the current batch back to the autograder
// VERY WRONG
void send_results(int msqid, long mtype, int finished) {
    // Format of message should be ("%s %d %d", executable_path, parameter, status)

}


// Send DONE message to autograder to indicate that the worker has finished testing
void send_done_msg(int msqid, long mtype) {
    msgbuf_t msg_buf;
    msg_buf.mtype = mtype;
    strcpy(msg_buf.mtext, "DONE");

    if (msgsnd(msqid, &msg_buf, strlen(msg_buf.mtext) + 1, 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <msqid> <worker_id>\n", argv[0]);
        return 1;
    }

    int msqid = atoi(argv[1]);
    worker_id = atoi(argv[2]);

    // TODO: Receive initial message from autograder specifying the number of (executable, parameter) 
    // pairs that the worker will test (should just be an integer in the message body). (mtype = worker_id)

    //CHECK
    msgbuf_t msg;
    if (msgrcv(msqid, &msg, sizeof(msg.mtext), worker_id, 0) == -1) {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }
    // printf("recieved msg 1st chk %s\n", msg.mtext);

    // TODO: Parse message and set up pairs_t array
    int pairs_to_test = atoi(msg.mtext);
    
    // fprintf("%d", pairs_to_test);
    //pairs_t *pairs;
    pairs = malloc(pairs_to_test * sizeof(pairs_t));
    for (int i = 0; i < pairs_to_test; i++) {
        pairs[i].executable_path = malloc(100); //executable_paths[i]; NEED TO GET EXECUTABLES
        // pairs[i].parameter = malloc((pairs_to_test) * sizeof(int));
        // pairs[i].status = malloc((pairs_to_test) * sizeof(int));
    }

    // TODO: Receive (executable, parameter) pairs from autograder and store them in pairs_t array.
    //       Messages will have the format ("%s %d", executable_path, parameter). (mtype = worker_id)
   
    for(int i = 0; i < pairs_to_test; i++) {
        // printf("pairs to test %d, i is %d\n", pairs_to_test, i);
        if (msgrcv(msqid, &msg, sizeof(msg.mtext), worker_id, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        // printf("recieved msg 2nd chk %s\n", msg.mtext);
        // CHECK THIS FOR MALLOC ERROR 
        sscanf(msg.mtext, "%s %d",pairs[i].executable_path, &pairs[i].parameter);
       //strncpy(pairs[i].executable_path, msg.mtext, sizeof(msg.mtext));
        // printf("recieved: %s\n", pairs[i].executable_path);
    }
    // printf("done receiving executables\n");
    // TODO: Send ACK message to mq_autograder after all pairs received (mtype = BROADCAST_MTYPE)
    msgbuf_t ack;
    ack.mtype = BROADCAST_MTYPE;
    strcpy(ack.mtext, "ACK");
    if (msgsnd(msqid, &ack, sizeof(ack.mtext), 0) == -1){
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    printf("%s got sent \n", ack.mtext);


    
    // TODO: Wait for SYNACK from autograder to start testing (mtype = BROADCAST_MTYPE).
    //       Be careful to account for the possibility of receiving ACK messages just sent.
    
    while(1){
        msgbuf_t syn;
        syn.mtype = BROADCAST_MTYPE;
        if (msgrcv(msqid, &syn, sizeof(syn.mtext), BROADCAST_MTYPE, 0) == -1){
            perror("synack msgrcv\n");
        }
        if (strcmp(syn.mtext, "SYNACK") == 0){
            printf("recieved SYNACK\n");
            break;
        }
        else{
            if(msgsnd(msqid, &syn, sizeof(syn.mtext), 0) == -1){
                exit(1);
            }

        }
    }

    // Run the pairs in batches of 8 and send results back to autograder
    for (int i = 0; i < pairs_to_test; i+= PAIRS_BATCH_SIZE) {
        int remaining = pairs_to_test - i;
        curr_batch_size = remaining < PAIRS_BATCH_SIZE ? remaining : PAIRS_BATCH_SIZE;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        for (int j = 0; j < curr_batch_size; j++) {
            // TODO: Execute the student executable
            execute_solution(pairs[i + j].executable_path, pairs[i + j].parameter, j);
        }

        // TODO: Setup timer to determine if child process is stuck
        start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

        // TODO: Wait for the batch to finish and check results
        monitor_and_evaluate_solutions(i);

        // TODO: Cancel the timer if all child processes have finished
        int count = 0;
        for (int i = 0; i < curr_batch_size; i++){
            if (child_status[i] == -1) {
                count++;
            }
        }
        if (count == curr_batch_size){
            cancel_timer();  // Implement this function (src/utils.c)
        }
        // TODO: Send batch results (intermediate results) back to autograder
        send_results(msqid, worker_id, i);

        free(pids);
    }

    // TODO: Send DONE message to autograder to indicate that the worker has finished testing
    send_done_msg(msqid, worker_id);

    // Free the pairs_t array
    for (int i = 0; i < pairs_to_test; i++) {
        free(pairs[i].executable_path);
    }
    free(pairs);

    // THIS WAS GIVEN BUT IS GIVING ERRORS 
    free(pids);
}