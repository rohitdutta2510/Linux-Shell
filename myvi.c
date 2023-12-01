#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ncurses.h>


// Function to display text and update the cursor position
void displayText(char** lines, int numLines, int cursorX, int cursorY) {
    clear();
    // Display each line
    for (int i = 0; i < numLines; i++) {
        mvprintw(i, 0, "%s", lines[i]);
    }
    // Move the cursor to the current position
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
        // Add a newline character after each line (except the last line)
        if (i < numLines - 1) {
            fprintf(file, "\n");
        }
    }
    fclose(file);
}

// Function to load the content from a file
void loadFromFile(const char* filename, char*** lines, int* numLines, int* numChars) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        // The file does not exist; initialize with an empty line
        *lines = (char**)malloc(sizeof(char*));
        (*lines)[0] = (char*)malloc(1);
        (*lines)[0][0] = '\0';
        *numLines = 1;
        *numChars = 0;
        return;
    }

    *numLines = 0;
    *numChars = 0;

    while (!feof(file)) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), file)) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                // Remove the newline character
                buffer[len - 1] = '\0';
            }
            (*lines) = (char**)realloc(*lines, ((*numLines) + 1) * sizeof(char*));
            (*lines)[*numLines] = strdup(buffer);
            (*numChars) += len;
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

// Function to display the status message
void displayStatus(const char* status) {
    mvprintw(LINES - 1, 0, "%s", status);
}

// Function to handle the 'vi' editor
int main(int argc, char* argv[]) {
    char* filename = argv[1];

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();

    int cursorX = 0;
    int cursorY = 0;
    int numLines = 0;
    char** lines = NULL;
    int numChars = 0;
    int numLinesModified = 0;
    int numWordsModified = 0;
    int numCharsModified = 0;

    if (filename) {
        // Load file content into 'lines' array
        // Initialize 'numLines', allocate memory for 'lines', and populate 'numChars'
        loadFromFile(filename, &lines, &numLines, &numChars);
    } else {
        lines = (char**)malloc(sizeof(char*));
        lines[0] = (char*)malloc(1);
        lines[0][0] = '\0';
        numLines = 1;
    }

    while (1) {
        displayText(lines, numLines, cursorX, cursorY);

        int ch = getch();
        
        printw("Keycode: %d\n", ch);
        printf("Keycode: %d\n", ch);

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
            case 27: // ESC key (Exit)
                // Cleanup and exit
                for (int i = 0; i < numLines; i++) {
                    free(lines[i]);
                }
                free(lines);
                endwin();
                return 0;
            case 330: // DELETE key
                if (cursorX > 0) {
                    // Delete the character at cursorX
                    memmove(&lines[cursorY][cursorX], &lines[cursorY][cursorX + 1], strlen(lines[cursorY]) - cursorX);
                    cursorX--;
                    numChars--;
                    numCharsModified--;
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
                    numLinesModified--;
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
                break;

            case 19: // Ctrl+S (Save)
                // Implement saving the content to a file
                if (filename) {
                    // Write 'lines' to the file
                    // Handle file I/O
                    saveToFile(filename, lines, numLines);
                    displayStatus("File saved.");
                }
                break;
            default:
                // Handle character insertion
                char c = (char)ch;
                lines[cursorY] = (char*)realloc(lines[cursorY], strlen(lines[cursorY]) + 2);
                memmove(&lines[cursorY][cursorX + 1], &lines[cursorY][cursorX], strlen(lines[cursorY]) - cursorX + 1);
                lines[cursorY][cursorX] = c;
                cursorX++;
                numChars++;
                numCharsModified++;
                break;
        }
    }
    // Free memory used by 'lines'
    freeLines(lines, numLines);
    
    // Cleanup
    endwin();

    
    printf("Lines: %d, Words: %d, Characters: %d", numLinesModified, numWordsModified, numCharsModified);
    
    return numLinesModified;
    //return 0;
}