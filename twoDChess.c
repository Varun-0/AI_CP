#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include "include/raylib.h" // Add raylib header

#define BOARD_SIZE 8
#define MAX_MOVES 256
#define INFINITY 1000000
#define MATE_SCORE (INFINITY - 100) // Score for checkmate, slightly less than infinity

enum Piece {
    EMPTY,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

enum PlayerColor {
    PLAYER_WHITE,
    PLAYER_BLACK,
    NONE
};

enum GameState {
    MENU_DIFFICULTY,
    MENU_COLOR,
    PLAYING,
    PROMOTION,
    GAME_OVER
};

struct Square {
    enum Piece piece;
    enum PlayerColor color;
};

// Store state needed to undo a move
struct PreviousState {
    enum Piece captured_piece;
    enum PlayerColor captured_color; // Need color for en passant undo
    bool white_castle_kingside;
    bool white_castle_queenside;
    bool black_castle_kingside;
    bool black_castle_queenside;
    int en_passant_row;
    int en_passant_col;
    int halfmove_clock;
    // fullmove_number is handled separately in make/undo
};

struct Move {
    int from_row;
    int from_col;
    int to_row;
    int to_col;
    enum Piece promotion;
    int score; // Used by AI
    // Remove is_capture, deduce from previous_state.captured_piece
    struct PreviousState previous_state; // Store state before the move
};

struct Board {
    struct Square squares[BOARD_SIZE][BOARD_SIZE];
    enum PlayerColor current_player;
    bool white_castle_kingside;
    bool white_castle_queenside;
    bool black_castle_kingside;
    bool black_castle_queenside;
    int en_passant_row;
    int en_passant_col;
    int halfmove_clock;
    int fullmove_number;
    // TODO: Add history for threefold repetition
};

// --- Global Variables ---
Texture2D pieceTextures[2][7]; // [Color: PLAYER_WHITE=0, PLAYER_BLACK=1][PieceType: EMPTY=0, PAWN=1..KING=6]
enum GameState currentGameState = MENU_DIFFICULTY;
int selectedDifficulty = 2; // Default Medium
bool playerIsWhite = true; // Default White
struct Move pendingPromotionMove; // To store move details during promotion selection

// --- Piece-Square Tables (White's perspective, mirrored for Black) ---
// Values are somewhat arbitrary, based on common chess engine principles.
// Higher values = better squares.

// Mirrored lookup: black_pst[row][col] == white_pst[7-row][col]

const int pawn_pst[BOARD_SIZE][BOARD_SIZE] = {
    {0,  0,  0,  0,  0,  0,  0,  0},
    {50, 50, 50, 50, 50, 50, 50, 50}, // Strong push potential
    {10, 10, 20, 30, 30, 20, 10, 10},
    { 5,  5, 10, 25, 25, 10,  5,  5},
    { 0,  0,  0, 20, 20,  0,  0,  0},
    { 5, -5,-10,  0,  0,-10, -5,  5},
    { 5, 10, 10,-20,-20, 10, 10,  5},
    { 0,  0,  0,  0,  0,  0,  0,  0} // Promotion handled by move generation
};

const int knight_pst[BOARD_SIZE][BOARD_SIZE] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};

const int bishop_pst[BOARD_SIZE][BOARD_SIZE] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};

const int rook_pst[BOARD_SIZE][BOARD_SIZE] = {
    { 0,  0,  0,  0,  0,  0,  0,  0},
    { 5, 10, 10, 10, 10, 10, 10,  5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    {-5,  0,  0,  0,  0,  0,  0, -5},
    { 0,  0,  0,  5,  5,  0,  0,  0} // Better on open files/7th rank
};

// Queen PST often combines Rook and Bishop ideas
const int queen_pst[BOARD_SIZE][BOARD_SIZE] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};

// King PST changes significantly between opening/midgame and endgame.
// This is a simplified midgame version.
const int king_pst[BOARD_SIZE][BOARD_SIZE] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    { 20, 20,  0,  0,  0,  0, 20, 20}, // Prefer castled positions
    { 20, 30, 10,  0,  0, 10, 30, 20}
};

// --- Function Prototypes ---
void display_piece_legend();
void init_board(struct Board *board);
void print_board(const struct Board *board);
bool is_valid_position(int row, int col);
// Renamed:
int generate_pseudo_legal_moves(const struct Board *board, struct Move moves[]);
// New:
int generate_legal_moves(struct Board *board, struct Move moves[]);
bool is_square_attacked(const struct Board *board, int row, int col, enum PlayerColor attacker_color);
bool find_king(const struct Board *board, enum PlayerColor king_color, int *king_row, int *king_col);
bool is_king_in_check(const struct Board *board, enum PlayerColor king_color);
// Updated:
void make_move(struct Board *board, const struct Move *move);
void undo_move(struct Board *board, const struct Move *move);
int evaluate_board(const struct Board *board); // Keep prototype
// Updated:
bool is_game_over(struct Board *board, char *result_message, int buffer_size);
int minimax(struct Board *board, int depth, int alpha, int beta, bool maximizing_player);
void ai_make_move(struct Board *board, int difficulty);
bool LoadPieceTextures();
void UnloadPieceTextures();
// --- UI Drawing Functions ---
void DrawDifficultyMenu(int screenWidth, int screenHeight) {
    ClearBackground(DARKGRAY);
    DrawText("Select Difficulty", screenWidth / 2 - MeasureText("Select Difficulty", 40) / 2, screenHeight / 4, 40, RAYWHITE);

    Rectangle easyButton = { screenWidth / 2 - 100, screenHeight / 2 - 30, 200, 50 };
    Rectangle mediumButton = { screenWidth / 2 - 100, screenHeight / 2 + 30, 200, 50 };
    Rectangle hardButton = { screenWidth / 2 - 100, screenHeight / 2 + 90, 200, 50 };

    DrawRectangleRec(easyButton, LIGHTGRAY);
    DrawRectangleRec(mediumButton, LIGHTGRAY);
    DrawRectangleRec(hardButton, LIGHTGRAY);

    DrawText("Easy", easyButton.x + easyButton.width / 2 - MeasureText("Easy", 20) / 2, easyButton.y + 15, 20, BLACK);
    DrawText("Medium", mediumButton.x + mediumButton.width / 2 - MeasureText("Medium", 20) / 2, mediumButton.y + 15, 20, BLACK);
    DrawText("Hard", hardButton.x + hardButton.width / 2 - MeasureText("Hard", 20) / 2, hardButton.y + 15, 20, BLACK);
}

void DrawColorMenu(int screenWidth, int screenHeight) {
    ClearBackground(DARKGRAY);
    DrawText("Choose Your Color", screenWidth / 2 - MeasureText("Choose Your Color", 40) / 2, screenHeight / 4, 40, RAYWHITE);

    Rectangle whiteButton = { screenWidth / 2 - 100, screenHeight / 2 - 30, 200, 50 };
    Rectangle blackButton = { screenWidth / 2 - 100, screenHeight / 2 + 30, 200, 50 };

    DrawRectangleRec(whiteButton, LIGHTGRAY);
    DrawRectangleRec(blackButton, LIGHTGRAY);

    DrawText("White (First)", whiteButton.x + whiteButton.width / 2 - MeasureText("White (First)", 20) / 2, whiteButton.y + 15, 20, BLACK);
    DrawText("Black (Second)", blackButton.x + blackButton.width / 2 - MeasureText("Black (Second)", 20) / 2, blackButton.y + 15, 20, BLACK);
}

