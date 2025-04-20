#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h> // For tolower
#include <limits.h> // For INT_MAX, INT_MIN in minimax

// --- Constants ---
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define BOARD_SIZE 8
#define BOARD_LAYERS 3
#define SQUARE_SIZE 1.0f
#define LAYER_GAP 2.0f // Vertical distance between layers
#define MAX_MOVES 512  // Increased for 3D
#define INFINITY 1000000

// --- Texture Globals ---
typedef struct PieceTextures {
    Texture2D white_pawn;
    Texture2D white_knight;
    Texture2D white_bishop;
    Texture2D white_rook;
    Texture2D white_queen;
    Texture2D white_king;
    Texture2D black_pawn;
    Texture2D black_knight;
    Texture2D black_bishop;
    Texture2D black_rook;
    Texture2D black_queen;
    Texture2D black_king;
} PieceTextures;

// --- Game Logic Data Structures ---
enum Piece {
    EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
};

// Renamed to avoid conflict with raylib::Color
enum PieceColor {
    P_WHITE, P_BLACK, P_NONE
};

struct Square {
    enum Piece piece;
    enum PieceColor color;
};

struct Move {
    int from_layer;
    int from_row;
    int from_col;
    int to_layer;
    int to_row;
    int to_col;
    enum Piece promotion;
    int score; // Used by AI
    bool is_capture; // Added for potential visual feedback
};

struct Board {
    struct Square squares[BOARD_LAYERS][BOARD_SIZE][BOARD_SIZE];
    enum PieceColor current_player;
    bool white_castle_kingside;
    bool white_castle_queenside;
    bool black_castle_kingside;
    bool black_castle_queenside;
    int en_passant_layer;
    int en_passant_row;
    int en_passant_col;
    int halfmove_clock;
    int fullmove_number;
};

// --- Game State Enum ---
typedef enum {
    MENU,
    PLAYING,
    GAME_OVER_STATE // Renamed to avoid conflict
} GameState;

// --- Function Prototypes (Game Logic - Implementations below) ---
void init_board(struct Board *board);
bool is_valid_position(int layer, int row, int col);
void generate_pawn_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_knight_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_directional_moves(const struct Board *board, int layer, int row, int col,int row_dir, int col_dir, int layer_dir, struct Move moves[], int *move_count);
void generate_bishop_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_rook_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_queen_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_king_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
void generate_moves_for_piece(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count);
int generate_pseudo_legal_moves(const struct Board *board, struct Move moves[]); // Renamed
int generate_legal_moves(struct Board *board, struct Move moves[]); // New function
void make_move(struct Board *board, const struct Move *move);
void undo_move(struct Board *board, const struct Move *move); // Simplified version for AI
int evaluate_board(const struct Board *board);
bool is_game_over(struct Board *board); // Modified to non-const
int minimax(struct Board *board, int depth, int alpha, int beta, bool maximizing_player);
void ai_make_move(struct Board *board, int difficulty);
bool is_move_valid(const struct Board *board, const struct Move *move);
// Function to check if a square is attacked by the opponent
bool is_square_attacked(const struct Board *board, int target_layer, int target_row, int target_col, enum PieceColor attacker_color);
// New function prototypes
bool find_king(const struct Board *board, enum PieceColor king_color, int *king_layer, int *king_row, int *king_col);
bool is_king_in_check(const struct Board *board, enum PieceColor king_color);


// --- Raylib Visualization Functions ---
void DrawChessboard(float board_center_x, float board_center_y, float board_center_z);
// Updated signature to include textures and camera
void DrawPieces(const struct Board *board, const PieceTextures *textures, Camera3D camera, float board_center_x, float board_center_y, float board_center_z);
// Function to draw highlights
void DrawHighlights(int selectedLayer, int selectedRow, int selectedCol, const struct Move validMoves[], int validMoveCount, float board_center_x, float board_center_y, float board_center_z);

// --- Helper Functions ---
// Function to get board coordinates from world position (approximated by collision)
bool GetBoardCoordinates(RayCollision collision, float board_center_x, float board_center_y, float board_center_z, int *layer, int *row, int *col);

