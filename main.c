#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For tolower()

int main() {
    char gameChoice[20];
    char modeChoice[5];
    char command[100]; // Buffer to hold the command to execute

    // Prompt for game selection
    printf("Choose a game to play:\t");
    printf("1. Tic Tac Toe (tictactoe)\n");
    printf("2. Connect Four (connectfour)\n");
    printf("3. Chess (chess)\n");
    printf("Enter your choice (e.g., 'tictactoe'): ");
    scanf("%19s", gameChoice);

    // Convert game choice to lowercase for easier comparison
    for(int i = 0; gameChoice[i]; i++){
        gameChoice[i] = tolower(gameChoice[i]);
    }

    // Prompt for mode selection
    printf("Choose a mode:\t");
    printf("1. 2D\t");
    printf("2. 3D\n");
    printf("Enter your choice (2D or 3D): ");
    scanf("%4s", modeChoice);

    // Convert mode choice to lowercase
     for(int i = 0; modeChoice[i]; i++){
        modeChoice[i] = tolower(modeChoice[i]);
    }

    // Determine the executable to run based on choices
    if (strcmp(gameChoice, "tictactoe") == 0 || strcmp(gameChoice, "1") == 0) {
        if (strcmp(modeChoice, "2d") == 0 || strcmp(modeChoice, "1") == 0) {
            sprintf(command, ".\\twoDTicTacToe.exe");
        } else if (strcmp(modeChoice, "3d") == 0 || strcmp(modeChoice, "2") == 0) {
            sprintf(command, ".\\threeDTicTacToe.exe");
        } else {
            printf("Invalid mode choice.\n");
            return 1;
        }
    } else if (strcmp(gameChoice, "connectfour") == 0 || strcmp(gameChoice, "2") == 0) {
        if (strcmp(modeChoice, "2d") == 0 || strcmp(modeChoice, "1") == 0) {
            sprintf(command, ".\\twoDConnectFour.exe");
        } else if (strcmp(modeChoice, "3d") == 0 || strcmp(modeChoice, "2") == 0) {
            // Assuming the raylib version is the intended 3D version
            sprintf(command, ".\\threeDConnectFour_raylib.exe");
        } else {
            printf("Invalid mode choice.\n");
            return 1;
        }
    } else if (strcmp(gameChoice, "chess") == 0 || strcmp(gameChoice, "3") == 0) {
        if (strcmp(modeChoice, "2d") == 0 || strcmp(modeChoice, "1") == 0) {
            sprintf(command, ".\\twoDChess.exe");
        } else if (strcmp(modeChoice, "3d") == 0 || strcmp(modeChoice, "2") == 0) {
             // Assuming the raylib version is the intended 3D version
            sprintf(command, ".\\threeDChess_raylib.exe");
        } else {
            printf("Invalid mode choice.\n");
            return 1;
        }
    } else {
        printf("Invalid game choice.\n");
        return 1;
    }

    // Execute the chosen game
    printf("Launching %s...\n", command);
    int result = system(command);

    if (result != 0) {
        printf("Failed to launch the game. Make sure the executable exists in the current directory.\n");
        return 1;
    }

    printf("Game finished.\n");

    return 0;
}