void DrawPromotionMenu(int screenWidth, int screenHeight, enum PlayerColor playerColor) {
    // Draw semi-transparent overlay
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
    DrawText("Promote Pawn To:", screenWidth / 2 - MeasureText("Promote Pawn To:", 30) / 2, screenHeight / 3, 30, RAYWHITE);

    int squareSize = screenHeight / BOARD_SIZE;
    int startX = screenWidth / 2 - (2 * squareSize);
    int startY = screenHeight / 2 - squareSize / 2;

    enum Piece options[] = {QUEEN, ROOK, BISHOP, KNIGHT};
    for (int i = 0; i < 4; i++) {
        Rectangle buttonRect = { (float)startX + i * squareSize, (float)startY, (float)squareSize, (float)squareSize };
        DrawRectangleRec(buttonRect, LIGHTGRAY);

        Texture2D texture = pieceTextures[playerColor][options[i]];
        if (texture.id > 0) {
            Rectangle sourceRect = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
            DrawTexturePro(texture, sourceRect, buttonRect, (Vector2){0,0}, 0.0f, WHITE);
        }
    }
}

// Placeholder drawing functions
// Update DrawBoard to highlight the selected square
void DrawBoard(int screenWidth, int screenHeight, int selected_row, int selected_col) {
    int squareSize = screenHeight / BOARD_SIZE; // Assuming square board fitting height
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            Color squareColor = ((row + col) % 2 == 0) ? RAYWHITE : LIGHTGRAY;
            DrawRectangle(col * squareSize, row * squareSize, squareSize, squareSize, squareColor);

            // Highlight selected square
            if (row == selected_row && col == selected_col) {
                DrawRectangleLines(col * squareSize, row * squareSize, squareSize, squareSize, YELLOW);
            }
        }
    }
    // Draw board border
    DrawRectangleLines(0, 0, squareSize * BOARD_SIZE, squareSize * BOARD_SIZE, DARKGRAY);
}

// Modify DrawPieces to use individual textures
void DrawPieces(const struct Board *board, int screenWidth, int screenHeight) {
    int squareSize = screenHeight / BOARD_SIZE;
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            enum Piece piece = board->squares[row][col].piece;
            enum PlayerColor color = board->squares[row][col].color;

            if (piece != EMPTY) {
                Texture2D texture = pieceTextures[color][piece];
                if (texture.id > 0) { // Check if texture is valid
                    Rectangle sourceRect = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
                    Rectangle destRect = { (float)col * squareSize, (float)row * squareSize, (float)squareSize, (float)squareSize };
                    DrawTexturePro(texture, sourceRect, destRect, (Vector2){ 0, 0 }, 0.0f, WHITE);
                } else {
                    // Optional: Draw a placeholder if texture failed to load
                    DrawText("?", col * squareSize + squareSize / 3, row * squareSize + squareSize / 4, squareSize / 2, RED);
                }
            }
        }
    }
}

void display_piece_legend() {
    printf("\nPiece Legend:\n");
    printf("P/p - Pawn (White/Black)\n");
    printf("N/n - Knight (White/Black)\n");
    printf("B/b - Bishop (White/Black)\n");
    printf("R/r - Rook (White/Black)\n");
    printf("Q/q - Queen (White/Black)\n");
    printf("K/k - King (White/Black)\n");
    printf(". - Empty square\n\n");
}

void init_board(struct Board *board) {
    // Set up pawns
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[1][col].piece = PAWN;
        board->squares[1][col].color = PLAYER_BLACK;
        board->squares[6][col].piece = PAWN;
        board->squares[6][col].color = PLAYER_WHITE;
    }

    // Set up other pieces
    enum Piece back_row[BOARD_SIZE] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[0][col].piece = back_row[col];
        board->squares[0][col].color = PLAYER_BLACK;
        board->squares[7][col].piece = back_row[col];
        board->squares[7][col].color = PLAYER_WHITE;
    }

    // Empty squares
    for (int row = 2; row < 6; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            board->squares[row][col].piece = EMPTY;
            board->squares[row][col].color = NONE;
        }
    }

    // Game state
    board->current_player = PLAYER_WHITE;
    board->white_castle_kingside = true;
    board->white_castle_queenside = true;
    board->black_castle_kingside = true;
    board->black_castle_queenside = true;
    board->en_passant_row = -1;
    board->en_passant_col = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
}

void print_board(const struct Board *board) {
    printf("\n  a b c d e f g h\n");
    for (int row = 0; row < BOARD_SIZE; row++) {
        printf("%d ", 8 - row);
        for (int col = 0; col < BOARD_SIZE; col++) {
            char piece_char = ' ';
            switch (board->squares[row][col].piece) {
                case PAWN:   piece_char = 'P'; break;
                case KNIGHT: piece_char = 'N'; break;
                case BISHOP: piece_char = 'B'; break;
                case ROOK:   piece_char = 'R'; break;
                case QUEEN:  piece_char = 'Q'; break;
                case KING:   piece_char = 'K'; break;
                case EMPTY:  piece_char = '.'; break;
            }
            
            if (board->squares[row][col].color == PLAYER_BLACK) {
                piece_char = tolower(piece_char);
            }
            printf("%c ", piece_char);
        }
        printf("%d\n", 8 - row);
    }
    printf("  a b c d e f g h\n");
    printf("%s to move\n", board->current_player == PLAYER_WHITE ? "White" : "Black");
}

bool is_valid_position(int row, int col) {
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE; // Adjusted for PLAYER_WHITE and PLAYER_BLACK
}

// --- Move Generation ---