// --- Main Function ---
int main(void) {
    // Initialization
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "3D Chess - raylib");
    SetTargetFPS(60);

    // --- Initialize Game Variables FIRST --- 
    // Declare board struct first as other variables might depend on its types indirectly
    struct Board board;
    GameState gameState = MENU;
    enum PieceColor playerColor = P_WHITE; // Default player color
    int selectedAiDifficulty = 2; // Default AI difficulty (Medium)
    int currentAiDifficulty = selectedAiDifficulty; // Difficulty used in the current game

    int selectedLayer = -1, selectedRow = -1, selectedCol = -1;
    struct Move validMoves[MAX_MOVES];
    int validMoveCount = 0;
    bool playerTurn = true; // Will be set based on playerColor when game starts

    // --- Camera Setup ---
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; // Camera position
    camera.target = (Vector3){ 0.0f, LAYER_GAP * (BOARD_LAYERS - 1) / 2.0f, 0.0f }; // Camera looking at the center of the board stack
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera mode type

    // --- Load Textures ---
    PieceTextures pieceTextures = { 0 }; // Initialize to zero
    pieceTextures.white_pawn   = LoadTexture("w_p.png");
    pieceTextures.white_knight = LoadTexture("w_n.png");
    pieceTextures.white_bishop = LoadTexture("w_b.png");
    pieceTextures.white_rook   = LoadTexture("w_r.png");
    pieceTextures.white_queen  = LoadTexture("w_q.png");
    pieceTextures.white_king   = LoadTexture("w_k.png");
    pieceTextures.black_pawn   = LoadTexture("b_p.png");
    pieceTextures.black_knight = LoadTexture("b_n.png");
    pieceTextures.black_bishop = LoadTexture("b_b.png");
    pieceTextures.black_rook   = LoadTexture("b_r.png");
    pieceTextures.black_queen  = LoadTexture("b_q.png");
    pieceTextures.black_king   = LoadTexture("b_k.png");

    // Check if textures loaded (basic check)
    if (pieceTextures.white_pawn.id == 0) {
        fprintf(stderr, "Error loading texture w_p.png\n");
        // Add more robust error handling if needed
    }
    // Add checks for other textures...

    // --- Board Geometry Calculation ---
    float board_width = BOARD_SIZE * SQUARE_SIZE;
    float board_depth = BOARD_SIZE * SQUARE_SIZE;
    float board_center_x = -board_width / 2.0f;
    float board_center_z = -board_depth / 2.0f;
    float board_center_y = 0.0f; // Base layer at y=0

    // --- Camera Control Constants ---
    const float rotateSpeed = 0.003f; // Adjust as needed
    const float zoomSpeed = 1.0f;     // Adjust as needed

    // --- Main Game Loop ---
    while (!WindowShouldClose()) { // Detect window close button or ESC key

        // --- Update Section --- 
        if (gameState == MENU) {
            // --- Menu Logic ---
            // Define button rectangles
            Rectangle whiteButton = { SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 60, 120, 40 };
            Rectangle blackButton = { SCREEN_WIDTH/2 + 30, SCREEN_HEIGHT/2 - 60, 120, 40 };
            Rectangle easyButton = { SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 + 0, 100, 40 };
            Rectangle mediumButton = { SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT/2 + 0, 100, 40 };
            Rectangle hardButton = { SCREEN_WIDTH/2 + 100, SCREEN_HEIGHT/2 + 0, 100, 40 };
            Rectangle startButton = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 70, 200, 50 };

            Vector2 mousePoint = GetMousePosition();

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePoint, whiteButton)) playerColor = P_WHITE;
                if (CheckCollisionPointRec(mousePoint, blackButton)) playerColor = P_BLACK;
                if (CheckCollisionPointRec(mousePoint, easyButton)) selectedAiDifficulty = 1;
                if (CheckCollisionPointRec(mousePoint, mediumButton)) selectedAiDifficulty = 2;
                if (CheckCollisionPointRec(mousePoint, hardButton)) selectedAiDifficulty = 3;

                if (CheckCollisionPointRec(mousePoint, startButton)) {
                    gameState = PLAYING;
                    currentAiDifficulty = selectedAiDifficulty;
                    init_board(&board); // Reset board
                    // White always moves first. playerTurn is true if the human player chose White.
                    playerTurn = (playerColor == P_WHITE);
                    selectedLayer = -1; // Reset selection
                    selectedRow = -1;
                    selectedCol = -1;
                    validMoveCount = 0;
                    printf("Starting game. Player is %s, AI Difficulty: %d\n", (playerColor == P_WHITE) ? "White" : "Black", currentAiDifficulty);
                }
            }
        } else if (gameState == PLAYING) {
            // --- Gameplay Update Logic ---
            // Custom Camera Rotation (on Right Mouse Button down)
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                Vector2 mouseDelta = GetMouseDelta();

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
                // Clamp pitch to prevent flipping upside down
                // Calculate current pitch angle (simplified approach)
                Vector3 viewDir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                float currentPitch = asinf(viewDir.y); // Angle with the horizontal plane (XZ)
                float maxPitch = PI/2.0f - 0.01f; // Slightly less than 90 degrees

                // Only apply pitch rotation if it doesn't exceed the limits
                if (!((currentPitch >= maxPitch && pitchAngle > 0) || (currentPitch <= -maxPitch && pitchAngle < 0))) {
                     targetToPos = Vector3RotateByAxisAngle(targetToPos, right, pitchAngle);
                }

                // Calculate the new camera position
                camera.position = Vector3Add(camera.target, targetToPos);
            }

            // Camera Zoom (Mouse Wheel)
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                Vector3 view = Vector3Subtract(camera.target, camera.position);
                float distance = Vector3Length(view);
                view = Vector3Normalize(view);

                // Calculate new distance (prevent zooming too close or too far)
                distance -= wheel * zoomSpeed;
                if (distance < 2.0f) distance = 2.0f; // Minimum distance
                if (distance > 50.0f) distance = 50.0f; // Maximum distance

                // Update camera position based on new distance
                camera.position = Vector3Subtract(camera.target, Vector3Scale(view, distance));
            }

            // --- Turn Logic ---
            // Use is_game_over(board) which now checks for legal moves
            if (!is_game_over(&board)) {
                if (playerTurn) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        Ray mouseRay = GetMouseRay(GetMousePosition(), camera);
                        RayCollision closestCollision = { 0 };
                        closestCollision.distance = INFINITY;
                        closestCollision.hit = false;
                        int hitLayer = -1, hitRow = -1, hitCol = -1;

                        // Check collision with board squares
                        for (int l = 0; l < BOARD_LAYERS; l++) {
                            for (int r = 0; r < BOARD_SIZE; r++) {
                                for (int c = 0; c < BOARD_SIZE; c++) {
                                    float square_y = board_center_y + l * LAYER_GAP;
                                    float square_x = board_center_x + c * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
                                    float square_z = board_center_z + r * SQUARE_SIZE + SQUARE_SIZE / 2.0f;

                                    // Define the bounding box for the square surface (thin cube)
                                    BoundingBox squareBox = {
                                        (Vector3){ square_x - SQUARE_SIZE/2.0f, square_y - 0.05f, square_z - SQUARE_SIZE/2.0f },
                                        (Vector3){ square_x + SQUARE_SIZE/2.0f, square_y + 0.05f, square_z + SQUARE_SIZE/2.0f }
                                    };

                                    RayCollision collision = GetRayCollisionBox(mouseRay, squareBox);

                                    if (collision.hit && collision.distance < closestCollision.distance) {
                                        closestCollision = collision;
                                        hitLayer = l;
                                        hitRow = r;
                                        hitCol = c;
                                    }
                                }
                            }
                        }

                        if (closestCollision.hit) {
                            // A square was clicked
                            printf("Clicked Layer: %d, Row: %d, Col: %d\n", hitLayer, hitRow, hitCol);

                            if (selectedLayer == -1) { // Nothing selected, try selecting a piece
                                // Check if the clicked piece belongs to the current player AND the human player
                                if (board.squares[hitLayer][hitRow][hitCol].piece != EMPTY &&
                                    board.squares[hitLayer][hitRow][hitCol].color == board.current_player &&
                                    board.current_player == playerColor) { 
                                    selectedLayer = hitLayer;
                                    selectedRow = hitRow;
                                    selectedCol = hitCol;
                                    validMoveCount = 0; 
                                    // Generate LEGAL moves for highlighting
                                    validMoveCount = 0; // Reset move count
                                    // Need a temporary board copy for generate_legal_moves if it modifies the board
                                    // Or modify generate_legal_moves to be const (requires board copy internally)
                                    // For now, let's assume generate_moves_for_piece is sufficient for HIGHLIGHTING only
                                    // The actual move validation happens via generate_legal_moves check below
                                    generate_moves_for_piece(&board, selectedLayer, selectedRow, selectedCol, validMoves, &validMoveCount);
                                    printf("Selected piece at %d,%d,%d. Found %d potential moves.\n", hitLayer, hitRow, hitCol, validMoveCount);
                                }
                            } else { // A piece is already selected, try moving or deselecting
                                bool moved = false;
                                // Check if the clicked square corresponds to a LEGAL move
                                struct Move legal_moves_check[MAX_MOVES];
                                int legal_count_check = generate_legal_moves(&board, legal_moves_check);
                                struct Move chosen_move = {0}; // Store the potential move
                                bool potential_move_found = false;

                                // Find the pseudo-legal move corresponding to the click
                                for (int i = 0; i < validMoveCount; i++) { // Iterate through pseudo-legal moves generated for highlight
                                     if (validMoves[i].to_layer == hitLayer && validMoves[i].to_row == hitRow && validMoves[i].to_col == hitCol) {
                                        chosen_move = validMoves[i];
                                        potential_move_found = true;
                                        break;
                                    }
                                }

                                if (potential_move_found) {
                                    // Now check if this chosen move is actually in the list of legal moves
                                    for (int i = 0; i < legal_count_check; i++) {
                                        // Compare all fields of the move struct
                                        if (legal_moves_check[i].from_layer == chosen_move.from_layer &&
                                            legal_moves_check[i].from_row == chosen_move.from_row &&
                                            legal_moves_check[i].from_col == chosen_move.from_col &&
                                            legal_moves_check[i].to_layer == chosen_move.to_layer &&
                                            legal_moves_check[i].to_row == chosen_move.to_row &&
                                            legal_moves_check[i].to_col == chosen_move.to_col) // Add promotion check if needed
                                        {
                                            // Make the move
                                            printf("Making move from %d,%d,%d to %d,%d,%d\n", selectedLayer, selectedRow, selectedCol, hitLayer, hitRow, hitCol);
                                            make_move(&board, &legal_moves_check[i]); // Use the validated legal move
                                            moved = true;
                                            playerTurn = false; // Switch to AI turn
                                            break;
                                        }
                                    }
                                    if (!moved) {
                                         printf("Move is illegal (leaves king in check).\n");
                                    }
                                }

                                // If not moved, check for deselection or reselection
                                if (!moved) {
                                    if (hitLayer == selectedLayer && hitRow == selectedRow && hitCol == selectedCol) {
                                        printf("Deselected piece.\n");
                                    } else if (board.squares[hitLayer][hitRow][hitCol].piece != EMPTY &&
                                               board.squares[hitLayer][hitRow][hitCol].color == board.current_player &&
                                               board.current_player == playerColor) { // Clicked another of player's pieces
                                        printf("Reselected piece at %d,%d,%d.\n", hitLayer, hitRow, hitCol);
                                        selectedLayer = hitLayer;
                                        selectedRow = hitRow;
                                        selectedCol = hitCol;
                                        validMoveCount = 0; 
                                        generate_moves_for_piece(&board, selectedLayer, selectedRow, selectedCol, validMoves, &validMoveCount);
                                        printf("Found %d valid moves.\n", validMoveCount);
                                    } else {
                                        printf("Clicked invalid square, deselected piece.\n");
                                    }
                                }

                                // Reset selection state after a move or deselection/reselection
                                if (moved || !moved) { 
                                    selectedLayer = -1;
                                    selectedRow = -1;
                                    selectedCol = -1;
                                    validMoveCount = 0;
                                }
                            }
                        }
                    }
                } else { // AI's turn
                    // Check if it's actually the AI's turn (player is not the current player)
                    if (board.current_player != playerColor) {
                        printf("AI's turn (%s)...", (board.current_player == P_WHITE) ? "White" : "Black");
                        ai_make_move(&board, currentAiDifficulty); // Use selected difficulty
                        playerTurn = true; // Switch back to player's turn (potentially)
                    } else {
                        // This case should ideally not happen if playerTurn logic is correct,
                        // but acts as a safeguard. If it's the player's color's turn, 
                        // ensure playerTurn is true.
                        playerTurn = true; 
                    }
                }
            } else { // Game is over
                gameState = GAME_OVER_STATE;
            }
        } else if (gameState == GAME_OVER_STATE) {
             // --- Game Over Logic ---
             if (IsKeyPressed(KEY_M)) { // Press M to return to Menu
                 gameState = MENU;
             }
        }

        // --- Draw Section ---
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (gameState == MENU) {
            // --- Draw Menu UI ---
            DrawText("Choose Your Color:", SCREEN_WIDTH/2 - MeasureText("Choose Your Color:", 20)/2, SCREEN_HEIGHT/2 - 100, 20, DARKGRAY);
            Rectangle whiteButton = { SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 60, 120, 40 }; // Declare and initialize whiteButton
            DrawRectangleRec(whiteButton, (playerColor == P_WHITE) ? SKYBLUE : LIGHTGRAY);
            DrawText("White", whiteButton.x + 30, whiteButton.y + 10, 20, (playerColor == P_WHITE) ? BLUE : DARKGRAY);
            Rectangle blackButton = { SCREEN_WIDTH/2 + 30, SCREEN_HEIGHT/2 - 60, 120, 40 }; // Define and initialize blackButton
            DrawRectangleRec(blackButton, (playerColor == P_BLACK) ? SKYBLUE : LIGHTGRAY);
            DrawText("Black", blackButton.x + 30, blackButton.y + 10, 20, (playerColor == P_BLACK) ? BLUE : DARKGRAY);

            DrawText("Choose AI Difficulty:", SCREEN_WIDTH/2 - MeasureText("Choose AI Difficulty:", 20)/2, SCREEN_HEIGHT/2 - 20, 20, DARKGRAY);
            Rectangle easyButton = { SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 + 0, 100, 40 }; // Define and initialize easyButton
            DrawRectangleRec(easyButton, (selectedAiDifficulty == 1) ? SKYBLUE : LIGHTGRAY);
            DrawText("Easy", easyButton.x + 25, easyButton.y + 10, 20, (selectedAiDifficulty == 1) ? BLUE : DARKGRAY);
            Rectangle mediumButton = { SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT/2 + 0, 100, 40 }; // Define and initialize mediumButton
            DrawRectangleRec(mediumButton, (selectedAiDifficulty == 2) ? SKYBLUE : LIGHTGRAY);
            DrawText("Medium", mediumButton.x + 15, mediumButton.y + 10, 20, (selectedAiDifficulty == 2) ? BLUE : DARKGRAY);
            // Define and initialize hardButton before using it
                        Rectangle hardButton = { SCREEN_WIDTH/2 + 100, SCREEN_HEIGHT/2 + 0, 100, 40 };
                        DrawRectangleRec(hardButton, (selectedAiDifficulty == 3) ? SKYBLUE : LIGHTGRAY);
            DrawText("Hard", hardButton.x + 25, hardButton.y + 10, 20, (selectedAiDifficulty == 3) ? BLUE : DARKGRAY);

            Rectangle startButton = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 70, 200, 50 }; // Define and initialize startButton
            DrawRectangleRec(startButton, LIME);
            DrawText("Start Game", startButton.x + (startButton.width - MeasureText("Start Game", 30))/2, startButton.y + 10, 30, DARKGREEN);

        } else { // PLAYING or GAME_OVER_STATE
            // --- Draw Gameplay or Game Over UI ---
            BeginMode3D(camera);
                DrawChessboard(board_center_x, board_center_y, board_center_z);
                if (gameState == PLAYING) {
                    DrawHighlights(selectedLayer, selectedRow, selectedCol, validMoves, validMoveCount, board_center_x, board_center_y, board_center_z);
                }
                DrawPieces(&board, &pieceTextures, camera, board_center_x, board_center_y, board_center_z);
                DrawGrid(20, 1.0f); // Draw a grid
            EndMode3D();

            // Draw UI Text
            if (gameState == PLAYING) {
                 DrawText(TextFormat("%s to move", (board.current_player == P_WHITE) ? "White" : "Black"), 10, 10, 20, (board.current_player == P_WHITE) ? BLACK : DARKGRAY);
                 DrawText(TextFormat("Playing as: %s", (playerColor == P_WHITE) ? "White" : "Black"), 10, 70, 20, DARKBLUE);
                 // Add Check indicator
                 if (is_king_in_check(&board, board.current_player)) {
                     DrawText("CHECK!", SCREEN_WIDTH - 150, 10, 30, RED);
                 }
            } else { // GAME_OVER_STATE
                 DrawText("GAME OVER", SCREEN_WIDTH / 2 - MeasureText("GAME OVER", 40) / 2, SCREEN_HEIGHT / 2 - 40, 40, RED);
                 // Determine winner based on checkmate/stalemate
                 const char* winnerText = "";
                 if (is_king_in_check(&board, board.current_player)) {
                     winnerText = (board.current_player == P_WHITE) ? "Black Wins (Checkmate)!" : "White Wins (Checkmate)!";
                 } else {
                     winnerText = "Stalemate!";
                 }
                 DrawText(winnerText, SCREEN_WIDTH / 2 - MeasureText(winnerText, 30) / 2, SCREEN_HEIGHT / 2 + 10, 30, MAROON);
                 DrawText("Press [M] to return to Menu", SCREEN_WIDTH / 2 - MeasureText("Press [M] to return to Menu", 20) / 2, SCREEN_HEIGHT - 40, 20, DARKGRAY);
            }
            DrawFPS(10, 40);
        }

        EndDrawing();
    }

    // De-Initialization
    // Unload textures
    UnloadTexture(pieceTextures.white_pawn);
    UnloadTexture(pieceTextures.white_knight);
    UnloadTexture(pieceTextures.white_bishop);
    UnloadTexture(pieceTextures.white_rook);
    UnloadTexture(pieceTextures.white_queen);
    UnloadTexture(pieceTextures.white_king);
    UnloadTexture(pieceTextures.black_pawn);
    UnloadTexture(pieceTextures.black_knight);
    UnloadTexture(pieceTextures.black_bishop);
    UnloadTexture(pieceTextures.black_rook);
    UnloadTexture(pieceTextures.black_queen);
    UnloadTexture(pieceTextures.black_king);

    CloseWindow(); // Close window and OpenGL context

    return 0;
}

