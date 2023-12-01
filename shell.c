#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
#define len_command 30
#define max_args 10
#define max_commands 10
#define MAX_VECTOR_DIMENSION 100
#define MAX_THREADS 3

// Structure to pass arguments to the thread functions
struct ThreadArgs {
    double* vec1;
    double* vec2;
    double* result;
    int dimension;
    int start_idx;
    int end_idx;
};

// Structure to store editor stats
typedef struct {
    int lines;
    int words;
    int characters;
} viStats;

//tokenize pipe separated multiple commands
int processPipe(char* command, char** parsedComm){
    int i=0;
    char *token = strtok(command, "|");
    while (token != NULL) {
        parsedComm[i++] = token;
        token = strtok(NULL, "|");
    }
    parsedComm[i] = NULL;
    return i;
}

//tokenize the commands into an array of arguments
int processCommand(char* command, char** parsedArgs){
    int i=0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        parsedArgs[i++] = token;
        token = strtok(NULL, " ");
    }
    parsedArgs[i] = NULL;
    return i;
}

//function removing newline character
void removeNewline(char* str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

//change directory function
void changeDir(char *command){
    char* parsedArgs[max_args];
    int num_args = processCommand(command, parsedArgs);
    if (num_args != 2) {
            printf("Usage: cd <directory_name>\n");
    } else {
        if (chdir(parsedArgs[1]) != 0) {
            perror("chdir");
        }
    }
}

//print command list
void printHelp(){
    printf("Available commands:\n");
    printf("1. pwd\n");
    printf("2. cd <directory_name>\n");
    printf("3. mkdir <directory_name>\n");
    printf("4. ls <flag>\n");
    printf("5. addvec <filename1> <filename2> -<no_threads>\n");
    printf("6. subvec <filename1> <filename2> -<no_threads>\n");
    printf("7. dotprod <filename1> <filename2> -<no_threads>\n");
    printf("8. exit\n");
    printf("9. help\n");
}

//execute command
void execute(char* command){
    char* parsedArgs[max_args];
    int num_args = processCommand(command, parsedArgs);

    if(num_args == 0)
        return;

    if(execvp(parsedArgs[0], parsedArgs) < 0){
        perror("Could not execute command");
        exit(1);
    }

    exit(0);
}

// Function to display text and update the cursor position
void displayText(char** lines, int numLines, int cursorX, int cursorY) {
    clear();
    // Display each line
    for (int i = 0; i < numLines; i++) {
        mvprintw(i, 0, "%s", lines[i]);
    }
    // Moving cursor to current position
    move(cursorY, cursorX);
    refresh();
}

// Function to save the content to a file
void saveToFile(const char* filename, char** lines, int numLines) {
    FILE* file = fopen(filename, "w+");
    if (!file) {
        mvprintw(LINES - 1, 0, "Error: Cannot open the file for writing.");
        return;
    }

    for (int i = 0; i < numLines; i++) {
        fprintf(file, "%s", lines[i]);
        if (i < numLines - 1) { // Adding newline character after each line (except the last line)
            fprintf(file, "\n");
        }
    }
    fclose(file);
}

// Function to load the content from a file
void loadFromFile(const char* filename, char*** lines, int* numLines) {
    FILE* file = fopen(filename, "r");
    if (!file) {                            // The file does not exist; initialize with an empty line
        *lines = (char**)malloc(sizeof(char*));
        (*lines)[0] = (char*)malloc(1);
        (*lines)[0][0] = '\0';
        *numLines = 1;
        return;
    }

    *numLines = 0;

    while (!feof(file)) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), file)) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') { // Remove the newline character
                buffer[len - 1] = '\0';
            }
            (*lines) = (char**)realloc(*lines, ((*numLines) + 1) * sizeof(char*));
            (*lines)[*numLines] = strdup(buffer);
            (*numLines)++;
        }
    }

    fclose(file);   
}

// Function to free the memory used by 'lines'
void freeLines(char** lines, int numLines) {
    for (int i = 0; i < numLines; i++) {
        free(lines[i]);
    }
    free(lines);
}

// Function to count the number of words in a string
int countWords(const char *str) {
    int count = 0;
    int inWord = 0; // 0 indicates not in a word, 1 indicates in a word

    for (int i = 0; str[i]; i++) {
        if (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') {
            inWord = 0; // Not in a word
        } else if (!inWord) {
            inWord = 1; // Start of a new word
            count++;
        }
    }
    return count;
}

// Function to update EditorStats based on modifications
void updateEditorStats(char **lines, int numLines, viStats *stats) {
    stats->lines = numLines;
    stats->words = 0;
    stats->characters = 0;

    for (int i = 0; i < numLines; i++) {
        stats->characters += strlen(lines[i]);
        stats->words += countWords(lines[i]);
    }
}