void generate_pawn_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    int direction = (board->squares[row][col].color == PLAYER_WHITE) ? -1 : 1;
    int start_row = (board->squares[row][col].color == PLAYER_WHITE) ? 6 : 1;
    enum PlayerColor current_color = board->squares[row][col].color;
    enum PlayerColor opponent_color = (current_color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    int promotion_rank = (current_color == PLAYER_WHITE) ? 0 : 7;

    // 1. Forward move
    int next_row = row + direction;
    if (is_valid_position(next_row, col) && board->squares[next_row][col].piece == EMPTY) {
        if (next_row == promotion_rank) {
            // Add promotion moves
            enum Piece promo_pieces[] = {QUEEN, ROOK, BISHOP, KNIGHT};
            for (int i = 0; i < 4; i++) {
                moves[*move_count].from_row = row; moves[*move_count].from_col = col;
                moves[*move_count].to_row = next_row; moves[*move_count].to_col = col;
                moves[*move_count].promotion = promo_pieces[i];
                moves[*move_count].previous_state.captured_piece = EMPTY;
                moves[*move_count].previous_state.captured_color = NONE;
                (*move_count)++;
            }
        } else {
            // Standard forward move
            moves[*move_count].from_row = row; moves[*move_count].from_col = col;
            moves[*move_count].to_row = next_row; moves[*move_count].to_col = col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].previous_state.captured_piece = EMPTY;
            moves[*move_count].previous_state.captured_color = NONE;
            (*move_count)++;
        }

        // 2. Double forward move (only if single move was possible)
        if (row == start_row && is_valid_position(row + 2 * direction, col) && board->squares[row + 2 * direction][col].piece == EMPTY) {
            moves[*move_count].from_row = row; moves[*move_count].from_col = col;
            moves[*move_count].to_row = row + 2 * direction; moves[*move_count].to_col = col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].previous_state.captured_piece = EMPTY;
            moves[*move_count].previous_state.captured_color = NONE;
            (*move_count)++;
        }
    }

    // 3. Captures (including en passant)
    for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
        int capture_col = col + col_offset;
        if (is_valid_position(next_row, capture_col)) {
            // Standard capture
            if (board->squares[next_row][capture_col].piece != EMPTY && board->squares[next_row][capture_col].color == opponent_color) {
                 if (next_row == promotion_rank) {
                    enum Piece promo_pieces[] = {QUEEN, ROOK, BISHOP, KNIGHT};
                    for (int i = 0; i < 4; i++) {
                        moves[*move_count].from_row = row; moves[*move_count].from_col = col;
                        moves[*move_count].to_row = next_row; moves[*move_count].to_col = capture_col;
                        moves[*move_count].promotion = promo_pieces[i];
                        moves[*move_count].previous_state.captured_piece = board->squares[next_row][capture_col].piece;
                        moves[*move_count].previous_state.captured_color = board->squares[next_row][capture_col].color;
                        (*move_count)++;
                    }
                 } else {
                    moves[*move_count].from_row = row; moves[*move_count].from_col = col;
                    moves[*move_count].to_row = next_row; moves[*move_count].to_col = capture_col;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].previous_state.captured_piece = board->squares[next_row][capture_col].piece;
                    moves[*move_count].previous_state.captured_color = board->squares[next_row][capture_col].color;
                    (*move_count)++;
                 }
            }
            // En passant capture
            else if (next_row == board->en_passant_row && capture_col == board->en_passant_col &&
                     board->squares[next_row][capture_col].piece == EMPTY) { // Target square must be empty
                moves[*move_count].from_row = row; moves[*move_count].from_col = col;
                moves[*move_count].to_row = next_row; moves[*move_count].to_col = capture_col;
                moves[*move_count].promotion = EMPTY;
                // Indicate EP capture by setting captured piece to PAWN and color to opponent
                moves[*move_count].previous_state.captured_piece = PAWN;
                moves[*move_count].previous_state.captured_color = opponent_color;
                (*move_count)++;
            }
        }
    }
}

void generate_knight_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    int knight_moves[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    enum PlayerColor current_color = board->squares[row][col].color;

    for (int i = 0; i < 8; i++) {
        int new_row = row + knight_moves[i][0];
        int new_col = col + knight_moves[i][1];

        if (is_valid_position(new_row, new_col)) {
             if (board->squares[new_row][new_col].color != current_color) { // Empty or opponent
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_row = new_row;
                moves[*move_count].to_col = new_col;
                moves[*move_count].promotion = EMPTY;
                // Store captured piece info
                moves[*move_count].previous_state.captured_piece = board->squares[new_row][new_col].piece;
                moves[*move_count].previous_state.captured_color = board->squares[new_row][new_col].color;
                (*move_count)++;
             }
        }
    }
}

void generate_directional_moves(const struct Board *board, int row, int col,
                               int row_dir, int col_dir, struct Move moves[], int *move_count) {
    int new_row = row + row_dir;
    int new_col = col + col_dir;
    enum PlayerColor current_color = board->squares[row][col].color;

    while (is_valid_position(new_row, new_col)) {
        if (board->squares[new_row][new_col].piece == EMPTY) {
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_row = new_row;
            moves[*move_count].to_col = new_col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].previous_state.captured_piece = EMPTY;
            moves[*move_count].previous_state.captured_color = NONE;
            (*move_count)++;
        } else { // Hit a piece
            if (board->squares[new_row][new_col].color != current_color) { // Capture opponent
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_row = new_row;
                moves[*move_count].to_col = new_col;
                moves[*move_count].promotion = EMPTY;
                moves[*move_count].previous_state.captured_piece = board->squares[new_row][new_col].piece;
                moves[*move_count].previous_state.captured_color = board->squares[new_row][new_col].color;
                (*move_count)++;
            }
            break; // Stop after hitting any piece
        }
        new_row += row_dir;
        new_col += col_dir;
    }
}

void generate_bishop_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, row, col, directions[i][0], directions[i][1], moves, move_count);
    }
}

void generate_rook_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, row, col, directions[i][0], directions[i][1], moves, move_count);
    }
}

void generate_queen_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    generate_bishop_moves(board, row, col, moves, move_count);
    generate_rook_moves(board, row, col, moves, move_count);
}

void generate_king_moves(const struct Board *board, int row, int col, struct Move moves[], int *move_count) {
    enum PlayerColor current_color = board->squares[row][col].color;
    enum PlayerColor opponent_color = (current_color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Standard King Moves (add previous_state capture info like in knight moves)
    for (int row_dir = -1; row_dir <= 1; row_dir++) {
        for (int col_dir = -1; col_dir <= 1; col_dir++) {
            if (row_dir == 0 && col_dir == 0) continue; // Skip the case where the king doesn't move
            int new_row = row + row_dir;
            int new_col = col + col_dir;

            if (is_valid_position(new_row, new_col)) {
                 if (board->squares[new_row][new_col].color != current_color) { // Empty or opponent
                    moves[*move_count].from_row = row;
                    moves[*move_count].from_col = col;
                    moves[*move_count].to_row = new_row;
                    moves[*move_count].to_col = new_col;
                    moves[*move_count].promotion = EMPTY;
                    // Store captured piece info
                    moves[*move_count].previous_state.captured_piece = board->squares[new_row][new_col].piece;
                    moves[*move_count].previous_state.captured_color = board->squares[new_row][new_col].color;
                    (*move_count)++;
                 }
            }
        }
    }
}

// Remove generate_moves_for_piece and generate_all_moves
// Replace with generate_pseudo_legal_moves

// Generates pseudo-legal moves (doesn't check for leaving king in check)
int generate_pseudo_legal_moves(const struct Board *board, struct Move moves[]) {
    int move_count = 0;
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board->squares[row][col].piece != EMPTY &&
                board->squares[row][col].color == board->current_player) {
                 switch (board->squares[row][col].piece) {
                    case PAWN:   generate_pawn_moves(board, row, col, moves, &move_count);   break;
                    case KNIGHT: generate_knight_moves(board, row, col, moves, &move_count); break;
                    case BISHOP: generate_bishop_moves(board, row, col, moves, &move_count); break;
                    case ROOK:   generate_rook_moves(board, row, col, moves, &move_count);   break;
                    case QUEEN:  generate_queen_moves(board, row, col, moves, &move_count);  break;
                    case KING:   generate_king_moves(board, row, col, moves, &move_count);   break;
                    case EMPTY: break;
                }
            }
        }
    }
    return move_count;
}

