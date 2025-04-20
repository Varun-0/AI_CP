#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "include/raylib.h" // Include Raylib header
#include "include/raymath.h" // Include Raymath header

// Define constants (including those previously in the header)
#define ROWS 4 // Changed for Sogo-like 4x4x4
#define COLS 4 // Changed for Sogo-like 4x4x4
#define PLAYER 1
#define AI 2
#define EMPTY 0
#define HEIGHT 4 // Already 4, but confirming for 4x4x4

// Global variables
int difficulty = 4; // Default AI depth
int board3D[HEIGHT][ROWS][COLS]; // Moved global board definition here
Camera camera = { 0 }; // Raylib camera
int currentPlayer = PLAYER;
bool gameOver = false;
int winner = EMPTY; // 0: No winner, 1: Player, 2: AI, 3: Draw
const float PIECE_RADIUS = 0.4f;
const float SPACING = 1.0f; // Spacing between centers of pieces

// Forward declarations for 3D functions and others used before definition
bool isFull3D();
void clearInputBuffer();
void playGame3D(); // Renamed from playGame
void printBoard3D(); // Added forward declaration
bool isValidMove3D(int r, int c); // Added forward declaration
int makeMove3D(int r, int c, int piece); // Added forward declaration
bool winningMove3D(int piece); // Added forward declaration
void getBestMove3D(int *bestR, int *bestC); // Added forward declaration
int minimax3D(int depth, int alpha, int beta, bool maximizing); // Added forward declaration
int evaluateBoard3D(); // Added forward declaration
void undoMove3D(int r, int c); // Added forward declaration
void drawBoardRaylib(); // Forward declaration for Raylib drawing function
void updateGameRaylib(); // Forward declaration for game logic update

// ----------------------- 3D CONNECT 4 SECTION -----------------------

// int board3D[HEIGHT][ROWS][COLS]; // Moved higher up

void printBoard3D() {
    printf("\n3D CONNECT 4\n");
    for (int h = 0; h < HEIGHT; h++) {
        printf("Level %d:\n", h);
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                printf("| %d ", board3D[h][r][c] == EMPTY ? 0 : board3D[h][r][c]); // Use EMPTY
            }
            printf("|\n");
        }
        printf("\n");
    }
     printf("   ");
    for (int c = 0; c < COLS; c++) {
        printf(" %d ", c);
    }
    printf(" (Cols)\n");
     printf("Rows 0-%d\n", ROWS - 1);

}

bool isValidMove3D(int r, int c) {
    // Check bounds first
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
        return false;
    }
    // Check if the column is full (topmost level has a piece)
    return board3D[0][r][c] == EMPTY; // Use EMPTY
}


int makeMove3D(int r, int c, int piece) {
    // Find the lowest available height (h) in the selected (r, c) position
    for (int h = HEIGHT - 1; h >= 0; h--) {
        if (board3D[h][r][c] == EMPTY) { // Use EMPTY
            board3D[h][r][c] = piece;
            return h; // Return the height where the piece was placed
        }
    }
    return -1; // Should not happen if isValidMove3D was checked
}


void undoMove3D(int r, int c) {
    // Find the highest piece in the column (r, c) and remove it
    for (int h = 0; h < HEIGHT; h++) {
        if (board3D[h][r][c] != EMPTY) { // Use EMPTY
            board3D[h][r][c] = EMPTY; // Use EMPTY
            break;
        }
    }
}