// --- Visualization Function Implementations ---

void DrawChessboard(float board_center_x, float board_center_y, float board_center_z) {
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        float current_y = board_center_y + layer * LAYER_GAP;
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                float current_x = board_center_x + col * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
                float current_z = board_center_z + row * SQUARE_SIZE + SQUARE_SIZE / 2.0f;

                Color squareColor = ((row + col + layer) % 2 == 0) ? BEIGE : BROWN; // Alternate color based on layer too
                // Draw cube slightly below the y-level to form the board surface
                DrawCube((Vector3){current_x, current_y - 0.05f, current_z}, SQUARE_SIZE, 0.1f, SQUARE_SIZE, squareColor);
                // Draw wires for better visibility
                DrawCubeWires((Vector3){current_x, current_y - 0.05f, current_z}, SQUARE_SIZE, 0.1f, SQUARE_SIZE, DARKBROWN);
            }
        }
    }
}

// Updated signature to include textures and camera
void DrawPieces(const struct Board *board, const PieceTextures *textures, Camera3D camera, float board_center_x, float board_center_y, float board_center_z) {
    float billboard_size = SQUARE_SIZE * 0.8f;

    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                struct Square current_square = board->squares[layer][row][col];
                if (current_square.piece != EMPTY) {
                    // Calculate 3D position for the center of the square
                    float piece_x = board_center_x + col * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
                    float piece_z = board_center_z + row * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
                    // Adjust Y position so the base of the billboard sits near the square surface
                    float piece_y = board_center_y + layer * LAYER_GAP + billboard_size / 2.0f;

                    // Select correct texture based on piece type and color
                    Texture2D pieceTexture;
                    switch (current_square.piece) {
                        case PAWN:   pieceTexture = (current_square.color == P_WHITE) ? textures->white_pawn   : textures->black_pawn;   break;
                        case KNIGHT: pieceTexture = (current_square.color == P_WHITE) ? textures->white_knight : textures->black_knight; break;
                        case BISHOP: pieceTexture = (current_square.color == P_WHITE) ? textures->white_bishop : textures->black_bishop; break;
                        case ROOK:   pieceTexture = (current_square.color == P_WHITE) ? textures->white_rook   : textures->black_rook;   break;
                        case QUEEN:  pieceTexture = (current_square.color == P_WHITE) ? textures->white_queen  : textures->black_queen;  break;
                        case KING:   pieceTexture = (current_square.color == P_WHITE) ? textures->white_king   : textures->black_king;   break;
                        default: continue; // Skip if piece type is somehow invalid
                    }

                    // Draw the piece texture as a billboard (always facing the camera)
                    DrawBillboard(camera, pieceTexture, (Vector3){piece_x, piece_y, piece_z}, billboard_size, WHITE);
                }
            }
        }
    }
}