// Function to handle the 'vi' editor
viStats myvi(char* filename) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();

    int cursorX = 0;
    int cursorY = 0;
    int numLines = 0;
    char** lines = NULL;
    int numLinesModified = 0;
    int numWordsModified = 0;
    int numCharsModified = 0;

    // Load file content into 'lines' array
    // Initialize 'numLines', allocate memory for 'lines', and populate 'numChars'
    if (filename) {
        loadFromFile(filename, &lines, &numLines);
    } else {
        lines = (char**)malloc(sizeof(char*));
        lines[0] = (char*)malloc(1);
        lines[0][0] = '\0';
        numLines = 1;
    }

    int go = 1;
    while (go) {
        displayText(lines, numLines, cursorX, cursorY); // display the text from the file loaded

        int ch = getch();

        switch (ch) {
            case KEY_LEFT:
                cursorX = (cursorX > 0) ? cursorX - 1 : 0;
                break;
            case KEY_RIGHT:
                cursorX = (cursorX < (int)strlen(lines[cursorY])) ? cursorX + 1 : strlen(lines[cursorY]);
                break;
            case KEY_UP:
                cursorY = (cursorY > 0) ? cursorY - 1 : 0;
                break;
            case KEY_DOWN:
                cursorY = (cursorY < numLines - 1) ? cursorY + 1 : numLines - 1;
                break;
            case 24: // Ctrl + X
                go = 0;
                break;
            case 27: // ESC key (Exit)
                nodelay(stdscr, TRUE); // Don't wait for another key
                int n = getch();
                if (n == -1) {
                    // Escape key was pressed
                    go = 0;
                }
                nodelay(stdscr, FALSE); // Return to normal input mode
                break;
            case 330: // DELETE key
                if (cursorX > 0) {
                    // Delete the character at cursorX
                    memmove(&lines[cursorY][cursorX], &lines[cursorY][cursorX + 1], strlen(lines[cursorY]) - cursorX);
                    cursorX--;
                    numCharsModified++;
                } else if (cursorY > 0) {
                    // Merge the current line with the previous line
                    int len = strlen(lines[cursorY - 1]);
                    int len_current = strlen(lines[cursorY]);
                    lines[cursorY - 1] = (char*)realloc(lines[cursorY - 1], len + len_current + 1);
                    strcat(lines[cursorY - 1], lines[cursorY]);
                    free(lines[cursorY]);
                    
                    // Shift the lines below up by one position
                    for (int i = cursorY; i < numLines - 1; i++) {
                        lines[i] = lines[i + 1];
                    }
                    cursorY--;
                    numLines--;
                    numLinesModified++;
                }
                break;
            case 10: // ENTER key
                // Allocate memory for the new line
                char* newLine = (char*)malloc(1);
                newLine[0] = '\0';

                if (numLines == 0) {
                    // If there are no lines, create a new one and assign it to 'lines'
                    lines = (char**)malloc(sizeof(char*));
                    lines[0] = newLine;
                    numLines = 1;
                } else {
                    // Insert the new line below the current line
                    lines = (char**)realloc(lines, (numLines + 1) * sizeof(char*));
                    for (int i = numLines; i > cursorY + 1; i--) {
                        lines[i] = lines[i - 1];
                    }
                    lines[cursorY + 1] = newLine;
                    numLines++;
                }

                // Split the current line into two lines at the cursor position
                int len_current = strlen(lines[cursorY]);
                char* temp = (char*)malloc(len_current - cursorX + 1);
                strcpy(temp, &lines[cursorY][cursorX]);
                lines[cursorY][cursorX] = '\0';
                lines[cursorY + 1] = (char*)realloc(lines[cursorY + 1], len_current - cursorX + 1);
                strcpy(lines[cursorY + 1], temp);

                // Update cursor position
                cursorX = 0;
                cursorY++;

                numLinesModified++;
                break;

            case 19: // Ctrl+S (Save)
                if (filename) {         // Writing 'lines' to the file
                    saveToFile(filename, lines, numLines);
                }
                break;
            default:
                // Handle character insertion
                char c = (char)ch;
                lines[cursorY] = (char*)realloc(lines[cursorY], strlen(lines[cursorY]) + 2);
                memmove(&lines[cursorY][cursorX + 1], &lines[cursorY][cursorX], strlen(lines[cursorY]) - cursorX + 1);
                lines[cursorY][cursorX] = c;
                cursorX++;
                numCharsModified++;
                break;
        }
    }

    numWordsModified += countWords(lines[cursorY]);

    // Free memory used by 'lines'
    freeLines(lines, numLines);
    
    // Close window
    endwin();

    viStats stats;
    stats.lines = numLinesModified;
    stats.words = numWordsModified;
    stats.characters = numCharsModified;

    return stats; // return editor stats
}