// Comprehensive check for 4-in-a-row in 3D
bool winningMove3D(int piece) {
    // Check all possible lines of 4 (horizontal, vertical, depth-wise, and all diagonals)
    for (int h = 0; h < HEIGHT; ++h) {
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                // Check lines starting from (h, r, c) in all 13 directions in 3D space
                // (Only need to check directions where coordinates increase or stay same,
                // as other directions are covered by starting from different points)

                 // Simplified directions (check only positive/zero directions from each point)
                 int simplified_directions[13][3] = {
                    // Within a plane (h constant)
                    {0, 0, 1},  // --> (Horizontal)
                    {0, 1, 0},  // | (Vertical)
                    {0, 1, 1},  // / (Diagonal up-right)
                    {0, 1, -1}, // \\ (Diagonal up-left) - Need this one explicitly

                    // Vertical columns
                    {1, 0, 0},  // Upwards

                    // Diagonals involving height change
                    {1, 0, 1},  // Up-forward
                    {1, 0, -1}, // Up-backward
                    {1, 1, 0},  // Up-right (vertical plane)
                    {1, -1, 0}, // Up-left (vertical plane)
                    {1, 1, 1},  // 3D diagonal (all increasing)
                    {1, 1, -1}, // 3D diagonal
                    {1, -1, 1}, // 3D diagonal
                    {1, -1, -1} // 3D diagonal
                };


                for (int i = 0; i < 13; ++i) {
                    int dh = simplified_directions[i][0];
                    int dr = simplified_directions[i][1];
                    int dc = simplified_directions[i][2];

                    // Check if the line fits within the board boundaries
                    if (h + 3 * dh >= 0 && h + 3 * dh < HEIGHT &&
                        r + 3 * dr >= 0 && r + 3 * dr < ROWS &&
                        c + 3 * dc >= 0 && c + 3 * dc < COLS)
                    {
                        // Check if all 4 pieces match
                        if (board3D[h][r][c] == piece &&
                            board3D[h + dh][r + dr][c + dc] == piece &&
                            board3D[h + 2 * dh][r + 2 * dr][c + 2 * dc] == piece &&
                            board3D[h + 3 * dh][r + 3 * dr][c + 3 * dc] == piece)
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false; // No winning line found
}


// Basic evaluation for 3D
int evaluateBoard3D() {
     if (winningMove3D(AI)) return 100;
     if (winningMove3D(PLAYER)) return -100;
     return 0;
}

int minimax3D(int depth, int alpha, int beta, bool maximizing) {
    if (winningMove3D(PLAYER)) return -100 - depth; // Prioritize faster wins/losses
    if (winningMove3D(AI)) return 100 + depth;
    if (isFull3D()) return 0;
    if (depth == 0) return evaluateBoard3D();

    if (maximizing) {
        int maxEval = INT_MIN;
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (isValidMove3D(r, c)) {
                    int h = makeMove3D(r, c, AI); // Need height for potential undo optimization later if needed
                    if (h != -1) { // Check if move was actually made
                        int eval = minimax3D(depth - 1, alpha, beta, false);
                        undoMove3D(r, c);
                        maxEval = eval > maxEval ? eval : maxEval;
                        alpha = alpha > eval ? alpha : eval;
                        if (beta <= alpha)
                            goto end_maximizing_loop; // Use goto for breaking nested loops
                    }
                }
            }
        }
    end_maximizing_loop:;
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (isValidMove3D(r, c)) {
                     int h = makeMove3D(r, c, PLAYER);
                     if (h != -1) {
                        int eval = minimax3D(depth - 1, alpha, beta, true);
                        undoMove3D(r, c);
                        minEval = eval < minEval ? eval : minEval;
                        beta = beta < eval ? beta : eval;
                        if (beta <= alpha)
                             goto end_minimizing_loop; // Use goto for breaking nested loops
                    }
                }
            }
        }
     end_minimizing_loop:;
        return minEval;
    }
}

