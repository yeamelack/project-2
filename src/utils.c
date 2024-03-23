#include "utils.h"


const char* get_status_message(int status) {
    switch (status) {
        case CORRECT: return "correct";
        case INCORRECT: return "incorrect";
        case SEGFAULT: return "crash";
        case STUCK_OR_INFINITE: return "stuck/inf";
        default: return "unknown";
    }
}


char *get_exe_name(char *path) {
    return strrchr(path, '/') + 1;
}


char **get_student_executables(char *solution_dir, int *num_executables) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    // Open the directory
    dir = opendir(solution_dir);
    if (!dir) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Count the number of executables
    *num_executables = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);
        
        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.')
                (*num_executables)++;
        } 
        else {
            perror("Failed to get file status");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for the array of strings
    char **executables = (char **) malloc(*num_executables * sizeof(char *));

    // Reset the directory stream
    rewinddir(dir);

    // Read the file names
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);

        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.') {
                executables[i] = (char *) malloc((strlen(solution_dir) + strlen(entry->d_name) + 2) * sizeof(char));
                sprintf(executables[i], "%s/%s", solution_dir, entry->d_name);
                i++;
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Return the array of strings (remember to free the memory later)
    return executables;
}


// TODO: Implement this function
int get_batch_size() {
    FILE* cpuinfo_file = fopen("/proc/cpuinfo", "r");
    if (cpuinfo_file == NULL) {
        fprintf(stderr, "Error: Could not open /proc/cpuinfo\n");
        exit(1);
    }

    char line[256];
    int processor_count = 0;

    while (fgets(line, sizeof(line), cpuinfo_file)) {
        if (strstr(line, "processor") != NULL) {
            processor_count++;
        }
    }

    fclose(cpuinfo_file);
    return processor_count;

}


// TODO: Implement this function
void create_input_files(char **argv_params, int num_parameters) {
    if(argv_params == NULL || num_parameters <= 0){
        perror("argv_params is either NULL or num_paramerter is less than 1. try again");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i<num_parameters; i++){
        char filename[127];
        sprintf(filename, "input/%s.in",argv_params[i]);
        FILE *file = fopen(filename, "w");

        if(file == NULL){
            perror("failed opening student file");
            exit(EXIT_FAILURE);
        }

        fprintf(file, "%s" , argv_params[i]);
        fclose(file);
    }

}


// TODO: Implement this function
void start_timer(int seconds, void (*timeout_handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timeout_handler;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    alarm(TIMEOUT_SECS);
}


// TODO: Implement this function
void cancel_timer() {
    alarm(0);
}


// TODO: Implement this function
void remove_input_files(char **argv_params, int num_parameters) {
    if(argv_params == NULL || num_parameters <= 0){
        perror("argv_params is either NULL or num_paramerter is less than 1. try again");
        exit(EXIT_FAILURE);
    }
    for(int i=0; i<num_parameters; i++){
        char filename[127];
        sprintf(filename, "input/%s.in", argv_params[i]);

        if(unlink(filename) != 0){
            perror("cannot remove file");
            exit(EXIT_FAILURE);
        }
    }

}


// TODO: Implement this function
void remove_output_files(autograder_results_t *results, int tested, int current_batch_size, char *param) {
    //check this -----------------
    for (int i = tested - current_batch_size; i < tested; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        char filename[255];
        sprintf(filename, "output/%s.%s", exe_name, param);
        if (remove(filename) != 0) {
            perror("Error removing output file");
        }
    }
}


int get_longest_len_executable(autograder_results_t *results, int num_executables) {
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }
    return longest_len;
}
 

void write_results_to_file(autograder_results_t *results, int num_executables, int total_params) {
    FILE *file = fopen("results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Find the longest executable name (for formatting purposes)
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }

    // Sort the results data structure by executable name (specifically number at the end)
    for (int i = 0; i < num_executables; i++) {
        for (int j = i + 1; j < num_executables; j++) {
            char *exe_name_i = get_exe_name(results[i].exe_path);
            int num_i = atoi(strrchr(exe_name_i, '_') + 1);
            char *exe_name_j = get_exe_name(results[j].exe_path);
            int num_j = atoi(strrchr(exe_name_j, '_') + 1);
            if (num_i > num_j) {
                autograder_results_t temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Write results to file
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);

        char format[20];
        sprintf(format, "%%-%ds:", longest_len);
        fprintf(file, format, exe_name); // Write the program path
        for (int j = 0; j < total_params; j++) {
            fprintf(file, "%5d (", results[i].params_tested[j]); // Write the pi value for the program
            const char* message = get_status_message(results[i].status[j]);
            fprintf(file, "%9s) ", message); // Write each status
        }
        fprintf(file, "\n");
    }

    fclose(file);
}


// TODO: Implement this function
double get_score(char *results_file, char *executable_name) {
    FILE *fp = fopen(results_file, "r");
    if (!fp) {
        perror("Failed to open file");
        return -1.0;
    }

    // Read the first line to determine the length of lines
    char line[255];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1.0;
    }
    int line_length = strlen(line);

    // Calculate the number of tests from the first line
    double test_count = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '(') test_count++;
    }

    // Extracting 'x' from "sol_x"
    int line_number = atoi(executable_name + strlen(executable_name) - 1);
    printf("%d\n", line_number);

    // Seek to the beginning of the line for the executable_name
    fseek(fp, (line_number - 1) * line_length, SEEK_SET);

    // Read the line containing the executable's results
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1.0;
    }

    fclose(fp);

    // Parse the line for correct answers
    double correct_count = 0;
    double incorrect_count = 0;
    double seg = 0;
    double stuck_inf = 0;
    char *token = strtok(line, " ");
    printf("%s\n", token);
    while (token) {
        if (strstr(token, "(incorrect)")){
            incorrect_count++;
        }
        if (strstr(token, "crash")){
            seg++;
        }
        if (strstr(token, "(stuck/inf)")){
            stuck_inf++;;
        }
        correct_count = test_count - seg - incorrect_count - stuck_inf;


        printf("%s  ", token);
        token = strtok(NULL, " ");
    }

    return (correct_count / test_count);
}


void write_scores_to_file(autograder_results_t *results, int num_executables, char *results_file) {
    for (int i = 0; i < num_executables; i++) {
        double student_score = get_score(results_file, results[i].exe_path);
        char *student_exe = get_exe_name(results[i].exe_path);

        char score_file[] = "scores.txt";

        FILE *score_fp;
        if (i == 0)
            score_fp = fopen(score_file, "w");
        else
            score_fp = fopen(score_file, "a");

        if (!score_fp) {
            perror("Failed to open score file");
            exit(1);
        }

        int longest_len = get_longest_len_executable(results, num_executables);

        char format[20];
        sprintf(format, "%%-%ds: ", longest_len);
        fprintf(score_fp, format, student_exe);
        fprintf(score_fp, "%5.3f\n", student_score);

        fclose(score_fp);
    }
}