// vectorized addition function for threads
void* add_vector(void* args) {
    struct ThreadArgs* targs = (struct ThreadArgs*)args;
    for (int i = targs->start_idx; i < targs->end_idx; i++) {
        targs->result[i] = targs->vec1[i] + targs->vec2[i];
    }
    pthread_exit(NULL);
}

// vectorized subtract function for threads
void* subtract_vector(void* args) {
    struct ThreadArgs* targs = (struct ThreadArgs*)args;
    for (int i = targs->start_idx; i < targs->end_idx; i++) {
        targs->result[i] = targs->vec1[i] - targs->vec2[i];
    }
    pthread_exit(NULL);
}

// vectorized dotproduct function for threads
void* dot_product(void* args) {
    struct ThreadArgs* targs = (struct ThreadArgs*)args;
    double dot_prod = 0;
    for (int i = targs->start_idx; i < targs->end_idx; i++) {
        dot_prod += targs->vec1[i] * targs->vec2[i];
    }
    targs->result[0] += dot_prod; // store the sum in result[0]
    pthread_exit(NULL);
}

// read the vectorized inputs from the file, given filname1 and filename2
int readVectorfromFile(char* file1_name, char* file2_name, double* vec1, double* vec2){
    int size1 = 0, size2 = 0, dimension = 0;
    FILE* file1 = fopen(file1_name, "r");
    FILE* file2 = fopen(file2_name, "r");

    if (file1 && file2) {
        while (fscanf(file1, "%lf", &vec1[size1]) == 1) // read vector from file1
            size1++;

        while (fscanf(file2, "%lf", &vec2[size2]) == 1) // read vector from file2
            size2++;

        fclose(file1);
        fclose(file2);

        if(size1 != size2){
            printf("\nError: Vector size mismatch\n");
            return -1;
        }
        dimension = size1;
    } else {
        printf("Error: Cannot open input files.\n");
        return 1;
    }
    if (dimension == 0) {
        printf("Error: Empty or invalid input files.\n");
        return 1;
    }
    return dimension;
}