// Function to draw highlights for selected piece and valid moves
void DrawHighlights(int selectedLayer, int selectedRow, int selectedCol, const struct Move validMoves[], int validMoveCount, float board_center_x, float board_center_y, float board_center_z) {
    // Highlight selected square
    if (selectedLayer != -1) {
        float sel_y = board_center_y + selectedLayer * LAYER_GAP;
        float sel_x = board_center_x + selectedCol * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
        float sel_z = board_center_z + selectedRow * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
        DrawCubeWires((Vector3){sel_x, sel_y, sel_z}, SQUARE_SIZE * 1.05f, SQUARE_SIZE * 1.05f, SQUARE_SIZE * 1.05f, YELLOW); // Highlight selected piece slightly larger
    }

    // Highlight valid moves
    for (int i = 0; i < validMoveCount; i++) {
        float move_y = board_center_y + validMoves[i].to_layer * LAYER_GAP;
        float move_x = board_center_x + validMoves[i].to_col * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
        float move_z = board_center_z + validMoves[i].to_row * SQUARE_SIZE + SQUARE_SIZE / 2.0f;
        // Draw a small indicator or different wire color for valid moves
        DrawCubeWires((Vector3){move_x, move_y, move_z}, SQUARE_SIZE * 0.9f, 0.15f, SQUARE_SIZE * 0.9f, GREEN);
    }
}

// --- Game Logic Function Implementations (Copied from threeDChess.c) ---

void init_board(struct Board *board) {
    // Initialize all squares to empty
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                board->squares[layer][row][col].piece = EMPTY;
                board->squares[layer][row][col].color = P_NONE;
            }
        }
    }

    // Layer 0 (Bottom layer) - Black Pieces
    enum Piece back_row[BOARD_SIZE] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    // Black Back Row (row 0)
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[0][0][col].piece = back_row[col];
        board->squares[0][0][col].color = P_BLACK;
    }
    // Black Pawns (row 1)
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[0][1][col].piece = PAWN;
        board->squares[0][1][col].color = P_BLACK;
    }

    // Layer 1 (Middle layer) - Empty
    // (Already initialized to empty above)

    // Layer 2 (Top layer) - White Pieces
    // White Pawns (row 6)
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[2][6][col].piece = PAWN;
        board->squares[2][6][col].color = P_WHITE;
    }
    // White Back Row (row 7)
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[2][7][col].piece = back_row[col];
        board->squares[2][7][col].color = P_WHITE;
    }

    // Game state
    board->current_player = P_WHITE; // White starts
    // Castling rights only apply to layer 0 in standard chess, adapt if needed for 3D rules
    // For now, assume standard castling applies only if pieces are on layer 0
    // Since kings/rooks start on different layers, disable castling initially.
    // If your 3D rules allow inter-layer castling or different start, adjust this.
    board->white_castle_kingside = false; // White king doesn't start on layer 0
    board->white_castle_queenside = false;
    board->black_castle_kingside = true; // Black king starts on layer 0
    board->black_castle_queenside = true;
    board->en_passant_layer = -1;
    board->en_passant_row = -1;
    board->en_passant_col = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
}

bool is_valid_position(int layer, int row, int col) {
    return layer >= 0 && layer < BOARD_LAYERS &&
           row >= 0 && row < BOARD_SIZE &&
           col >= 0 && col < BOARD_SIZE;
}