void getBestMove3D(int *bestR, int *bestC) {
    int bestScore = INT_MIN;
    *bestR = -1; // Initialize to invalid
    *bestC = -1;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (isValidMove3D(r, c)) {
                 // Check for immediate AI win
                int h_win = makeMove3D(r, c, AI);
                if (h_win != -1) {
                    if (winningMove3D(AI)) {
                        undoMove3D(r, c);
                        *bestR = r;
                        *bestC = c;
                        return; // Found winning move
                    }
                    undoMove3D(r, c);
                }

                 // Check for immediate Player win to block
                 int h_block = makeMove3D(r, c, PLAYER);
                 if (h_block != -1) {
                     if (winningMove3D(PLAYER)) {
                         undoMove3D(r, c);
                         *bestR = r; // Prioritize blocking
                         *bestC = c;
                         bestScore = INT_MAX - 1; // High score, but less than immediate win
                         continue; // Check other moves in case AI can win immediately elsewhere
                     }
                     undoMove3D(r, c);
                 }


                // Run minimax if not blocking an immediate player win
                 if (*bestR != r || *bestC != c || bestScore < INT_MAX -1) {
                    int h_eval = makeMove3D(r, c, AI);
                    if (h_eval != -1) {
                        int score = minimax3D(difficulty, INT_MIN, INT_MAX, false);
                        undoMove3D(r, c);
                        if (score > bestScore) {
                            bestScore = score;
                            *bestR = r;
                            *bestC = c;
                        } else if (*bestR == -1) { // Ensure a move is chosen
                             *bestR = r;
                             *bestC = c;
                        }
                    }
                 }
            }
        }
    }
     // If no move found (e.g., board full, though isFull3D should catch this)
     // Or if all moves lead to loss and bestR/bestC remain -1
    if (*bestR == -1 || *bestC == -1) {
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (isValidMove3D(r, c)) {
                    *bestR = r;
                    *bestC = c;
                    return;
                }
            }
        }
    }
}


bool isFull3D() {
    for (int r = 0; r < ROWS; r++) // Check top level only is sufficient
        for (int c = 0; c < COLS; c++)
            if (board3D[0][r][c] == EMPTY)
                return false;
    return true;
}

// Function to clear the input buffer
void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// ----------------------- RAYLIB VISUALIZATION & GAME LOOP -----------------------

// Function to calculate the 3D position of a piece
Vector3 GetPiecePosition(int h, int r, int c) {
    // Center the 4x4x4 board roughly around origin
    float boardWidth = COLS * SPACING;
    float boardDepth = ROWS * SPACING;
    float boardHeight = HEIGHT * SPACING;

    float x = (c + 0.5f) * SPACING - boardWidth / 2.0f;
    float y = (h + 0.5f) * SPACING; // Height still goes upwards
    float z = (r + 0.5f) * SPACING - boardDepth / 2.0f;
    return (Vector3){ x, y, z };
}

// Raylib drawing function
void drawBoardRaylib() {
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);

    // Draw the wireframe structure for the 4x4x4 grid
    float boardWidth = COLS * SPACING;
    float boardDepth = ROWS * SPACING;
    float boardHeight = HEIGHT * SPACING;
    Vector3 boardCenter = { 0.0f, boardHeight / 2.0f, 0.0f };
    DrawCubeWiresV((Vector3){ boardCenter.x, boardCenter.y - SPACING/2.0f, boardCenter.z }, // Offset Y slightly to center wires around pieces
                   (Vector3){ boardWidth, boardHeight, boardDepth }, LIGHTGRAY);

    // Draw the pieces (Loops updated for 4x4x4)
    for (int h = 0; h < HEIGHT; h++) {
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (board3D[h][r][c] != EMPTY) {
                    Vector3 pos = GetPiecePosition(h, r, c);
                    Color color = (board3D[h][r][c] == PLAYER) ? RED : YELLOW;
                    DrawSphere(pos, PIECE_RADIUS, color);
                }
                // Optional: Draw faint spheres for empty slots
                /* else {
                    Vector3 pos = GetPiecePosition(h, r, c);
                    DrawSphereWires(pos, PIECE_RADIUS * 0.8f, 6, 6, LIGHTGRAY);
                } */
            }
        }
    }

    // Draw the base grid (4x4)
    float gridWidth = COLS * SPACING;
    float gridDepth = ROWS * SPACING;
    float gridY = -0.1f; // Slightly below the first level
    Vector3 centerOffset = { -gridWidth / 2.0f, gridY, -gridDepth / 2.0f };

    // Draw horizontal lines (along Z)
    for (int i = 0; i <= ROWS; i++) {
        DrawLine3D((Vector3){ centerOffset.x, gridY, centerOffset.z + i * SPACING },
                   (Vector3){ centerOffset.x + gridWidth, gridY, centerOffset.z + i * SPACING }, DARKGRAY);
    }
    // Draw vertical lines (along X)
    for (int i = 0; i <= COLS; i++) {
        DrawLine3D((Vector3){ centerOffset.x + i * SPACING, gridY, centerOffset.z },
                   (Vector3){ centerOffset.x + i * SPACING, gridY, centerOffset.z + gridDepth }, DARKGRAY);
    }

    EndMode3D();

    // Draw UI elements
    DrawText("3D Connect Four (Raylib)", 10, 10, 20, DARKGRAY);
    if (gameOver) {
        const char* winText = "";
        if (winner == PLAYER) winText = "Player Wins!";
        else if (winner == AI) winText = "AI Wins!";
        else winText = "It's a Draw!";
        DrawText(winText, GetScreenWidth() / 2 - MeasureText(winText, 40) / 2, GetScreenHeight() / 2 - 20, 40, BLACK);
        DrawText("Press [R] to Restart", GetScreenWidth() / 2 - MeasureText("Press [R] to Restart", 20) / 2, GetScreenHeight() / 2 + 30, 20, DARKGRAY);
    } else {
        const char* turnText = (currentPlayer == PLAYER) ? "Player's Turn" : "AI's Turn";
        DrawText(turnText, 10, 40, 20, (currentPlayer == PLAYER) ? RED : ORANGE);
    }
}