// --- Make / Undo Move (Updated) ---

void make_move(struct Board *board, const struct Move *move) {
    // --- Store current state for undo ---
    // Cast to non-const to modify the struct within the const Move pointer
    struct PreviousState *prevState = (struct PreviousState *) &move->previous_state;
    prevState->white_castle_kingside = board->white_castle_kingside;
    prevState->white_castle_queenside = board->white_castle_queenside;
    prevState->black_castle_kingside = board->black_castle_kingside;
    prevState->black_castle_queenside = board->black_castle_queenside;
    prevState->en_passant_row = board->en_passant_row;
    prevState->en_passant_col = board->en_passant_col;
    prevState->halfmove_clock = board->halfmove_clock;
    // captured_piece/color are already set during move generation

    enum Piece moving_piece = board->squares[move->from_row][move->from_col].piece;
    enum PlayerColor moving_color = board->squares[move->from_row][move->from_col].color;
    bool is_pawn_move = moving_piece == PAWN;
    bool is_capture = prevState->captured_piece != EMPTY; // Use stored info

    // --- Update Board State ---

    // Handle Castling Rook Move
    bool castling_move = false;
    if (moving_piece == KING) {
        int col_diff = move->to_col - move->from_col;
        if (abs(col_diff) == 2) {
            castling_move = true;
            int rook_from_col = (col_diff > 0) ? 7 : 0;
            int rook_to_col = (col_diff > 0) ? 5 : 3;
            int rook_row = move->from_row; // Same row as king

            board->squares[rook_row][rook_to_col] = board->squares[rook_row][rook_from_col]; // Move rook
            board->squares[rook_row][rook_from_col].piece = EMPTY; // Empty original rook square
            board->squares[rook_row][rook_from_col].color = NONE;
        }
        // Update castling rights whenever king moves
        if (moving_color == PLAYER_WHITE) {
            board->white_castle_kingside = false;
            board->white_castle_queenside = false;
        } else {
            board->black_castle_kingside = false;
            board->black_castle_queenside = false;
        }
    }

    // Handle En Passant Capture (Remove the captured pawn)
    // Check if it's a pawn move, diagonal, to the EP square, and the generator marked captured_piece as PAWN
    bool en_passant_capture = is_pawn_move &&
                              move->to_row == prevState->en_passant_row && // Use stored EP square
                              move->to_col == prevState->en_passant_col &&
                              prevState->captured_piece == PAWN && // Generator signals EP this way
                              board->squares[move->to_row][move->to_col].piece == EMPTY; // Target square is empty

    if (en_passant_capture) {
        int captured_pawn_row = move->from_row; // Pawn captured is on the same row as the moving pawn started
        int captured_pawn_col = move->to_col;   // Pawn captured is in the destination column
        board->squares[captured_pawn_row][captured_pawn_col].piece = EMPTY;
        board->squares[captured_pawn_row][captured_pawn_col].color = NONE;
        // is_capture is already true because prevState->captured_piece was PAWN
    }

    // Move the piece
    if (move->promotion != EMPTY) {
        board->squares[move->to_row][move->to_col].piece = move->promotion;
    } else {
        board->squares[move->to_row][move->to_col].piece = moving_piece;
    }
    board->squares[move->to_row][move->to_col].color = moving_color;
    board->squares[move->from_row][move->from_col].piece = EMPTY;
    board->squares[move->from_row][move->from_col].color = NONE;

    // Update En Passant Target Square
    board->en_passant_row = -1; // Reset by default
    board->en_passant_col = -1;
    if (moving_piece == PAWN && abs(move->to_row - move->from_row) == 2) {
        board->en_passant_row = (move->from_row + move->to_row) / 2;
        board->en_passant_col = move->from_col;
    }

    // Update Castling Rights if Rook Moves or is Captured
    if (moving_piece == ROOK) {
        if (moving_color == PLAYER_WHITE) {
            if (move->from_row == 7 && move->from_col == 0) board->white_castle_queenside = false;
            else if (move->from_row == 7 && move->from_col == 7) board->white_castle_kingside = false;
        } else {
            if (move->from_row == 0 && move->from_col == 0) board->black_castle_queenside = false;
            else if (move->from_row == 0 && move->from_col == 7) board->black_castle_kingside = false;
        }
    }
    // If a rook is captured, update rights
    if (prevState->captured_piece == ROOK) {
         if (move->to_row == 7 && move->to_col == 0) board->white_castle_queenside = false;
         else if (move->to_row == 7 && move->to_col == 7) board->white_castle_kingside = false;
         else if (move->to_row == 0 && move->to_col == 0) board->black_castle_queenside = false;
         else if (move->to_row == 0 && move->to_col == 7) board->black_castle_kingside = false;
    }

    // Update Halfmove Clock (50-move rule)
    if (is_pawn_move || is_capture) {
        board->halfmove_clock = 0;
    } else {
        board->halfmove_clock++;
    }

    // Update Fullmove Number
    if (board->current_player == PLAYER_BLACK) {
        board->fullmove_number++;
    }

    // Switch Player
    board->current_player = (moving_color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Optional: Print capture message (removed from original make_move)
    // if (is_capture && !en_passant_capture) { // Don't print for EP capture here
    //     printf("Capture!\n");
    // }
}


void undo_move(struct Board *board, const struct Move *move) {
    enum PlayerColor previous_player = (board->current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    enum Piece moved_piece_type_on_board = board->squares[move->to_row][move->to_col].piece; // Piece currently on to_square
    enum Piece original_moved_piece = (move->promotion != EMPTY) ? PAWN : moved_piece_type_on_board;

    // --- Restore Board State from PreviousState ---
    board->white_castle_kingside = move->previous_state.white_castle_kingside;
    board->white_castle_queenside = move->previous_state.white_castle_queenside;
    board->black_castle_kingside = move->previous_state.black_castle_kingside;
    board->black_castle_queenside = move->previous_state.black_castle_queenside;
    board->en_passant_row = move->previous_state.en_passant_row;
    board->en_passant_col = move->previous_state.en_passant_col;
    board->halfmove_clock = move->previous_state.halfmove_clock;

    // --- Undo Piece Movement ---
    board->squares[move->from_row][move->from_col].piece = original_moved_piece;
    board->squares[move->from_row][move->from_col].color = previous_player;

    // Restore captured piece (if any)
    enum Piece captured_piece = move->previous_state.captured_piece;
    enum PlayerColor captured_color = move->previous_state.captured_color;

    // Check if it was an en passant capture
    bool en_passant_capture = (original_moved_piece == PAWN &&
                               move->to_row == board->en_passant_row && // Use restored EP row
                               move->to_col == board->en_passant_col &&
                               captured_piece == PAWN); // Check if make_move recorded it as EP capture

    if (en_passant_capture) {
        // Put captured pawn back in the correct en passant square
        int captured_pawn_row = move->from_row;
        int captured_pawn_col = move->to_col;
        board->squares[move->to_row][move->to_col].piece = EMPTY; // The landing square was empty
        board->squares[move->to_row][move->to_col].color = NONE;
        board->squares[captured_pawn_row][captured_pawn_col].piece = PAWN;
        board->squares[captured_pawn_row][captured_pawn_col].color = captured_color; // Use stored color
    } else {
        // Standard capture restore or empty square
        board->squares[move->to_row][move->to_col].piece = captured_piece; // Works even if EMPTY
        board->squares[move->to_row][move->to_col].color = captured_color; // Works even if NONE
    }

    // Undo Castling Rook Move
    if (original_moved_piece == KING) {
        int col_diff = move->to_col - move->from_col;
        if (abs(col_diff) == 2) {
            int rook_from_col = (col_diff > 0) ? 7 : 0;
            int rook_to_col = (col_diff > 0) ? 5 : 3;
            int rook_row = move->from_row;

            board->squares[rook_row][rook_from_col] = board->squares[rook_row][rook_to_col]; // Move rook back
            board->squares[rook_row][rook_to_col].piece = EMPTY; // Empty intermediate square
            board->squares[rook_row][rook_to_col].color = NONE;
        }
    }

    // Restore Fullmove Number
    if (board->current_player == PLAYER_WHITE) { // If current player is white, black just moved
        board->fullmove_number--;
    }

    // Restore Player
    board->current_player = previous_player;
}

// --- Evaluation (Updated with PSTs) ---
int evaluate_board(const struct Board *board) {
    int score = 0;
    int material_score = 0;
    int positional_score = 0;
    int piece_values[] = {0, 100, 320, 330, 500, 900, 20000}; // EMPTY, P, N, B, R, Q, K

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            enum Piece piece = board->squares[r][c].piece;
            if (piece != EMPTY) {
                int material_value = piece_values[piece];
                int positional_value = 0;

                // Get positional value from PST
                if (board->squares[r][c].color == PLAYER_WHITE) {
                    material_score += material_value;
                    switch (piece) {
                        case PAWN:   positional_value = pawn_pst[r][c]; break;
                        case KNIGHT: positional_value = knight_pst[r][c]; break;
                        case BISHOP: positional_value = bishop_pst[r][c]; break;
                        case ROOK:   positional_value = rook_pst[r][c]; break;
                        case QUEEN:  positional_value = queen_pst[r][c]; break;
                        case KING:   positional_value = king_pst[r][c]; break;
                        default: break;
                    }
                    positional_score += positional_value;
                } else { // PLAYER_BLACK
                    material_score -= material_value;
                    // Mirror the PST lookup for Black
                    switch (piece) {
                        case PAWN:   positional_value = pawn_pst[7-r][c]; break;
                        case KNIGHT: positional_value = knight_pst[7-r][c]; break;
                        case BISHOP: positional_value = bishop_pst[7-r][c]; break;
                        case ROOK:   positional_value = rook_pst[7-r][c]; break;
                        case QUEEN:  positional_value = queen_pst[7-r][c]; break;
                        case KING:   positional_value = king_pst[7-r][c]; break;
                        default: break;
                    }
                    positional_score -= positional_value; // Subtract Black's positional score (which is White's mirrored score)
                }
            }
        }
    }

    // Combine material and positional scores
    // Adjust weighting if desired (e.g., positional_score / 2)
    score = material_score + positional_score;

    // Return score from White's perspective ALWAYS for minimax consistency
    return score;
}