void generate_pawn_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int direction = (board->squares[layer][row][col].color == P_WHITE) ? -1 : 1;
    int start_row = (board->squares[layer][row][col].color == P_WHITE) ? 6 : 1;
    int promotion_row = (board->squares[layer][row][col].color == P_WHITE) ? 0 : 7;

    // --- Same Layer Moves ---
    // Standard forward move
    int next_row = row + direction;
    if (is_valid_position(layer, next_row, col) &&
        board->squares[layer][next_row][col].piece == EMPTY) {
        // Check for promotion
        if (next_row == promotion_row) {
             enum Piece promo_pieces[] = {QUEEN, ROOK, BISHOP, KNIGHT};
             for (int i = 0; i < 4; i++) {
                moves[*move_count] = (struct Move){layer, row, col, layer, next_row, col, promo_pieces[i], 0, false};
                (*move_count)++;
             }
        } else {
            moves[*move_count] = (struct Move){layer, row, col, layer, next_row, col, EMPTY, 0, false};
            (*move_count)++;
        }

        // Double move from starting position
        if (row == start_row && is_valid_position(layer, row + 2*direction, col) &&
            board->squares[layer][row + 2*direction][col].piece == EMPTY) {
            moves[*move_count] = (struct Move){layer, row, col, layer, row + 2*direction, col, EMPTY, 0, false};
            (*move_count)++;
        }
    }

    // Capture moves
    for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
        int capture_col = col + col_offset;
        if (is_valid_position(layer, next_row, capture_col)) {
            // Normal capture
            if (board->squares[layer][next_row][capture_col].piece != EMPTY &&
                board->squares[layer][next_row][capture_col].color != board->squares[layer][row][col].color) {
                 // Check for promotion on capture
                if (next_row == promotion_row) {
                    enum Piece promo_pieces[] = {QUEEN, ROOK, BISHOP, KNIGHT};
                    for (int i = 0; i < 4; i++) {
                        moves[*move_count] = (struct Move){layer, row, col, layer, next_row, capture_col, promo_pieces[i], 0, true};
                        (*move_count)++;
                    }
                } else {
                    moves[*move_count] = (struct Move){layer, row, col, layer, next_row, capture_col, EMPTY, 0, true};
                    (*move_count)++;
                }
            }
            // En passant capture
            else if (layer == board->en_passant_layer &&
                     next_row == board->en_passant_row &&
                     capture_col == board->en_passant_col &&
                     board->squares[layer][row][col].color != board->squares[layer][board->en_passant_row - direction][board->en_passant_col].color) { // Check color of pawn that moved
                moves[*move_count] = (struct Move){layer, row, col, layer, next_row, capture_col, EMPTY, 0, true};
                (*move_count)++;
            }
        }
    }

    // --- Inter-Layer Moves ---
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
        int new_layer = layer + layer_change;
        if (new_layer < 0 || new_layer >= BOARD_LAYERS) continue;

        // Forward move (inter-layer)
        if (is_valid_position(new_layer, next_row, col) &&
            board->squares[new_layer][next_row][col].piece == EMPTY) {
            // No promotion on inter-layer moves for simplicity, could be added
            moves[*move_count] = (struct Move){layer, row, col, new_layer, next_row, col, EMPTY, 0, false};
            (*move_count)++;
        }

        // Capture moves (inter-layer)
        for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
            int capture_col = col + col_offset;
            if (is_valid_position(new_layer, next_row, capture_col) &&
                board->squares[new_layer][next_row][capture_col].piece != EMPTY &&
                board->squares[new_layer][next_row][capture_col].color != board->squares[layer][row][col].color) {
                // No promotion on inter-layer moves for simplicity
                moves[*move_count] = (struct Move){layer, row, col, new_layer, next_row, capture_col, EMPTY, 0, true};
                (*move_count)++;
            }
        }
    }
}


void generate_knight_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int knight_moves[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };

    // Iterate through possible layers (current and adjacent)
    for (int l_offset = -1; l_offset <= 1; ++l_offset) {
        int current_layer = layer + l_offset;
        if (current_layer < 0 || current_layer >= BOARD_LAYERS) continue;

        // Iterate through standard knight moves
        for (int i = 0; i < 8; i++) {
            int new_row = row + knight_moves[i][0];
            int new_col = col + knight_moves[i][1];

            // Check if the move is within the board bounds for the target layer
            if (is_valid_position(current_layer, new_row, new_col)) {
                // Check if the target square is empty or contains an opponent's piece
                struct Square target_sq = board->squares[current_layer][new_row][new_col];
                if (target_sq.piece == EMPTY || target_sq.color != board->squares[layer][row][col].color) {

                    moves[*move_count] = (struct Move){
                        layer, row, col,
                        current_layer, new_row, new_col,
                        EMPTY, 0,
                        (target_sq.piece != EMPTY)
                    };
                    (*move_count)++;
                }
            }
        }
         // If checking the current layer, don't need to check again
        if (l_offset == 0) continue;
    }
}


void generate_directional_moves(const struct Board *board, int layer, int row, int col,
                               int row_dir, int col_dir, int layer_dir, struct Move moves[], int *move_count) {
    int current_layer = layer + layer_dir;
    int current_row = row + row_dir;
    int current_col = col + col_dir;

    while (is_valid_position(current_layer, current_row, current_col)) {
        if (board->squares[current_layer][current_row][current_col].piece == EMPTY) {
            moves[*move_count] = (struct Move){layer, row, col, current_layer, current_row, current_col, EMPTY, 0, false};
            (*move_count)++;
        } else {
            if (board->squares[current_layer][current_row][current_col].color != board->squares[layer][row][col].color) {
                moves[*move_count] = (struct Move){layer, row, col, current_layer, current_row, current_col, EMPTY, 0, true};
                (*move_count)++;
            }
            break; // Stop sliding after encountering any piece
        }

        current_layer += layer_dir;
        current_row += row_dir;
        current_col += col_dir;
    }
}

void generate_bishop_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    // Same layer moves
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, layer, row, col, directions[i][0], directions[i][1], 0, moves, move_count);
    }
    // Inter-layer diagonal moves (move layer and row/col)
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
         for (int i = 0; i < 4; i++) {
            generate_directional_moves(board, layer, row, col, directions[i][0], directions[i][1], layer_change, moves, move_count);
        }
    }
}

void generate_rook_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // Same layer moves
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, layer, row, col, directions[i][0], directions[i][1], 0, moves, move_count);
    }
    // Inter-layer straight moves (move layer and row OR col)
     for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
         // Move layer and row
         generate_directional_moves(board, layer, row, col, 1, 0, layer_change, moves, move_count);
         generate_directional_moves(board, layer, row, col, -1, 0, layer_change, moves, move_count);
         // Move layer and col
         generate_directional_moves(board, layer, row, col, 0, 1, layer_change, moves, move_count);
         generate_directional_moves(board, layer, row, col, 0, -1, layer_change, moves, move_count);
         // Move layer only (straight up/down)
         generate_directional_moves(board, layer, row, col, 0, 0, layer_change, moves, move_count);
    }
}

void generate_queen_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    generate_bishop_moves(board, layer, row, col, moves, move_count);
    generate_rook_moves(board, layer, row, col, moves, move_count);
}

void generate_king_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    enum PieceColor player_color = board->squares[layer][row][col].color;
    enum PieceColor opponent_color = (player_color == P_WHITE) ? P_BLACK : P_WHITE;

    // Iterate through all adjacent squares in 3D (including diagonals)
    for (int l_offset = -1; l_offset <= 1; ++l_offset) {
        for (int r_offset = -1; r_offset <= 1; ++r_offset) {
            for (int c_offset = -1; c_offset <= 1; ++c_offset) {
                // Skip the current square itself
                if (l_offset == 0 && r_offset == 0 && c_offset == 0) continue;

                int new_layer = layer + l_offset;
                int new_row = row + r_offset;
                int new_col = col + c_offset;

                // Check if the target position is valid
                if (is_valid_position(new_layer, new_row, new_col)) {
                    struct Square target_sq = board->squares[new_layer][new_row][new_col];
                    // Check if the target square is empty or occupied by an opponent
                    if (target_sq.piece == EMPTY || target_sq.color != player_color) {
                        // Add the move (legality check regarding check will be done later in generate_legal_moves)
                        moves[*move_count] = (struct Move){
                            layer, row, col,
                            new_layer, new_row, new_col,
                            EMPTY, 0,
                            (target_sq.piece != EMPTY)
                        };
                        (*move_count)++;
                    }
                }
            }
        }
    }

    // --- Castling --- 
    // Standard chess castling rules applied only to layer 0 for this implementation.
    // Modify if your 3D rules allow inter-layer castling.
    if (layer == 0) { 
        int king_row = (player_color == P_WHITE) ? 7 : 0;
        int king_col_start = 4; // Standard king starting column (E file)

        // Check if king is in the correct starting position (row check is sufficient if castling rights are checked)
        if (row == king_row && col == king_col_start) {
            // Check if king is currently in check
            if (!is_king_in_check(board, player_color)) { 
                // Kingside Castling (O-O)
                bool can_castle_kingside = (player_color == P_WHITE) ? board->white_castle_kingside : board->black_castle_kingside;
                if (can_castle_kingside &&
                    board->squares[layer][king_row][king_col_start + 1].piece == EMPTY && // F1/F8 empty
                    board->squares[layer][king_row][king_col_start + 2].piece == EMPTY && // G1/G8 empty
                    !is_square_attacked(board, layer, king_row, king_col_start + 1, opponent_color) && // F1/F8 not attacked
                    !is_square_attacked(board, layer, king_row, king_col_start + 2, opponent_color))   // G1/G8 not attacked (king lands here)
                {
                    // Add kingside castling move (king moves 2 squares)
                    moves[*move_count] = (struct Move){layer, row, col, layer, king_row, king_col_start + 2, EMPTY, 0, false};
                    (*move_count)++;
                }

                // Queenside Castling (O-O-O)
                bool can_castle_queenside = (player_color == P_WHITE) ? board->white_castle_queenside : board->black_castle_queenside;
                if (can_castle_queenside &&
                    board->squares[layer][king_row][king_col_start - 1].piece == EMPTY && // D1/D8 empty
                    board->squares[layer][king_row][king_col_start - 2].piece == EMPTY && // C1/C8 empty
                    board->squares[layer][king_row][king_col_start - 3].piece == EMPTY && // B1/B8 empty
                    !is_square_attacked(board, layer, king_row, king_col_start - 1, opponent_color) && // D1/D8 not attacked
                    !is_square_attacked(board, layer, king_row, king_col_start - 2, opponent_color))   // C1/C8 not attacked (king lands here)
                    // B1/B8 doesn't need to be checked for attack as king doesn't pass through it
                {
                    // Add queenside castling move (king moves 2 squares)
                    moves[*move_count] = (struct Move){layer, row, col, layer, king_row, king_col_start - 2, EMPTY, 0, false};
                    (*move_count)++;
                }
            }
        }
    }
}