// Game update logic within Raylib loop
void updateGameRaylib() {
    // UpdateCamera(&camera, CAMERA_ORBITAL); // Remove default orbital control
    Vector2 mouseDelta = GetMouseDelta();
        float zoomInput = GetMouseWheelMove();
        float rotateSpeed = 0.005f; // Sensitivity for rotation
        float panSpeed = 0.08f;    // Sensitivity for panning
        float zoomSpeed = 1.2f;     // Sensitivity for zoom
    // Custom Camera Control: Left-click drag to rotate, Wheel to zoom
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // Calculate rotation angles based on mouse delta
            float yawAngle = -mouseDelta.x * rotateSpeed;
            float pitchAngle = -mouseDelta.y * rotateSpeed;

            // Get the vector from target to position
            Vector3 targetToPos = Vector3Subtract(camera.position, camera.target);

            // Rotate around the global UP axis (Y) for yaw
            targetToPos = Vector3RotateByAxisAngle(targetToPos, camera.up, yawAngle);

            // Calculate the camera's right vector (perpendicular to view direction and up)
            Vector3 right = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up));
            // Ensure right vector is valid (avoid issues when looking straight up/down)
            if (fabsf(Vector3DotProduct(right, right)) < 0.001f) {
                 right = Vector3Normalize(Vector3CrossProduct((Vector3){0.0f, 0.0f, 1.0f}, camera.up)); // Use Z if view is aligned with up
                 if (fabsf(Vector3DotProduct(right, right)) < 0.001f) {
                     right = (Vector3){1.0f, 0.0f, 0.0f}; // Use X as last resort
                 }
            }

            // Rotate around the camera's right axis for pitch
            targetToPos = Vector3RotateByAxisAngle(targetToPos, right, pitchAngle);

            // Calculate the new camera position
            camera.position = Vector3Add(camera.target, targetToPos);

    } else {
        // Allow zooming anytime with the mouse wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            // Zoom based on wheel movement
            UpdateCameraPro(&camera, (Vector3){ 0.0f, 0.0f, wheel * -0.5f }, (Vector3){ 0.0f, 0.0f, 0.0f }, 0.0f);
        }
    }

    // Restart Game
    if (gameOver && IsKeyPressed(KEY_R)) {
        memset(board3D, EMPTY, sizeof(board3D));
        currentPlayer = PLAYER;
        gameOver = false;
        winner = EMPTY;
        return; // Skip rest of update on restart
    }

    if (gameOver) return; // Don't process moves if game is over

    // Player's Turn Logic
    if (currentPlayer == PLAYER && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Ray ray = GetMouseRay(GetMousePosition(), camera);

        // Define the bounding box for the base grid (4x4) for click detection
        float gridWidth = COLS * SPACING; // Now 4 * SPACING
        float gridDepth = ROWS * SPACING; // Now 4 * SPACING
        BoundingBox gridBox = {
            (Vector3){ -gridWidth / 2.0f, -0.1f, -gridDepth / 2.0f },
            (Vector3){ gridWidth / 2.0f, 0.1f, gridDepth / 2.0f }
        };

        RayCollision collision = GetRayCollisionBox(ray, gridBox);

        if (collision.hit) {
            // Map collision point to row and column (Logic remains the same, but bounds are 0-3)
            float hitX = collision.point.x + gridWidth / 2.0f;
            float hitZ = collision.point.z + gridDepth / 2.0f;

            int c = (int)(hitX / SPACING);
            int r = (int)(hitZ / SPACING);

            // Clamp values to new bounds (0 to 3)
            if (c < 0) c = 0; if (c >= COLS) c = COLS - 1; // COLS is now 4
            if (r < 0) r = 0; if (r >= ROWS) r = ROWS - 1; // ROWS is now 4

            printf("Clicked grid cell: r=%d, c=%d\n", r, c); // Debug print

            if (isValidMove3D(r, c)) {
                makeMove3D(r, c, PLAYER);
                if (winningMove3D(PLAYER)) {
                    gameOver = true;
                    winner = PLAYER;
                } else if (isFull3D()) {
                    gameOver = true;
                    winner = 3; // Use 3 for Draw consistently
                } else {
                    currentPlayer = AI;
                }
            } else {
                 printf("Invalid move attempt at r=%d, c=%d\n", r, c); // Debug print
            }
        }
    }
    // AI's Turn Logic (Loops inside getBestMove3D/minimax3D implicitly use new bounds)
    else if (currentPlayer == AI) {
        int ai_r, ai_c;
        getBestMove3D(&ai_r, &ai_c);

        if (ai_r != -1 && ai_c != -1) { // Check if a valid move was found
            makeMove3D(ai_r, ai_c, AI);
             printf("AI moved at r=%d, c=%d\n", ai_r, ai_c); // Debug print
            if (winningMove3D(AI)) {
                gameOver = true;
                winner = AI;
            } else if (isFull3D()) {
                gameOver = true;
                winner = 3; // Use 3 for Draw consistently
            } else {
                currentPlayer = PLAYER;
            }
        } else {
            // Should not happen unless board is full and isFull3D() didn't catch it
            printf("AI could not find a move!\n");
            gameOver = true;
            winner = 3; // Use 3 for Draw consistently
        }
    }
}