// --- Check, Checkmate, Stalemate (New/Updated) ---

bool find_king(const struct Board *board, enum PlayerColor king_color, int *king_row, int *king_col) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board->squares[r][c].piece == KING && board->squares[r][c].color == king_color) {
                *king_row = r;
                *king_col = c;
                return true;
            }
        }
    }
    *king_row = -1; // Indicate not found
    *king_col = -1;
    return false;
}

bool is_square_attacked(const struct Board *board, int row, int col, enum PlayerColor attacker_color) {
    // Check for pawn attacks
    int pawn_dir = (attacker_color == PLAYER_WHITE) ? 1 : -1; // Pawns attack diagonally forward from their perspective
    int attack_row = row + pawn_dir;
    if (is_valid_position(attack_row, col - 1) &&
        board->squares[attack_row][col - 1].piece == PAWN &&
        board->squares[attack_row][col - 1].color == attacker_color) return true;
    if (is_valid_position(attack_row, col + 1) &&
        board->squares[attack_row][col + 1].piece == PAWN &&
        board->squares[attack_row][col + 1].color == attacker_color) return true;


    // Check for knight attacks
    int knight_moves[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    for (int i = 0; i < 8; i++) {
        int nr = row + knight_moves[i][0];
        int nc = col + knight_moves[i][1];
        if (is_valid_position(nr, nc) &&
            board->squares[nr][nc].piece == KNIGHT &&
            board->squares[nr][nc].color == attacker_color) return true;
    }

    // Check for sliding pieces (Rook, Bishop, Queen)
    int directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (int i = 0; i < 8; i++) {
        int dr = directions[i][0];
        int dc = directions[i][1];
        for (int j = 1; ; j++) {
            int nr = row + j * dr;
            int nc = col + j * dc;
            if (!is_valid_position(nr, nc)) break;
            if (board->squares[nr][nc].piece != EMPTY) {
                if (board->squares[nr][nc].color == attacker_color) {
                    enum Piece p = board->squares[nr][nc].piece;
                    bool is_diagonal = (dr != 0 && dc != 0);
                    bool is_straight = (dr == 0 || dc == 0);
                    // Check if the piece type matches the direction
                    if (p == QUEEN || (is_diagonal && p == BISHOP) || (is_straight && p == ROOK)) {
                        return true;
                    }
                }
                break; // Path blocked (by attacker's own piece or defender's piece)
            }
        }
    }

    // Check for king attacks
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = row + dr;
            int nc = col + dc;
            if (is_valid_position(nr, nc) &&
                board->squares[nr][nc].piece == KING &&
                board->squares[nr][nc].color == attacker_color) return true;
        }
    }

    return false;
}