// Thread execution function
int executeThread(char* command) {
    char* parsedArgs[max_args];

    int num_args = processCommand(command, parsedArgs);

    char* operation = parsedArgs[0]; // get operation name
    char* file1_name = parsedArgs[1]; // get file1 name
    char* file2_name = parsedArgs[2]; // get file2 name


    int num_threads = MAX_THREADS; // default - 3 Threads 
    if ((num_args == 4) && (strncmp(parsedArgs[3], "-", 1) == 0)) {
        num_threads = atoi(parsedArgs[3] + 1);
    }

    // vector arrays to store the vectors from file
    double vec1[MAX_VECTOR_DIMENSION];
    double vec2[MAX_VECTOR_DIMENSION];
    
     // read files, store the vector and return the size of vectors
    int dimension = readVectorfromFile(file1_name, file2_name, vec1, vec2);

    double* result = (double*)malloc(dimension * sizeof(double)); //result array
    pthread_t threads[num_threads];
    struct ThreadArgs targs[num_threads];
    int chunk_size = dimension / num_threads; // amount of portion assigned to each thread


    if (strcmp(operation, "addvec") == 0) {
        for (int i = 0; i < num_threads; i++) {
            targs[i].vec1 = vec1;
            targs[i].vec2 = vec2;
            targs[i].result = result;
            targs[i].start_idx = i * chunk_size;
            targs[i].end_idx = (i == num_threads - 1) ? dimension : (i + 1) * chunk_size;
            pthread_create(&threads[i], NULL, add_vector, &targs[i]);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        // Print the result
        for (int i = 0; i < dimension; i++) {
            printf("%.2f ", result[i]);
        }
        printf("\n");
    } 
    else if (strcmp(operation, "subvec") == 0) {
        for (int i = 0; i < num_threads; i++) {
            targs[i].vec1 = vec1;
            targs[i].vec2 = vec2;
            targs[i].result = result;
            targs[i].start_idx = i * chunk_size;
            targs[i].end_idx = (i == num_threads - 1) ? dimension : (i + 1) * chunk_size;
            pthread_create(&threads[i], NULL, subtract_vector, &targs[i]);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        // Print the result
        for (int i = 0; i < dimension; i++) {
            printf("%.2f ", result[i]);
        }
        printf("\n");
    } 
    else if (strcmp(operation, "dotprod") == 0) {
        for (int i = 0; i < num_threads; i++) {
            targs[i].vec1 = vec1;
            targs[i].vec2 = vec2;
            targs[i].result = result;
            targs[i].start_idx = i * chunk_size;
            targs[i].end_idx = (i == num_threads - 1) ? dimension : (i + 1) * chunk_size;
            pthread_create(&threads[i], NULL, dot_product, &targs[i]);
        }

        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }

        printf("Dot Product: %.2f\n", result[0]);
    } else {
        printf("Error: Unknown operation '%s'\n", operation);
        return 1;
    }

    return 0;
}

// function to execute single command and multiple pipe separated command
void execArgsPiped(char* commandList){
    char* parsedComm[max_commands];
    int i;
    int num_comm = processPipe(commandList, parsedComm); //number of pipe separated commands

    if(num_comm == 0)
        return;

    //single command execution
    if(num_comm == 1){ 
        int background = 0;
        if((parsedComm[0][strlen(parsedComm[0]) - 1] == '&')){
            background = 1;  
        }
        if(!background){
            if(strncmp(parsedComm[0], "cd", 2) == 0){
                changeDir(parsedComm[0]);
            }
            else if (strcmp(parsedComm[0], "exit") == 0) {
                printf("\nClosing shell ...\n");
                exit(0);
            }
            else if(strncmp(parsedComm[0], "help", 4) == 0){
                printHelp();
            }
            else{
                pid_t pid = fork();

                if (pid < 0) { 
                    perror("Failed forking child"); 
                    exit(1);
                } else if(pid == 0){
                    // Child process
                    execute(parsedComm[0]);
                    exit(0);
                }else{
                    // Parent process
                    wait(NULL);
                    return;
                }
            }
        }
    }
    //multiple pipe separated commands execution
    else{
        int i;
        int pipefd[2]; //Pipe file descriptor
        int prev_stdin = 0;

        for(i=0; i<num_comm; i++){
            if(strncmp(parsedComm[i], "cd", 2) == 0){
                changeDir(parsedComm[i]);
            }
            else if (strcmp(parsedComm[i], "exit") == 0) {
                printf("\nClosing shell ...\n");
                exit(0);
            }
            else if(strncmp(parsedComm[i], "help", 4) == 0){
                printHelp();
            }
            else{
                if(pipe(pipefd) < 0){
                    perror("Pipe could not be initialized");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Failed forking child");
                    exit(1);
                } else if (pid == 0) {
                    // Child process
                    close(pipefd[0]); // Close the read end of the pipe

                    if (i < num_comm - 1) { // if not last command
                        dup2(pipefd[1], STDOUT_FILENO); // set output to the write end of the pipe
                    }

                    if (i > 0) { // if not the first command
                        dup2(prev_stdin, STDIN_FILENO); // set input to the prev comm input
                    }
                    execute(parsedComm[i]);
                    exit(0);
                } else {
                    // Parent process
                    wait(NULL); // Wait for the child process to finish
                    close(pipefd[1]); // Write end of the pipe closed
                    prev_stdin = pipefd[0]; // Prev std input updated
                }
            } //commands other than cd end
        } //loop end
    } //multi-command end
}


int main(){
    char *line = NULL;
    char *command = NULL;
    size_t command_size = 0;
    
    while (1) {
        line = readline("\nshell> ");

        if (line == NULL) {
            break;
        }
        
        //multi-line command
        if(strlen(line) > 0 && line[strlen(line) - 1] == '\\'){
            line[strlen(line) - 1] = ' ';
            command_size = strlen(line);
            command = (char*)malloc(command_size + 1);
            strcpy(command, line);
            add_history(line);

            while(1){
                line = readline("> ");
                //intermediate lines of multiline command
                if(line[strlen(line) - 1] == '\\'){
                    line[strlen(line) - 1] = ' ';
                    command_size += strlen(line);
                    command = (char *)realloc(command, command_size + 1);
                    strcat(command, line);
                    add_history(line);
                }
                //ending line of multi-line command
                else{
                    command_size += strlen(line);
                    command = (char *)realloc(command, command_size + 1);
                    strcat(command, line);
                    add_history(line);
                    add_history(command);
                    free(line);
                    break;
                }
            }
        }
        //single-line command
        else{
            command_size = strlen(line);
            command = (char*)malloc(command_size + 1);
            strcpy(command, line);
            add_history(line);
            free(line);
        }
        
        if (strlen(command) > 0) {
            if (strncmp(command, "vi ", 3) == 0) {
                char* filename = command + 3; // Extract filename from the 'vi' command
                // char path[50] = "./myvi ";                
                // strcat(path, filename);
                // printf("%s", path);
                // system(path);
                viStats stats = myvi(filename);
                printf("Lines modified = %d, Words modified = %d, Characters modified = %d", stats.lines, stats.words, stats.characters);
            }
            else if (strncmp(command, "addvec", 6) == 0 || strncmp(command, "subvec", 6) == 0 || strncmp(command, "dotprod", 7) == 0){
                executeThread(command);
            }
            else{
            execArgsPiped(command);
            }
        }
    }

    return 0;
}