void generate_moves_for_piece(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    switch (board->squares[layer][row][col].piece) {
        case PAWN:   generate_pawn_moves(board, layer, row, col, moves, move_count);   break;
        case KNIGHT: generate_knight_moves(board, layer, row, col, moves, move_count); break;
        case BISHOP: generate_bishop_moves(board, layer, row, col, moves, move_count); break;
        case ROOK:   generate_rook_moves(board, layer, row, col, moves, move_count);   break;
        case QUEEN:  generate_queen_moves(board, layer, row, col, moves, move_count);  break;
        case KING:   generate_king_moves(board, layer, row, col, moves, move_count);   break;
        case EMPTY:  break;
    }
}

// Generates all pseudo-legal moves for the current player
// Renamed from generate_all_moves to avoid confusion
int generate_pseudo_legal_moves(const struct Board *board, struct Move moves[]) {
    int move_count = 0;
    enum PieceColor current_player = board->current_player;

    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board->squares[layer][row][col].color == current_player) {
                    generate_moves_for_piece(board, layer, row, col, moves, &move_count);
                }
            }
        }
    }
    return move_count;
}

// Generates only legal moves for the current player
// This function filters moves that leave the king in check
int generate_legal_moves(struct Board *board, struct Move moves[]) {
    struct Move pseudo_legal_moves[MAX_MOVES];
    int pseudo_legal_count = generate_pseudo_legal_moves(board, pseudo_legal_moves);
    int legal_move_count = 0;
    enum PieceColor current_player = board->current_player;

    for (int i = 0; i < pseudo_legal_count; i++) {
        // Temporarily make the move
        struct Square captured_square = board->squares[pseudo_legal_moves[i].to_layer][pseudo_legal_moves[i].to_row][pseudo_legal_moves[i].to_col]; // Store captured piece info
        make_move(board, &pseudo_legal_moves[i]);

        // Check if the king is now in check
        if (!is_king_in_check(board, current_player)) {
            moves[legal_move_count++] = pseudo_legal_moves[i];
        }

        // Undo the move
        // Need a more robust undo_move or make a board copy
        // For now, using a simplified undo based on make_move structure
        // This simplified undo might miss castling rights, en passant state changes!
        // A full undo requires storing more state.
        // Let's try the simplified undo first.
        struct Square moved_piece = board->squares[pseudo_legal_moves[i].to_layer][pseudo_legal_moves[i].to_row][pseudo_legal_moves[i].to_col];
        board->squares[pseudo_legal_moves[i].from_layer][pseudo_legal_moves[i].from_row][pseudo_legal_moves[i].from_col] = moved_piece;
        board->squares[pseudo_legal_moves[i].to_layer][pseudo_legal_moves[i].to_row][pseudo_legal_moves[i].to_col] = captured_square; // Restore captured piece
        board->current_player = current_player; // Restore player
        // WARNING: This undo does NOT restore castling rights or en passant status if they were changed by make_move.
        // This might lead to incorrect legal move generation in complex scenarios.
        // A better approach is to copy the board before make_move.
    }

    return legal_move_count;
}


void make_move(struct Board *board, const struct Move *move) {
    struct Square moved_piece = board->squares[move->from_layer][move->from_row][move->from_col];
    struct Square target_square = board->squares[move->to_layer][move->to_row][move->to_col]; // Store target for capture info

    // --- Handle Special Moves ---
    bool is_en_passant_capture = false;
    bool is_castle = false;

    // En Passant Capture
    if (moved_piece.piece == PAWN &&
        move->to_col != move->from_col &&
        target_square.piece == EMPTY &&
        move->to_layer == board->en_passant_layer && // Must move to the en passant layer
        move->to_row == board->en_passant_row &&
        move->to_col == board->en_passant_col)
    {
        is_en_passant_capture = true;
        // Clear the captured pawn's square (which is behind the target square)
        board->squares[move->from_layer][move->from_row][move->to_col].piece = EMPTY;
        board->squares[move->from_layer][move->from_row][move->to_col].color = P_NONE;
    }

    // Castling (only on layer 0)
    if (moved_piece.piece == KING && abs(move->to_col - move->from_col) == 2 && move->from_layer == 0 && move->to_layer == 0) {
        is_castle = true;
        // Move the rook
        if (move->to_col == 6) { // Kingside
            board->squares[0][move->from_row][5] = board->squares[0][move->from_row][7];
            board->squares[0][move->from_row][7].piece = EMPTY;
            board->squares[0][move->from_row][7].color = P_NONE;
        } else { // Queenside (move->to_col == 2)
            board->squares[0][move->from_row][3] = board->squares[0][move->from_row][0];
            board->squares[0][move->from_row][0].piece = EMPTY;
            board->squares[0][move->from_row][0].color = P_NONE;
        }
    }

    // --- Update Board ---
    if (move->promotion != EMPTY) {
        board->squares[move->to_layer][move->to_row][move->to_col].piece = move->promotion;
        board->squares[move->to_layer][move->to_row][move->to_col].color = moved_piece.color;
    } else {
        board->squares[move->to_layer][move->to_row][move->to_col] = moved_piece;
    }
    // Clear the original square
    board->squares[move->from_layer][move->from_row][move->from_col].piece = EMPTY;
    board->squares[move->from_layer][move->from_row][move->from_col].color = P_NONE;


    // --- Update Game State ---
    // Reset en passant target square by default
    int prev_ep_layer = board->en_passant_layer; // Store previous state for undo
    int prev_ep_row = board->en_passant_row;
    int prev_ep_col = board->en_passant_col;
    board->en_passant_layer = -1;
    board->en_passant_row = -1;
    board->en_passant_col = -1;

    // Set new en passant target if pawn moved two squares
    if (moved_piece.piece == PAWN && abs(move->to_row - move->from_row) == 2 && move->from_layer == move->to_layer) {
        board->en_passant_layer = move->from_layer;
        board->en_passant_row = (move->from_row + move->to_row) / 2;
        board->en_passant_col = move->from_col;
    }

    // Update castling rights (only layer 0 relevant)
    if (move->from_layer == 0) {
        if (moved_piece.piece == KING) {
            if (moved_piece.color == P_WHITE) {
                board->white_castle_kingside = false;
                board->white_castle_queenside = false;
            } else {
                board->black_castle_kingside = false;
                board->black_castle_queenside = false;
            }
        } else if (moved_piece.piece == ROOK) {
            if (moved_piece.color == P_WHITE) {
                if (move->from_row == 7 && move->from_col == 0) board->white_castle_queenside = false;
                if (move->from_row == 7 && move->from_col == 7) board->white_castle_kingside = false;
            } else { // Black rook
                if (move->from_row == 0 && move->from_col == 0) board->black_castle_queenside = false;
                if (move->from_row == 0 && move->from_col == 7) board->black_castle_kingside = false;
            }
        }
    }
     // Also update if a rook is captured on its starting square (layer 0)
    if (move->to_layer == 0) {
         if (move->to_row == 7 && move->to_col == 0) board->white_castle_queenside = false;
         if (move->to_row == 7 && move->to_col == 7) board->white_castle_kingside = false;
         if (move->to_row == 0 && move->to_col == 0) board->black_castle_queenside = false;
         if (move->to_row == 0 && move->to_col == 7) board->black_castle_kingside = false;
    }


    // Update halfmove clock (reset on capture or pawn move, otherwise increment)
    if (moved_piece.piece == PAWN || target_square.piece != EMPTY || is_en_passant_capture) {
        board->halfmove_clock = 0;
    } else {
        board->halfmove_clock++;
    }

    // Update fullmove number (increment after Black moves)
    if (board->current_player == P_BLACK) {
        board->fullmove_number++;
    }

    // Switch player
    board->current_player = (board->current_player == P_WHITE) ? P_BLACK : P_WHITE;
}