bool is_king_in_check(const struct Board *board, enum PlayerColor king_color) {
    int king_row, king_col;
    if (!find_king(board, king_color, &king_row, &king_col)) {
        // This should ideally not happen in a valid game state
        fprintf(stderr, "Error: King of color %d not found!\n", king_color);
        return false;
    }
    enum PlayerColor attacker_color = (king_color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    return is_square_attacked(board, king_row, king_col, attacker_color);
}

// Generates only fully legal moves (filters out moves leaving king in check)
int generate_legal_moves(struct Board *board, struct Move moves[]) {
    struct Move pseudo_legal_moves[MAX_MOVES];
    // Need a mutable board copy to simulate moves without affecting the original board passed to this function
    struct Board temp_board = *board;
    int pseudo_legal_count = generate_pseudo_legal_moves(&temp_board, pseudo_legal_moves); // Generate on temp board context
    int legal_move_count = 0;

    enum PlayerColor current_player = temp_board.current_player;

    for (int i = 0; i < pseudo_legal_count; i++) {
        // Simulate the move on the temporary board
        make_move(&temp_board, &pseudo_legal_moves[i]);

        // Check if the king of the player who just moved is now in check
        if (!is_king_in_check(&temp_board, current_player)) {
            // If not in check, the move is legal - copy it to the output array
            // Important: Copy the move *before* undoing, including the previous_state populated by the generator
            moves[legal_move_count++] = pseudo_legal_moves[i];
        }

        // Undo the move on the temporary board to restore its state for the next iteration
        undo_move(&temp_board, &pseudo_legal_moves[i]);
    }

    return legal_move_count;
}

// --- AI (Updated with Move Ordering) ---

// Helper function to assign a score to a move for ordering
int score_move(const struct Board *board, const struct Move *move) {
    int score = 0;
    int piece_values[] = {0, 100, 320, 330, 500, 900, 0}; // No value for king capture

    // 1. Promotions (Highest priority)
    if (move->promotion != EMPTY) {
        score += piece_values[move->promotion]; // Add value of promoted piece
        score += 10000; // Big bonus for promotion
        return score;
    }

    // 2. Captures (MVV-LVA: Most Valuable Victim - Least Valuable Attacker)
    enum Piece captured_piece = move->previous_state.captured_piece;
    if (captured_piece != EMPTY) {
        enum Piece moving_piece = board->squares[move->from_row][move->from_col].piece;
        // Approximate MVV-LVA: Value of captured piece - Value of attacking piece / 10
        // Dividing attacker value helps prioritize capturing with lower-value pieces
        score += piece_values[captured_piece] - (piece_values[moving_piece] / 10);
        score += 1000; // Bonus for any capture
    }

    // TODO: Add bonus for checks? (Requires checking if move results in check)

    // Quiet moves will have score 0 or close to it
    return score;
}

// Comparison function for qsort
int compare_moves(const void *a, const void *b) {
    const struct Move *moveA = (const struct Move *)a;
    const struct Move *moveB = (const struct Move *)b;
    // Sort in descending order of score
    return moveB->score - moveA->score;
}

bool is_game_over(struct Board *board, char *result_message, int buffer_size) {
    struct Move legal_moves[MAX_MOVES];
    // Generate legal moves for the current player
    int legal_move_count = generate_legal_moves(board, legal_moves);

    bool in_check = is_king_in_check(board, board->current_player);

    if (legal_move_count == 0) {
        if (in_check) {
            snprintf(result_message, buffer_size, "Checkmate! %s wins.", (board->current_player == PLAYER_WHITE) ? "Black" : "White");
            return true; // Checkmate
        } else {
            snprintf(result_message, buffer_size, "Stalemate! Draw.");
            return true; // Stalemate
        }
    }

    // Check 50-move rule
    if (board->halfmove_clock >= 100) { // 50 moves by each player = 100 half-moves
         snprintf(result_message, buffer_size, "Draw by 50-move rule.");
         return true;
    }

    // TODO: Check for threefold repetition (requires move history)
    // TODO: Check for insufficient material (more complex)

    // Basic check if kings are missing (shouldn't happen in normal play)
    int kr, kc;
    if (!find_king(board, PLAYER_WHITE, &kr, &kc) || !find_king(board, PLAYER_BLACK, &kr, &kc)) {
         snprintf(result_message, buffer_size, "Game Over! A king is missing.");
         return true;
    }

    snprintf(result_message, buffer_size, "Game ongoing.");
    return false;
}

int minimax(struct Board *board, int depth, int alpha, int beta, bool maximizing_player) {
    char msg[100];
    // Check game over state at the beginning of the evaluation
    if (is_game_over(board, msg, sizeof(msg))) {
        if (strstr(msg, "Checkmate")) {
            return (board->current_player == PLAYER_WHITE) ? (-MATE_SCORE - depth) : (MATE_SCORE + depth);
        } else { // Stalemate or other draw
            return 0;
        }
    }

    // If depth limit reached, return static evaluation
    if (depth == 0) {
        return evaluate_board(board);
    }

    struct Move moves[MAX_MOVES];
    int move_count = generate_legal_moves(board, moves); // Use legal moves

    // --- Move Ordering --- 
    // Score each move
    for (int i = 0; i < move_count; i++) {
        moves[i].score = score_move(board, &moves[i]);
    }
    // Sort moves based on score (descending)
    qsort(moves, move_count, sizeof(struct Move), compare_moves);
    // --- End Move Ordering ---

    // Node evaluation based on whose turn it is (from White's perspective)
    if (maximizing_player) { // White's turn (or AI is White)
        int max_eval = -INFINITY -1; // Use -INFINITY - 1 to handle potential -INFINITY scores
        for (int i = 0; i < move_count; i++) {
            make_move(board, &moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, false); // Next turn is minimizing (Black)
            undo_move(board, &moves[i]);
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > max_eval) ? alpha : max_eval; // Update alpha
            if (beta <= alpha) break; // Pruning
        }
        return max_eval;
    } else { // Black's turn (or AI is Black)
        int min_eval = INFINITY + 1; // Use INFINITY + 1 to handle potential INFINITY scores
        for (int i = 0; i < move_count; i++) {
            make_move(board, &moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, true); // Next turn is maximizing (White)
            undo_move(board, &moves[i]);
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < min_eval) ? beta : min_eval; // Update beta
            if (beta <= alpha) break; // Pruning
        }
        return min_eval;
    }
}

void ai_make_move(struct Board *board, int difficulty) {
    struct Move moves[MAX_MOVES];
    int move_count = generate_legal_moves(board, moves); // Use legal moves

    if (move_count == 0) return; // Game should already be over

    int best_move_index = 0;
    int best_eval = (board->current_player == PLAYER_WHITE) ? -INFINITY -1 : INFINITY + 1;

    int depth;
    switch (difficulty) {
        case 1: depth = 2; break;
        case 2: depth = 3; break;
        case 3: depth = 4; break;
        default: depth = 3; break;
    }

    enum PlayerColor ai_color = board->current_player;
    bool is_ai_white = (ai_color == PLAYER_WHITE);

    // --- Move Ordering for Root --- (Optional but good practice)
    // Score moves at the root as well to potentially break ties better
    for (int i = 0; i < move_count; i++) {
        moves[i].score = score_move(board, &moves[i]);
    }
    qsort(moves, move_count, sizeof(struct Move), compare_moves);
    // --- End Move Ordering for Root ---

    for (int i = 0; i < move_count; i++) {
        make_move(board, &moves[i]);
        int eval = minimax(board, depth - 1, -INFINITY -1 , INFINITY + 1, !is_ai_white);
        undo_move(board, &moves[i]);

        // Store the minimax eval, overwriting the move ordering score
        moves[i].score = eval; 

        if (is_ai_white) { // White AI wants to maximize the score
            if (eval > best_eval) {
                best_eval = eval;
                best_move_index = i;
            }
        } else { // Black AI wants to minimize the score (from White's perspective)
            if (eval < best_eval) {
                best_eval = eval;
                best_move_index = i;
            }
        }
        // Add randomness for moves with the same best_eval?
        // if (eval == best_eval && (rand() % 3 == 0)) { // Example: 1/3 chance to switch to equally good move
        //     best_move_index = i;
        // }
    }

    // Make the best move found
    make_move(board, &moves[best_move_index]);
    printf("AI (%s) moves from %c%d to %c%d (Eval: %d)\n",
           is_ai_white ? "White" : "Black",
           'a' + moves[best_move_index].from_col, 8 - moves[best_move_index].from_row,
           'a' + moves[best_move_index].to_col, 8 - moves[best_move_index].to_row, best_eval);
}

