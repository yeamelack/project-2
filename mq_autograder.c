#include "utils.h"

pid_t *workers;          // Workers determined by batch size
int *worker_done;        // 1 for done, 0 for still running

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int total_params;         // Total number of parameters to test - (argc - 2)
int num_workers;          // Number of workers to spawn


void launch_worker(int msqid, int pairs_per_worker, int worker_id) {
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        // TODO: exec() the worker program and pass it the message queue id and worker id.
        //       Use ./worker as the path to the worker program.

        //CHECK

        char msqid_str[10]; // Buffer to hold msqid as string
        char worker_id_str[10]; // Buffer to hold worker_id as string

        // Convert msqid and worker_id to strings
        snprintf(msqid_str, sizeof(msqid_str), "%d", msqid);
        snprintf(worker_id_str, sizeof(worker_id_str), "%d", worker_id);

        // Execute the worker program with msqid and worker_id as arguments
        execl("./worker", msqid_str, worker_id_str, NULL);
        // execl("./worker", msqid, worker_id, NULL);
        // execl("./worker", worker_id, NULL);

        perror("Failed to spawn worker");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        // TODO: Send the total number of pairs to worker via message queue (mtype = worker_id)
        // char buffer[255];
        // msgsnd(worker_id, buffer, sizeof(buffer), IPC_NOWAIT); // May need to recheck prolly did it wrong

        //CHECK
        // msgsnd(worker_id, pairs_per_worker, sizeof(pairs_per_worker), IPC_NOWAIT); // May need to recheck prolly did it wrong
        struct my_msg  {
        long type;
        char text[100];
        } msg;

        char pairs_str[10];
        snprintf(pairs_str, sizeof(pairs_str), "%d", pairs_per_worker);
        msg.type = worker_id; // Use worker_id as the message type for identification
        strncpy(msg.text, pairs_str, sizeof(msg.text) - 1); 
        msg.text[sizeof(msg.text) - 1] = '\0';
        // Send the message
        if (msgsnd(msqid, &msg, sizeof(msg), IPC_NOWAIT) == -1) {
            perror("msgsnd failed");
        }

        // Store the worker's pid for monitoring
        workers[worker_id - 1] = pid;
    }
    // Fork failed 
    else {
        perror("Failed to fork worker");
        exit(1);
    }
}


// TODO: Receive ACK from all workers using message queue (mtype = BROADCAST_MTYPE)
void receive_ack_from_workers(int msqid, int num_workers) {
    struct Message {
    long mtype; 
    char data[100]; 
    };
    struct Message buf;
    int received_acks = 0;

    // Loop until acknowledgments are received from all workers
    while (received_acks < num_workers) {
        // Receive a message from the message queue
        if (msgrcv(msqid, &buf, sizeof(buf), BROADCAST_MTYPE, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        // Check if the received message is an acknowledgment
        if (buf.mtype == BROADCAST_MTYPE) {
            // Process the acknowledgment
            printf("Received ACK from worker %ld\n", buf.mtype);
            received_acks++;
        }
    }

}


// TODO: Send SYNACK to all workers using message queue (mtype = BROADCAST_MTYPE)
void send_synack_to_workers(int msqid, int num_workers) {
    struct Message {
    long mtype; 
    char data[100]; 
    };
    struct Message buf;
    int worker_id;

    // Prepare the SYNACK message
    buf.mtype = BROADCAST_MTYPE;
    // Add other fields or data to the message if needed

    // Send SYNACK to each worker
    for (worker_id = 1; worker_id <= num_workers; worker_id++) {
        // Send the message
        if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        printf("SYNACK sent to worker %d\n", worker_id);
    }
}


// Wait for all workers to finish and collect their results from message queue
void wait_for_workers(int msqid, int pairs_to_test, char **argv_params) {
    int received = 0;
    worker_done = malloc(num_workers * sizeof(int));
    for (int i = 0; i < num_workers; i++) {
        worker_done[i] = 0;
    }

    while (received < pairs_to_test) {
        for (int i = 0; i < num_workers; i++) {
            if (worker_done[i] == 1) {
                continue;
            }

            // Check if worker has finished
            pid_t retpid = waitpid(workers[i], NULL, WNOHANG);
            
            int msgflg;
            if (retpid > 0)
                // Worker has finished and still has messages to receive
                msgflg = 0;
            else if (retpid == 0)
                // Worker is still running -> receive intermediate results
                msgflg = IPC_NOWAIT;
            else {
                // Error
                perror("Failed to wait for child process");
                exit(1);
            }

            // TODO: Receive results from worker and store them in the results struct.
            //       If message is "DONE", set worker_done[i] to 1 and break out of loop.
            //       Messages will have the format ("%s %d %d", executable_path, parameter, status)
            //       so consider using sscanf() to parse the message.
            while (1) {
                
            }
        }
    }

    free(worker_done);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    num_workers = get_batch_size();
    // Check if some workers won't be used -> don't spawn them
    if (num_workers > num_executables * total_params) {
        num_workers = num_executables * total_params;
    }
    workers = malloc(num_workers * sizeof(pid_t));

    // Create a unique key for message queue
    key_t key = IPC_PRIVATE; // Creates shared space in memory 

    // TODO: Create a message queue --> Done
    int msqid = msgget(key, IPC_CREAT | 0666);
    // key_t keyID = ftok(argv[1], 3); // assuming argv[1] holds our path name we are then assiging a unique identfier with it which is 3 -- may need to double check
    int msgid = msgget(key, IPC_CREAT); // Creating the message queue here if it does not exist otherwise it uses it's own 

    

    int num_pairs_to_test = num_executables * total_params;
    
    // Spawn workers and send them the total number of (executable, parameter) pairs they will test 
    for (int i = 0; i < num_workers; i++) {
        int leftover = num_pairs_to_test % num_workers - i > 0 ? 1 : 0;
        int pairs_per_worker = num_pairs_to_test / num_workers + leftover;

        // TODO: Spawn worker and send it the number of pairs it will test via message queue --> Done
        launch_worker(msqid, pairs_per_worker, i + 1);
    }

    // Send (executable, parameter) pairs to workers
    int sent = 0;
    for (int i = 0; i < total_params; i++) {
        for (int j = 0; j < num_executables; j++) {
            msgbuf_t msg;
            long worker_id = sent % num_workers + 1;
            
            // TODO: Send (executable, parameter) pair to worker via message queue (mtype = worker_id) --> DOUBLE CHECK WITH TA
            char buf2[255];
            execl(executable_paths, argv[j], NULL); // --> maybe we are wrong DOUBLE CHECK WITH TA
            msgsnd(worker_id, &msg, sizeof(msg), IPC_NOWAIT); // --> maybe we are wrong DOUBLE CHECK WITH TA
            sent++;
        }
    }

    // TODO: Wait for ACK from workers to tell all workers to start testing (synchronization)
    receive_ack_from_workers(msqid, num_workers);

    // TODO: Send message to workers to allow them to start testing
    send_synack_to_workers(msqid, num_workers);

    // TODO: Wait for all workers to finish and collect their results from message queue
    wait_for_workers(msqid, num_pairs_to_test, argv + 2);

    // TODO: Remove ALL output files (output/<executable>.<input>)
    //check ------------------
    remove_output_files(results, msqid, num_workers, total_params); // --> DOUBLE CHECK THIS WITH TA


    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // TODO: Remove the message queue
    msgctl(msqid,IPC_RMID,NULL); // --> double check this MAYBE WRONG


    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);
    free(workers);
    
    return 0;
}