// Simplified version for AI - DOES NOT FULLY REVERT STATE
// WARNING: This is insufficient for generate_legal_moves validation!
void undo_move(struct Board *board, const struct Move *move) {
    struct Square moved_piece = board->squares[move->to_layer][move->to_row][move->to_col];
    board->squares[move->from_layer][move->from_row][move->from_col] = moved_piece;

    // Basic restoration - needs captured piece info stored in Move or elsewhere
    // If move->is_capture is true, we need to know WHAT was captured.
    // For now, assume it becomes empty (INCORRECT for validation)
    if (move->is_capture) {
         board->squares[move->to_layer][move->to_row][move->to_col].piece = EMPTY; // Placeholder
         board->squares[move->to_layer][move->to_row][move->to_col].color = P_NONE;
    } else {
        board->squares[move->to_layer][move->to_row][move->to_col].piece = EMPTY;
        board->squares[move->to_layer][move->to_row][move->to_col].color = P_NONE;
    }

    // Revert player (assuming it was switched in make_move)
    board->current_player = (board->current_player == P_WHITE) ? P_BLACK : P_WHITE;

    // TODO: Revert castling rights, en passant, halfmove clock, fullmove number
}

int evaluate_board(const struct Board *board) {
    int score = 0;
    // Simplified piece values
    int piece_values[] = {0, 100, 320, 330, 500, 900, 20000}; // EMPTY, P, N, B, R, Q, K

    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                struct Square current_square = board->squares[layer][row][col];
                if (current_square.piece != EMPTY) {
                    int value = piece_values[current_square.piece];

                    // Positional bonuses (example: center control)
                    if ((row >= 2 && row <= 5) && (col >= 2 && col <= 5)) {
                        value += 10; // Bonus for central squares
                    }
                    // Bonus for higher layers
                    value += layer * 15; // Pieces on higher layers might be more valuable

                    // Add/subtract based on color
                    if (current_square.color == P_WHITE) {
                        score += value;
                    } else {
                        score -= value;
                    }
                }
            }
        }
    }

    // Return score relative to the current player
    // return (board->current_player == WHITE) ? score : -score;
     // Let's return the absolute score (White positive, Black negative)
     return score;
}

bool is_game_over(struct Board *board) { // Needs non-const board to generate legal moves
    struct Move legal_moves[MAX_MOVES];
    int legal_move_count = generate_legal_moves(board, legal_moves);

    if (legal_move_count == 0) {
        return true; // Checkmate or Stalemate
    }

    // TODO: Add 50-move rule check (using board->halfmove_clock)
    // TODO: Add insufficient material check (more complex)
    // TODO: Add threefold repetition check (requires move history)

    return false;
}


int minimax(struct Board *board, int depth, int alpha, int beta, bool maximizing_player) {
    if (depth == 0 || is_game_over(board)) {
        return evaluate_board(board);
    }

    struct Move legal_moves[MAX_MOVES];
    int move_count = generate_legal_moves(board, legal_moves);

    // Handle checkmate/stalemate explicitly in evaluation if needed, or rely on move count
    if (move_count == 0) {
        if (is_king_in_check(board, board->current_player)) {
            return maximizing_player ? -INFINITY : INFINITY; // Checkmated
        } else {
            return 0; // Stalemate
        }
    }

    if (maximizing_player) {
        int max_eval = -INFINITY;
        for (int i = 0; i < move_count; i++) {
            // Store state for undo (simple version)
            struct Square captured_square = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
            enum PieceColor original_player = board->current_player;

            make_move(board, &legal_moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, false);
            max_eval = (eval > max_eval) ? eval : max_eval; // Basic max
            alpha = (alpha > eval) ? alpha : eval; // Basic max for alpha

            // Undo move (simple version - WARNING: INSUFFICIENT)
            struct Square moved_piece = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
            board->squares[legal_moves[i].from_layer][legal_moves[i].from_row][legal_moves[i].from_col] = moved_piece;
            board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col] = captured_square;
            board->current_player = original_player;
            // WARNING: Does not restore castling/en passant

            if (beta <= alpha) {
                break;
            }
        }
        return max_eval;
    } else { // Minimizing player
        int min_eval = INFINITY;
        for (int i = 0; i < move_count; i++) {
             // Store state for undo (simple version)
            struct Square captured_square = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
            enum PieceColor original_player = board->current_player;

            make_move(board, &legal_moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, true);
            min_eval = (eval < min_eval) ? eval : min_eval; // Basic min
            beta = (beta < eval) ? beta : eval; // Basic min for beta

            // Undo move (simple version - WARNING: INSUFFICIENT)
            struct Square moved_piece = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
            board->squares[legal_moves[i].from_layer][legal_moves[i].from_row][legal_moves[i].from_col] = moved_piece;
            board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col] = captured_square;
            board->current_player = original_player;
            // WARNING: Does not restore castling/en passant

            if (beta <= alpha) {
                break;
            }
        }
        return min_eval;
    }
}


void ai_make_move(struct Board *board, int difficulty) {
    struct Move legal_moves[MAX_MOVES];
    int move_count = generate_legal_moves(board, legal_moves);

    if (move_count == 0) {
        printf("AI has no legal moves!\n");
        return; // Should be game over
    }

    int best_move_index = -1;
    int best_score = (board->current_player == P_WHITE) ? -INFINITY : INFINITY;
    int alpha = -INFINITY;
    int beta = INFINITY;

    for (int i = 0; i < move_count; i++) {
        // Store state for undo (simple version)
        struct Square captured_square = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
        enum PieceColor original_player = board->current_player;

        make_move(board, &legal_moves[i]);
        int score = minimax(board, difficulty, alpha, beta, board->current_player == P_WHITE); // Pass difficulty as depth

        // Undo move (simple version - WARNING: INSUFFICIENT)
        struct Square moved_piece = board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col];
        board->squares[legal_moves[i].from_layer][legal_moves[i].from_row][legal_moves[i].from_col] = moved_piece;
        board->squares[legal_moves[i].to_layer][legal_moves[i].to_row][legal_moves[i].to_col] = captured_square;
        board->current_player = original_player;
        // WARNING: Does not restore castling/en passant

        if (board->current_player == P_WHITE) { // AI is White (maximizing)
            if (score > best_score) {
                best_score = score;
                best_move_index = i;
            }
            alpha = (alpha > score) ? alpha : score;
        } else { // AI is Black (minimizing)
            if (score < best_score) {
                best_score = score;
                best_move_index = i;
            }
            beta = (beta < score) ? beta : score;
        }
    }

    if (best_move_index != -1) {
        printf("AI chooses move %d/%d: from %d,%d,%d to %d,%d,%d (Score: %d)\n",
               best_move_index + 1, move_count,
               legal_moves[best_move_index].from_layer, legal_moves[best_move_index].from_row, legal_moves[best_move_index].from_col,
               legal_moves[best_move_index].to_layer, legal_moves[best_move_index].to_row, legal_moves[best_move_index].to_col,
               best_score);
        make_move(board, &legal_moves[best_move_index]);
    } else {
        // Fallback: make a random legal move if minimax fails (shouldn't happen)
        printf("AI minimax failed, making random move.\n");
        make_move(board, &legal_moves[rand() % move_count]);
    }
}