// --- Main Game Loop (Updated with Restart) ---

void play_game() {
    struct Board board;
    char gameOverMessage[100] = ""; // Buffer for game over reason

    // --- Raylib Window Setup ---
    const int screenWidth = 800;
    const int screenHeight = 640;
    InitWindow(screenWidth, screenHeight, "2D Chess - Raylib");
    SetTargetFPS(60);
    int squareSize = screenHeight / BOARD_SIZE;

    // --- Load Textures ---
    if (!LoadPieceTextures()) {
        printf("Failed to load piece textures. Exiting.\n");
        CloseWindow();
        return;
    }

    // --- Game State for Input ---
    int selected_row = -1;
    int selected_col = -1;
    currentGameState = MENU_DIFFICULTY; // Start at menu

    while (!WindowShouldClose()) {

        // --- Global Input Handling (Restart) ---
        if (IsKeyPressed(KEY_R)) {
            currentGameState = MENU_DIFFICULTY;
            selected_row = -1; // Reset selection
            selected_col = -1;
            // No need to init_board here, it happens after color selection
        }

        // --- Update based on State ---
        switch (currentGameState) {
            case MENU_DIFFICULTY:
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    Rectangle easyButton = { screenWidth / 2 - 100, screenHeight / 2 - 30, 200, 50 };
                    Rectangle mediumButton = { screenWidth / 2 - 100, screenHeight / 2 + 30, 200, 50 };
                    Rectangle hardButton = { screenWidth / 2 - 100, screenHeight / 2 + 90, 200, 50 };

                    if (CheckCollisionPointRec(mousePos, easyButton)) { selectedDifficulty = 1; currentGameState = MENU_COLOR; }
                    else if (CheckCollisionPointRec(mousePos, mediumButton)) { selectedDifficulty = 2; currentGameState = MENU_COLOR; }
                    else if (CheckCollisionPointRec(mousePos, hardButton)) { selectedDifficulty = 3; currentGameState = MENU_COLOR; }
                }
                break;

            case MENU_COLOR:
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    Rectangle whiteButton = { screenWidth / 2 - 100, screenHeight / 2 - 30, 200, 50 };
                    Rectangle blackButton = { screenWidth / 2 - 100, screenHeight / 2 + 30, 200, 50 };

                    if (CheckCollisionPointRec(mousePos, whiteButton)) {
                        playerIsWhite = true;
                        init_board(&board); // Initialize board after settings are chosen
                        currentGameState = PLAYING;
                    } else if (CheckCollisionPointRec(mousePos, blackButton)) {
                        playerIsWhite = false;
                        init_board(&board);
                        currentGameState = PLAYING;
                        // If player is black, AI (White) makes the first move immediately
                        if (!is_game_over(&board, gameOverMessage, sizeof(gameOverMessage))) {
                             ai_make_move(&board, selectedDifficulty);
                        } else {
                             currentGameState = GAME_OVER; // Should not happen on first move
                        }
                    }
                }
                break;

            case PLAYING:
                // Check game over at the start of the turn
                if (is_game_over(&board, gameOverMessage, sizeof(gameOverMessage))) {
                    currentGameState = GAME_OVER;
                    break;
                }

                bool isPlayerTurn = (board.current_player == PLAYER_WHITE && playerIsWhite) ||
                                    (board.current_player == PLAYER_BLACK && !playerIsWhite);

                if (isPlayerTurn) {
                    // Player's turn
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        Vector2 mousePos = GetMousePosition();
                        int clicked_col = (int)(mousePos.x / squareSize);
                        int clicked_row = (int)(mousePos.y / squareSize);

                        if (is_valid_position(clicked_row, clicked_col)) {
                            if (selected_row == -1) {
                                // Select piece only if it belongs to the current player
                                if (board.squares[clicked_row][clicked_col].piece != EMPTY &&
                                    board.squares[clicked_row][clicked_col].color == board.current_player) {
                                    selected_row = clicked_row;
                                    selected_col = clicked_col;
                                }
                            } else {
                                // Try to move selected piece
                                // Generate legal moves for the selected piece
                                struct Move legal_moves[MAX_MOVES];
                                int legal_move_count = generate_legal_moves(&board, legal_moves);
                                bool found_legal_move = false;
                                struct Move chosen_move;

                                for (int i = 0; i < legal_move_count; i++) {
                                    // Check if the clicked square matches the destination of any legal move
                                    // for the currently selected piece
                                    if (legal_moves[i].from_row == selected_row &&
                                        legal_moves[i].from_col == selected_col &&
                                        legal_moves[i].to_row == clicked_row &&
                                        legal_moves[i].to_col == clicked_col)
                                    {
                                        // Found a legal move matching the click
                                        if (legal_moves[i].promotion != EMPTY) {
                                            // If promotion is required, store base move and go to promotion state
                                            pendingPromotionMove = legal_moves[i]; // Store the move template
                                            pendingPromotionMove.promotion = EMPTY; // Clear promotion piece for now
                                            currentGameState = PROMOTION;
                                            found_legal_move = true; // Mark as found to prevent deselection
                                            break; // Exit loop
                                        } else {
                                            // Standard legal move
                                            chosen_move = legal_moves[i];
                                            found_legal_move = true;
                                            break; // Exit loop
                                        }
                                    }
                                }

                                if (currentGameState == PROMOTION) {
                                     // Do nothing here, wait for promotion state handler
                                     // Keep selection active
                                } else if (found_legal_move) {
                                    make_move(&board, &chosen_move);
                                    selected_row = -1; // Reset selection
                                    selected_col = -1;
                                    // Check if player's move ended the game
                                    if (is_game_over(&board, gameOverMessage, sizeof(gameOverMessage))) {
                                        currentGameState = GAME_OVER;
                                    }
                                } else {
                                    // Invalid move target or not a legal move for the selected piece
                                    // Check if clicking on another piece of the same color to change selection
                                    if (board.squares[clicked_row][clicked_col].piece != EMPTY &&
                                        board.squares[clicked_row][clicked_col].color == board.current_player) {
                                        selected_row = clicked_row;
                                        selected_col = clicked_col;
                                    } else {
                                        selected_row = -1; // Deselect if clicked empty/opponent/invalid target
                                        selected_col = -1;
                                    }
                                }
                            }
                        } else {
                            selected_row = -1; // Clicked outside board, deselect
                            selected_col = -1;
                        }
                    }
                } else {
                    // AI's turn (add a small delay maybe?)
                    // Wait(0.5f); // Requires including raylib `WaitTime`
                    ai_make_move(&board, selectedDifficulty);
                    // Check if AI move ended the game
                    if (is_game_over(&board, gameOverMessage, sizeof(gameOverMessage))) {
                         currentGameState = GAME_OVER;
                    }
                }
                break;

            case PROMOTION:
                 if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    int startX = screenWidth / 2 - (2 * squareSize);
                    int startY = screenHeight / 2 - squareSize / 2;
                    enum Piece options[] = {QUEEN, ROOK, BISHOP, KNIGHT};
                    enum Piece chosenPromotion = EMPTY;

                    for (int i = 0; i < 4; i++) {
                        Rectangle buttonRect = { (float)startX + i * squareSize, (float)startY, (float)squareSize, (float)squareSize };
                        if (CheckCollisionPointRec(mousePos, buttonRect)) {
                            chosenPromotion = options[i];
                            break;
                        }
                    }

                    if (chosenPromotion != EMPTY) {
                        // Find the specific legal move corresponding to this promotion choice
                        struct Move legal_moves[MAX_MOVES];
                        int legal_move_count = generate_legal_moves(&board, legal_moves);
                        bool found_promo_move = false;
                        struct Move final_move;

                        for(int i=0; i < legal_move_count; ++i) {
                            if (legal_moves[i].from_row == pendingPromotionMove.from_row &&
                                legal_moves[i].from_col == pendingPromotionMove.from_col &&
                                legal_moves[i].to_row == pendingPromotionMove.to_row &&
                                legal_moves[i].to_col == pendingPromotionMove.to_col &&
                                legal_moves[i].promotion == chosenPromotion) // Match the chosen piece
                            {
                                final_move = legal_moves[i];
                                found_promo_move = true;
                                break;
                            }
                        }

                        if (found_promo_move) {
                            make_move(&board, &final_move);
                            selected_row = -1; // Reset selection
                            selected_col = -1;
                            currentGameState = PLAYING; // Return to playing
                             // Check if promotion move ended the game
                            if (is_game_over(&board, gameOverMessage, sizeof(gameOverMessage))) {
                                currentGameState = GAME_OVER;
                            }
                        } else {
                             // This shouldn't happen if the base move was legal and required promotion
                             fprintf(stderr, "Error: Could not find legal move for selected promotion.\n");
                             currentGameState = PLAYING; // Go back, reset selection
                             selected_row = -1;
                             selected_col = -1;
                        }
                    }
                    // Optional: Clicking outside promotion box cancels selection?
                    // else { currentGameState = PLAYING; selected_row = -1; selected_col = -1; }
                }
                break;

            case GAME_OVER:
                // Wait for window close or 'R' key press (handled globally)
                break;
        }

        // --- Drawing based on State ---
        BeginDrawing();
        switch (currentGameState) {
             case MENU_DIFFICULTY: DrawDifficultyMenu(screenWidth, screenHeight); break;
             case MENU_COLOR: DrawColorMenu(screenWidth, screenHeight); break;

            case PLAYING:
            case PROMOTION:
                ClearBackground(DARKGRAY);
                DrawBoard(screenWidth, screenHeight, selected_row, selected_col);
                DrawPieces(&board, screenWidth, screenHeight);
                // Display Check status if in PLAYING state
                if (currentGameState == PLAYING && is_king_in_check(&board, board.current_player)) {
                     DrawText("Check!", screenHeight + 10, 40, 20, RED);
                }
                DrawText(TextFormat("%s to move", board.current_player == PLAYER_WHITE ? "White" : "Black"),
                         screenHeight + 10, 10, 20, RAYWHITE);
                // Draw promotion menu overlay if needed
                if (currentGameState == PROMOTION) {
                    enum PlayerColor promotingPlayer = (board.current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE; // Player who just moved
                    DrawPromotionMenu(screenWidth, screenHeight, promotingPlayer);
                }
                // Add Restart hint
                DrawText("Press R to Restart", screenHeight + 10, screenHeight - 30, 20, LIGHTGRAY);
                break;
            case GAME_OVER:
                 ClearBackground(DARKGRAY);
                 DrawBoard(screenWidth, screenHeight, -1, -1); // No selection
                 DrawPieces(&board, screenWidth, screenHeight);
                 // Display the specific game over message centered
                 float textWidth = MeasureText(gameOverMessage, 30);
                 DrawText(gameOverMessage, (screenWidth - textWidth) / 2, screenHeight / 2 - 15, 30, RED);
                 // Add Restart hint
                 DrawText("Press R to Restart", (screenWidth - MeasureText("Press R to Restart", 20)) / 2, screenHeight / 2 + 30, 20, LIGHTGRAY);
                 break;
        }
        EndDrawing();
    }

    // --- Cleanup ---
    UnloadPieceTextures();
    CloseWindow();
}