// ----------------------- MAIN (Modified for Raylib) -----------------------

int main() {
    // Initialization
    const int screenWidth = 800; // Adjusted size
    const int screenHeight = 600; // Adjusted size

    InitWindow(screenWidth, screenHeight, "Sogo (4x4x4 Connect Four) - Raylib"); // Updated Title

    // Define the camera to look into 3D space (Adjusted for 4x4x4)
    camera.position = (Vector3){ (COLS / 2.0f + 4)*SPACING, (HEIGHT + 1)*SPACING, (ROWS + 4)*SPACING }; // Pull back camera slightly more
    camera.target = (Vector3){ 0.0f, (HEIGHT / 2.0f)*SPACING, 0.0f };      // Look at the center of the 4x4x4 board
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera mode type

    // Initialize 3D board (memset is fine for 4x4x4)
    memset(board3D, EMPTY, sizeof(board3D));
    currentPlayer = PLAYER; // Start with player
    gameOver = false;
    winner = EMPTY;

    // TODO: Add difficulty selection logic here if needed, or set a default
    // For now, using the default difficulty = 4

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        updateGameRaylib(); // Handle game logic, input, AI moves

        // Draw
        BeginDrawing();

        drawBoardRaylib(); // Draw the game state

        EndDrawing();
    }

    // De-Initialization
    CloseWindow();                // Close window and OpenGL context

    return 0;
}

// Remove or comment out the old console-based game loop
/*
void playGame3D() {
    // ... (old console game loop code) ...
}
*/