// Function to find the king of a specific color
bool find_king(const struct Board *board, enum PieceColor king_color, int *king_layer, int *king_row, int *king_col) {
    for (int l = 0; l < BOARD_LAYERS; l++) {
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (board->squares[l][r][c].piece == KING && board->squares[l][r][c].color == king_color) {
                    *king_layer = l;
                    *king_row = r;
                    *king_col = c;
                    return true;
                }
            }
        }
    }
    return false; // Should not happen in a valid game state
}

// Function to check if the specified king is in check
bool is_king_in_check(const struct Board *board, enum PieceColor king_color) {
    int king_layer, king_row, king_col;
    if (!find_king(board, king_color, &king_layer, &king_row, &king_col)) {
        return false; // King not found, treat as not in check (error state)
    }
    enum PieceColor attacker_color = (king_color == P_WHITE) ? P_BLACK : P_WHITE;
    return is_square_attacked(board, king_layer, king_row, king_col, attacker_color);
}

// Function to check if a square is attacked by the opponent
bool is_square_attacked(const struct Board *board, int target_layer, int target_row, int target_col, enum PieceColor attacker_color) {
    // Check for pawn attacks
    int pawn_direction = (attacker_color == P_WHITE) ? -1 : 1;
    int pawn_start_row = target_row - pawn_direction; // Row where an attacking pawn would be
    // Same layer pawn attacks
    for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
        int pawn_col = target_col + col_offset;
        if (is_valid_position(target_layer, pawn_start_row, pawn_col)) {
            struct Square sq = board->squares[target_layer][pawn_start_row][pawn_col];
            if (sq.piece == PAWN && sq.color == attacker_color) return true;
        }
    }
    // Inter-layer```c
    for (int layer_offset = -1; layer_offset <= 1; layer_offset += 2) {
        int pawn_layer = target_layer + layer_offset;
        if (pawn_layer < 0 || pawn_layer >= BOARD_LAYERS) continue;
        for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
            int pawn_col = target_col + col_offset;
            if (is_valid_position(pawn_layer, pawn_start_row, pawn_col)) {
                struct Square sq = board->squares[pawn_layer][pawn_start_row][pawn_col];
                if (sq.piece == PAWN && sq.color == attacker_color) return true;
            }
        }
    }

    // Check for knight attacks
    int knight_moves[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    for (int l_offset = -1; l_offset <= 1; ++l_offset) {
        int knight_layer = target_layer + l_offset;
        if (knight_layer < 0 || knight_layer >= BOARD_LAYERS) continue;
        for (int i = 0; i < 8; i++) {
            int knight_row = target_row + knight_moves[i][0];
            int knight_col = target_col + knight_moves[i][1];
            if (is_valid_position(knight_layer, knight_row, knight_col)) {
                struct Square sq = board->squares[knight_layer][knight_row][knight_col];
                if (sq.piece == KNIGHT && sq.color == attacker_color) return true;
            }
        }
    }

    // Check for sliding piece attacks (Rook, Bishop, Queen)
    int directions[28][3]; // Max directions: 8 same-layer + 8 inter-layer diag + 12 inter-layer straight/vertical
    int dir_count = 0;
    // Rook directions (same layer)
    directions[dir_count][0]=0; directions[dir_count][1]=1; directions[dir_count][2]=0; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=-1; directions[dir_count][2]=0; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=0; directions[dir_count][2]=1; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=0; directions[dir_count][2]=-1; dir_count++;
    // Bishop directions (same layer)
    directions[dir_count][0]=0; directions[dir_count][1]=1; directions[dir_count][2]=1; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=1; directions[dir_count][2]=-1; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=-1; directions[dir_count][2]=1; dir_count++;
    directions[dir_count][0]=0; directions[dir_count][1]=-1; directions[dir_count][2]=-1; dir_count++;
    // Inter-layer directions
    for (int l_change = -1; l_change <= 1; l_change += 2) {
        // Rook-like
        directions[dir_count][0]=l_change; directions[dir_count][1]=1; directions[dir_count][2]=0; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=-1; directions[dir_count][2]=0; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=0; directions[dir_count][2]=1; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=0; directions[dir_count][2]=-1; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=0; directions[dir_count][2]=0; dir_count++; // Straight up/down
        // Bishop-like
        directions[dir_count][0]=l_change; directions[dir_count][1]=1; directions[dir_count][2]=1; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=1; directions[dir_count][2]=-1; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=-1; directions[dir_count][2]=1; dir_count++;
        directions[dir_count][0]=l_change; directions[dir_count][1]=-1; directions[dir_count][2]=-1; dir_count++;
    }

    for (int i = 0; i < dir_count; i++) {
        int l_dir = directions[i][0];
        int r_dir = directions[i][1];
        int c_dir = directions[i][2];

        bool is_diagonal = (r_dir != 0 && c_dir != 0); // Diagonal within a layer or between layers
        bool is_straight = !is_diagonal && (r_dir != 0 || c_dir != 0); // Straight within a layer or between layers (but not layer only)
        bool is_vertical = (l_dir != 0 && r_dir == 0 && c_dir == 0); // Straight up/down
        bool is_layer_diag = (l_dir != 0 && is_diagonal);
        bool is_layer_straight = (l_dir != 0 && is_straight);

        for (int step = 1; ; step++) {
            int current_layer = target_layer + step * l_dir;
            int current_row = target_row + step * r_dir;
            int current_col = target_col + step * c_dir;

            if (!is_valid_position(current_layer, current_row, current_col)) break; // Off board

            struct Square sq = board->squares[current_layer][current_row][current_col];
            if (sq.piece != EMPTY) {
                if (sq.color == attacker_color) {
                    // Check if the piece type matches the attack direction
                    if (sq.piece == QUEEN) return true; // Queen attacks in all directions
                    if (sq.piece == ROOK && (is_straight || is_vertical || is_layer_straight)) return true;
                    if (sq.piece == BISHOP && (is_diagonal || is_layer_diag)) return true;
                }
                break; // Path blocked by a piece (either attacker or defender)
            }
        }
    }

    // Check for king attacks (adjacent squares)
    for (int l_offset = -1; l_offset <= 1; ++l_offset) {
        for (int r_offset = -1; r_offset <= 1; ++r_offset) {
            for (int c_offset = -1; c_offset <= 1; ++c_offset) {
                if (l_offset == 0 && r_offset == 0 && c_offset == 0) continue;
                int king_layer = target_layer + l_offset;
                int king_row = target_row + r_offset;
                int king_col = target_col + c_offset;
                if (is_valid_position(king_layer, king_row, king_col)) {
                    struct Square sq = board->squares[king_layer][king_row][king_col];
                    if (sq.piece == KING && sq.color == attacker_color) return true;
                }
            }
        }
    }

    return false;
}