// --- Texture Loading and Unloading ---

bool LoadPieceTextures() {
    const char* pieceChars = "_pnbrqk"; // Corresponds to EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
    const char* colorChars = "wb";     // Corresponds to PLAYER_WHITE, PLAYER_BLACK

    for (int c = PLAYER_WHITE; c <= PLAYER_BLACK; c++) {
        for (int p = PAWN; p <= KING; p++) {
            char filename[10];
            snprintf(filename, sizeof(filename), "%c_%c.png", colorChars[c], pieceChars[p]);

            pieceTextures[c][p] = LoadTexture(filename);
            if (pieceTextures[c][p].id == 0) {
                printf("Error: Could not load texture '%s'\n", filename);
                // Unload already loaded textures before returning failure
                for (int pc = PAWN; pc < p; pc++) {
                    if (pieceTextures[c][pc].id > 0) UnloadTexture(pieceTextures[c][pc]);
                }
                for (int cc = PLAYER_WHITE; cc < c; cc++) {
                     for (int pc = PAWN; pc <= KING; pc++) {
                         if (pieceTextures[cc][pc].id > 0) UnloadTexture(pieceTextures[cc][pc]);
                     }
                }
                return false; // Indicate failure
            }
             // printf("Loaded texture '%s'\n", filename); // Optional debug message
        }
    }
    return true; // Indicate success
}

void UnloadPieceTextures() {
     for (int c = PLAYER_WHITE; c <= PLAYER_BLACK; c++) {
       for (int p = PAWN; p <= KING; p++) {

             if (pieceTextures[c][p].id > 0) { // Check if texture was loaded
                UnloadTexture(pieceTextures[c][p]);
             }
        }
    }
}

int main() {
    play_game(); // Call play_game without arguments

    return 0;
}