#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#define BOARD_SIZE 8
#define BOARD_LAYERS 3
#define MAX_MOVES 512  // Increased for 3D
#define INFINITY 1000000

enum Piece {
    EMPTY,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

enum Color {
    WHITE,
    BLACK,
    NONE
};

struct Square {
    enum Piece piece;
    enum Color color;
};

struct Move {
    int from_layer;
    int from_row;
    int from_col;
    int to_layer;
    int to_row;
    int to_col;
    enum Piece promotion;
    int score;
    bool is_capture;
};

struct Board {
    struct Square squares[BOARD_LAYERS][BOARD_SIZE][BOARD_SIZE];
    enum Color current_player;
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
    // Initialize all squares to empty
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                board->squares[layer][row][col].piece = EMPTY;
                board->squares[layer][row][col].color = NONE;
            }
        }
    }

    // Layer 0 (Bottom layer)
    // Set up pawns
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[0][1][col].piece = PAWN;
        board->squares[0][1][col].color = BLACK;
        board->squares[0][6][col].piece = PAWN;
        board->squares[0][6][col].color = WHITE;
    }

    // Set up other pieces
    enum Piece back_row[BOARD_SIZE] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[0][0][col].piece = back_row[col];
        board->squares[0][0][col].color = BLACK;
        board->squares[0][7][col].piece = back_row[col];
        board->squares[0][7][col].color = WHITE;
    }

    // Layer 1 (Middle layer) - Empty except for special pieces
    // Add some special pieces to make 3D interesting
    board->squares[1][3][3].piece = QUEEN;
    board->squares[1][3][3].color = WHITE;
    board->squares[1][4][4].piece = QUEEN;
    board->squares[1][4][4].color = BLACK;

    // Layer 2 (Top layer) - Mirror of bottom layer but with colors reversed
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[2][1][col].piece = PAWN;
        board->squares[2][1][col].color = WHITE;  // Note color reversal
        board->squares[2][6][col].piece = PAWN;
        board->squares[2][6][col].color = BLACK;  // Note color reversal
    }
    for (int col = 0; col < BOARD_SIZE; col++) {
        board->squares[2][0][col].piece = back_row[col];
        board->squares[2][0][col].color = WHITE;  // Note color reversal
        board->squares[2][7][col].piece = back_row[col];
        board->squares[2][7][col].color = BLACK;  // Note color reversal
    }

    // Game state
    board->current_player = WHITE;
    board->white_castle_kingside = true;
    board->white_castle_queenside = true;
    board->black_castle_kingside = true;
    board->black_castle_queenside = true;
    board->en_passant_layer = -1;
    board->en_passant_row = -1;
    board->en_passant_col = -1;
    board->halfmove_clock = 0;
    board->fullmove_number = 1;
}

void print_layer(const struct Board *board, int layer) {
    printf("\nLayer %d:\n", layer + 1);
    printf("  a b c d e f g h\n");
    for (int row = 0; row < BOARD_SIZE; row++) {
        printf("%d ", 8 - row);
        for (int col = 0; col < BOARD_SIZE; col++) {
            char piece_char = ' ';
            switch (board->squares[layer][row][col].piece) {
                case PAWN:   piece_char = 'P'; break;
                case KNIGHT: piece_char = 'N'; break;
                case BISHOP: piece_char = 'B'; break;
                case ROOK:   piece_char = 'R'; break;
                case QUEEN:  piece_char = 'Q'; break;
                case KING:   piece_char = 'K'; break;
                case EMPTY:   piece_char = '.'; break;
            }
            
            if (board->squares[layer][row][col].color == BLACK) {
                piece_char = tolower(piece_char);
            }
            printf("%c ", piece_char);
        }
        printf("%d\n", 8 - row);
    }
    printf("  a b c d e f g h\n");
}

void print_board(const struct Board *board) {
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        print_layer(board, layer);
    }
    printf("%s to move\n", board->current_player == WHITE ? "White" : "Black");
}

bool is_valid_position(int layer, int row, int col) {
    return layer >= 0 && layer < BOARD_LAYERS && 
           row >= 0 && row < BOARD_SIZE && 
           col >= 0 && col < BOARD_SIZE;
}

void generate_pawn_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int direction = (board->squares[layer][row][col].color == WHITE) ? -1 : 1;
    int start_row = (board->squares[layer][row][col].color == WHITE) ? 6 : 1;
    
    // Standard forward move (same layer)
    if (is_valid_position(layer, row + direction, col) && 
        board->squares[layer][row + direction][col].piece == EMPTY) {
        moves[*move_count].from_layer = layer;
        moves[*move_count].from_row = row;
        moves[*move_count].from_col = col;
        moves[*move_count].to_layer = layer;
        moves[*move_count].to_row = row + direction;
        moves[*move_count].to_col = col;
        moves[*move_count].promotion = EMPTY;
        moves[*move_count].is_capture = false;
        (*move_count)++;
        
        // Double move from starting position (same layer)
        if (row == start_row && 
            board->squares[layer][row + 2*direction][col].piece == EMPTY) {
            moves[*move_count].from_layer = layer;
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_layer = layer;
            moves[*move_count].to_row = row + 2*direction;
            moves[*move_count].to_col = col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].is_capture = false;
            (*move_count)++;
        }
    }
    
    // Capture moves (same layer)
    for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
        if (is_valid_position(layer, row + direction, col + col_offset)) {
            if (board->squares[layer][row + direction][col + col_offset].piece != EMPTY &&
                board->squares[layer][row + direction][col + col_offset].color != board->squares[layer][row][col].color) {
                moves[*move_count].from_layer = layer;
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_layer = layer;
                moves[*move_count].to_row = row + direction;
                moves[*move_count].to_col = col + col_offset;
                moves[*move_count].promotion = EMPTY;
                moves[*move_count].is_capture = true;
                (*move_count)++;
            }
            else if (layer == board->en_passant_layer &&
                     row + direction == board->en_passant_row && 
                     col + col_offset == board->en_passant_col) {
                moves[*move_count].from_layer = layer;
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_layer = layer;
                moves[*move_count].to_row = row + direction;
                moves[*move_count].to_col = col + col_offset;
                moves[*move_count].promotion = EMPTY;
                moves[*move_count].is_capture = true;
                (*move_count)++;
            }
        }
    }
    
    // Inter-layer moves (pawns can move up or down one layer when moving forward)
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
        int new_layer = layer + layer_change;
        if (is_valid_position(new_layer, row + direction, col) && 
            board->squares[new_layer][row + direction][col].piece == EMPTY) {
            moves[*move_count].from_layer = layer;
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_layer = new_layer;
            moves[*move_count].to_row = row + direction;
            moves[*move_count].to_col = col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].is_capture = false;
            (*move_count)++;
        }
        
        // Inter-layer captures
        for (int col_offset = -1; col_offset <= 1; col_offset += 2) {
            if (is_valid_position(new_layer, row + direction, col + col_offset)) {
                if (board->squares[new_layer][row + direction][col + col_offset].piece != EMPTY &&
                    board->squares[new_layer][row + direction][col + col_offset].color != board->squares[layer][row][col].color) {
                    moves[*move_count].from_layer = layer;
                    moves[*move_count].from_row = row;
                    moves[*move_count].from_col = col;
                    moves[*move_count].to_layer = new_layer;
                    moves[*move_count].to_row = row + direction;
                    moves[*move_count].to_col = col + col_offset;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = true;
                    (*move_count)++;
                }
            }
        }
    }
    
    // Promotion (on any layer)
    if ((row + direction == 0 || row + direction == 7) && 
        board->squares[layer][row + direction][col].piece == EMPTY) {
        enum Piece promo_pieces[] = {QUEEN, ROOK, BISHOP, KNIGHT};
        for (int i = 0; i < 4; i++) {
            moves[*move_count].from_layer = layer;
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_layer = layer;
            moves[*move_count].to_row = row + direction;
            moves[*move_count].to_col = col;
            moves[*move_count].promotion = promo_pieces[i];
            moves[*move_count].is_capture = false;
            (*move_count)++;
        }
    }
}

void generate_knight_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int knight_moves[8][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    
    // Standard knight moves (same layer)
    for (int i = 0; i < 8; i++) {
        int new_row = row + knight_moves[i][0];
        int new_col = col + knight_moves[i][1];
        
        if (is_valid_position(layer, new_row, new_col) &&
            (board->squares[layer][new_row][new_col].piece == EMPTY ||
             board->squares[layer][new_row][new_col].color != board->squares[layer][row][col].color)) {
            moves[*move_count].from_layer = layer;
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_layer = layer;
            moves[*move_count].to_row = new_row;
            moves[*move_count].to_col = new_col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].is_capture = (board->squares[layer][new_row][new_col].piece != EMPTY);
            (*move_count)++;
        }
    }
    
    // 3D knight moves (can jump between layers)
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
        int new_layer = layer + layer_change;
        if (new_layer >= 0 && new_layer < BOARD_LAYERS) {
            for (int i = 0; i < 8; i++) {
                int new_row = row + knight_moves[i][0];
                int new_col = col + knight_moves[i][1];
                
                if (is_valid_position(new_layer, new_row, new_col) &&
                    (board->squares[new_layer][new_row][new_col].piece == EMPTY ||
                     board->squares[new_layer][new_row][new_col].color != board->squares[layer][row][col].color)) {
                    moves[*move_count].from_layer = layer;
                    moves[*move_count].from_row = row;
                    moves[*move_count].from_col = col;
                    moves[*move_count].to_layer = new_layer;
                    moves[*move_count].to_row = new_row;
                    moves[*move_count].to_col = new_col;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = (board->squares[new_layer][new_row][new_col].piece != EMPTY);
                    (*move_count)++;
                }
            }
        }
    }
}

void generate_directional_moves(const struct Board *board, int layer, int row, int col, 
                               int row_dir, int col_dir, struct Move moves[], int *move_count) {
    // Same layer moves
    int new_row = row + row_dir;
    int new_col = col + col_dir;
    
    while (is_valid_position(layer, new_row, new_col)) {
        if (board->squares[layer][new_row][new_col].piece == EMPTY) {
            moves[*move_count].from_layer = layer;
            moves[*move_count].from_row = row;
            moves[*move_count].from_col = col;
            moves[*move_count].to_layer = layer;
            moves[*move_count].to_row = new_row;
            moves[*move_count].to_col = new_col;
            moves[*move_count].promotion = EMPTY;
            moves[*move_count].is_capture = false;
            (*move_count)++;
        } else {
            if (board->squares[layer][new_row][new_col].color != board->squares[layer][row][col].color) {
                moves[*move_count].from_layer = layer;
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_layer = layer;
                moves[*move_count].to_row = new_row;
                moves[*move_count].to_col = new_col;
                moves[*move_count].promotion = EMPTY;
                moves[*move_count].is_capture = true;
                (*move_count)++;
            }
            break;
        }
        
        new_row += row_dir;
        new_col += col_dir;
    }
    
    // Inter-layer moves (bishops, rooks, queens can move between layers)
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
        int new_layer = layer + layer_change;
        if (new_layer >= 0 && new_layer < BOARD_LAYERS) {
            new_row = row + row_dir;
            new_col = col + col_dir;
            
            while (is_valid_position(new_layer, new_row, new_col)) {
                if (board->squares[new_layer][new_row][new_col].piece == EMPTY) {
                    moves[*move_count].from_layer = layer;
                    moves[*move_count].from_row = row;
                    moves[*move_count].from_col = col;
                    moves[*move_count].to_layer = new_layer;
                    moves[*move_count].to_row = new_row;
                    moves[*move_count].to_col = new_col;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = false;
                    (*move_count)++;
                } else {
                    if (board->squares[new_layer][new_row][new_col].color != board->squares[layer][row][col].color) {
                        moves[*move_count].from_layer = layer;
                        moves[*move_count].from_row = row;
                        moves[*move_count].from_col = col;
                        moves[*move_count].to_layer = new_layer;
                        moves[*move_count].to_row = new_row;
                        moves[*move_count].to_col = new_col;
                        moves[*move_count].promotion = EMPTY;
                        moves[*move_count].is_capture = true;
                        (*move_count)++;
                    }
                    break;
                }
                
                new_row += row_dir;
                new_col += col_dir;
            }
        }
    }
}

void generate_bishop_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, layer, row, col, directions[i][0], directions[i][1], moves, move_count);
    }
}

void generate_rook_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    
    for (int i = 0; i < 4; i++) {
        generate_directional_moves(board, layer, row, col, directions[i][0], directions[i][1], moves, move_count);
    }
}

void generate_queen_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    generate_bishop_moves(board, layer, row, col, moves, move_count);
    generate_rook_moves(board, layer, row, col, moves, move_count);
}

void generate_king_moves(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    // Standard king moves (same layer)
    for (int row_dir = -1; row_dir <= 1; row_dir++) {
        for (int col_dir = -1; col_dir <= 1; col_dir++) {
            if (row_dir == 0 && col_dir == 0) continue;
            
            int new_row = row + row_dir;
            int new_col = col + col_dir;
            
            if (is_valid_position(layer, new_row, new_col) &&
                (board->squares[layer][new_row][new_col].piece == EMPTY ||
                 board->squares[layer][new_row][new_col].color != board->squares[layer][row][col].color)) {
                moves[*move_count].from_layer = layer;
                moves[*move_count].from_row = row;
                moves[*move_count].from_col = col;
                moves[*move_count].to_layer = layer;
                moves[*move_count].to_row = new_row;
                moves[*move_count].to_col = new_col;
                moves[*move_count].promotion = EMPTY;
                moves[*move_count].is_capture = (board->squares[layer][new_row][new_col].piece != EMPTY);
                (*move_count)++;
            }
        }
    }
    
    // 3D king moves (can move to adjacent layers)
    for (int layer_change = -1; layer_change <= 1; layer_change += 2) {
        int new_layer = layer + layer_change;
        if (new_layer >= 0 && new_layer < BOARD_LAYERS) {
            for (int row_dir = -1; row_dir <= 1; row_dir++) {
                for (int col_dir = -1; col_dir <= 1; col_dir++) {
                    if (row_dir == 0 && col_dir == 0) continue;
                    
                    int new_row = row + row_dir;
                    int new_col = col + col_dir;
                    
                    if (is_valid_position(new_layer, new_row, new_col) &&
                        (board->squares[new_layer][new_row][new_col].piece == EMPTY ||
                         board->squares[new_layer][new_row][new_col].color != board->squares[layer][row][col].color)) {
                        moves[*move_count].from_layer = layer;
                        moves[*move_count].from_row = row;
                        moves[*move_count].from_col = col;
                        moves[*move_count].to_layer = new_layer;
                        moves[*move_count].to_row = new_row;
                        moves[*move_count].to_col = new_col;
                        moves[*move_count].promotion = EMPTY;
                        moves[*move_count].is_capture = (board->squares[new_layer][new_row][new_col].piece != EMPTY);
                        (*move_count)++;
                    }
                }
            }
        }
    }
    
    // Castling (only on bottom layer)
    if (layer == 0) {
        if (board->squares[layer][row][col].color == WHITE) {
            if (board->white_castle_kingside) {
                if (board->squares[0][7][5].piece == EMPTY && board->squares[0][7][6].piece == EMPTY) {
                    moves[*move_count].from_layer = 0;
                    moves[*move_count].from_row = 7;
                    moves[*move_count].from_col = 4;
                    moves[*move_count].to_layer = 0;
                    moves[*move_count].to_row = 7;
                    moves[*move_count].to_col = 6;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = false;
                    (*move_count)++;
                }
            }
            if (board->white_castle_queenside) {
                if (board->squares[0][7][3].piece == EMPTY && board->squares[0][7][2].piece == EMPTY && board->squares[0][7][1].piece == EMPTY) {
                    moves[*move_count].from_layer = 0;
                    moves[*move_count].from_row = 7;
                    moves[*move_count].from_col = 4;
                    moves[*move_count].to_layer = 0;
                    moves[*move_count].to_row = 7;
                    moves[*move_count].to_col = 2;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = false;
                    (*move_count)++;
                }
            }
        } else {
            if (board->black_castle_kingside) {
                if (board->squares[0][0][5].piece == EMPTY && board->squares[0][0][6].piece == EMPTY) {
                    moves[*move_count].from_layer = 0;
                    moves[*move_count].from_row = 0;
                    moves[*move_count].from_col = 4;
                    moves[*move_count].to_layer = 0;
                    moves[*move_count].to_row = 0;
                    moves[*move_count].to_col = 6;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = false;
                    (*move_count)++;
                }
            }
            if (board->black_castle_queenside) {
                if (board->squares[0][0][3].piece == EMPTY && board->squares[0][0][2].piece == EMPTY && board->squares[0][0][1].piece == EMPTY) {
                    moves[*move_count].from_layer = 0;
                    moves[*move_count].from_row = 0;
                    moves[*move_count].from_col = 4;
                    moves[*move_count].to_layer = 0;
                    moves[*move_count].to_row = 0;
                    moves[*move_count].to_col = 2;
                    moves[*move_count].promotion = EMPTY;
                    moves[*move_count].is_capture = false;
                    (*move_count)++;
                }
            }
        }
    }
}

void generate_moves_for_piece(const struct Board *board, int layer, int row, int col, struct Move moves[], int *move_count) {
    switch (board->squares[layer][row][col].piece) {
        case PAWN:
            generate_pawn_moves(board, layer, row, col, moves, move_count);
            break;
        case KNIGHT:
            generate_knight_moves(board, layer, row, col, moves, move_count);
            break;
        case BISHOP:
            generate_bishop_moves(board, layer, row, col, moves, move_count);
            break;
        case ROOK:
            generate_rook_moves(board, layer, row, col, moves, move_count);
            break;
        case QUEEN:
            generate_queen_moves(board, layer, row, col, moves, move_count);
            break;
        case KING:
            generate_king_moves(board, layer, row, col, moves, move_count);
            break;
        case EMPTY:
            break;
    }
}

int generate_all_moves(const struct Board *board, struct Move moves[]) {
    int move_count = 0;
    
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board->squares[layer][row][col].piece != EMPTY && 
                    board->squares[layer][row][col].color == board->current_player) {
                    generate_moves_for_piece(board, layer, row, col, moves, &move_count);
                }
            }
        }
    }
    
    return move_count;
}

void make_move(struct Board *board, const struct Move *move) {
    // Track if this move captures a piece
    bool is_capture = board->squares[move->to_layer][move->to_row][move->to_col].piece != EMPTY;
    enum Piece captured_piece = board->squares[move->to_layer][move->to_row][move->to_col].piece;
    enum Color captured_color = board->squares[move->to_layer][move->to_row][move->to_col].color;

    // Handle castling (only on bottom layer)
    if (move->from_layer == 0 && board->squares[move->from_layer][move->from_row][move->from_col].piece == KING) {
        int col_diff = move->to_col - move->from_col;
        if (abs(col_diff) == 2) {
            if (col_diff > 0) {
                board->squares[0][move->from_row][5] = board->squares[0][move->from_row][7];
                board->squares[0][move->from_row][7].piece = EMPTY;
                board->squares[0][move->from_row][7].color = NONE;
            } else {
                board->squares[0][move->from_row][3] = board->squares[0][move->from_row][0];
                board->squares[0][move->from_row][0].piece = EMPTY;
                board->squares[0][move->from_row][0].color = NONE;
            }
        }
        
        if (board->current_player == WHITE) {
            board->white_castle_kingside = false;
            board->white_castle_queenside = false;
        } else {
            board->black_castle_kingside = false;
            board->black_castle_queenside = false;
        }
    }
    
    // Handle en passant
    if (board->squares[move->from_layer][move->from_row][move->from_col].piece == PAWN && 
        move->to_col != move->from_col && 
        board->squares[move->to_layer][move->to_row][move->to_col].piece == EMPTY) {
        is_capture = true;
        captured_piece = PAWN;
        captured_color = (board->current_player == WHITE) ? BLACK : WHITE;
        board->squares[move->from_layer][move->from_row][move->to_col].piece = EMPTY;
        board->squares[move->from_layer][move->from_row][move->to_col].color = NONE;
    }
    
    // Handle promotion
    if (move->promotion != EMPTY) {
        board->squares[move->to_layer][move->to_row][move->to_col].piece = move->promotion;
    } else {
        board->squares[move->to_layer][move->to_row][move->to_col].piece = 
            board->squares[move->from_layer][move->from_row][move->from_col].piece;
    }
    
    board->squares[move->to_layer][move->to_row][move->to_col].color = 
        board->squares[move->from_layer][move->from_row][move->from_col].color;
    board->squares[move->from_layer][move->from_row][move->from_col].piece = EMPTY;
    board->squares[move->from_layer][move->from_row][move->from_col].color = NONE;
    
    // Display capture message if a piece was captured
    if (is_capture) {
        const char *piece_names[] = {"", "Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};
        const char *colors[] = {"White", "Black"};
        
        printf("%s %s captured %s %s on layer %d!\n",
               colors[board->current_player],
               piece_names[board->squares[move->to_layer][move->to_row][move->to_col].piece],
               colors[captured_color],
               piece_names[captured_piece],
               move->to_layer + 1);
    }
    
    // Update en passant target
    board->en_passant_layer = -1;
    board->en_passant_row = -1;
    board->en_passant_col = -1;
    if (board->squares[move->to_layer][move->to_row][move->to_col].piece == PAWN && 
        abs(move->to_row - move->from_row) == 2) {
        board->en_passant_layer = move->from_layer;
        board->en_passant_row = (move->from_row + move->to_row) / 2;
        board->en_passant_col = move->from_col;
    }
    
    // Update castling rights if rook moves (only on bottom layer)
    if (move->from_layer == 0 && board->squares[move->from_layer][move->from_row][move->from_col].piece == ROOK) {
        if (board->current_player == WHITE) {
            if (move->from_row == 7 && move->from_col == 0) {
                board->white_castle_queenside = false;
            } else if (move->from_row == 7 && move->from_col == 7) {
                board->white_castle_kingside = false;
            }
        } else {
            if (move->from_row == 0 && move->from_col == 0) {
                board->black_castle_queenside = false;
            } else if (move->from_row == 0 && move->from_col == 7) {
                board->black_castle_kingside = false;
            }
        }
    }
    
    // Update move counters
    if (board->current_player == BLACK) {
        board->fullmove_number++;
    }
    
    // Switch player
    board->current_player = (board->current_player == WHITE) ? BLACK : WHITE;
}

void undo_move(struct Board *board, const struct Move *move) {
    board->squares[move->from_layer][move->from_row][move->from_col] = 
        board->squares[move->to_layer][move->to_row][move->to_col];
    board->squares[move->to_layer][move->to_row][move->to_col].piece = EMPTY;
    board->squares[move->to_layer][move->to_row][move->to_col].color = NONE;
    
    board->current_player = (board->current_player == WHITE) ? BLACK : WHITE;
}

int evaluate_board(const struct Board *board) {
    int score = 0;
    int piece_values[] = {0, 100, 320, 330, 500, 900, 20000};
    
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board->squares[layer][row][col].piece != EMPTY) {
                    int value = piece_values[board->squares[layer][row][col].piece];
                    
                    // Bonus for controlling center squares on any layer
                    if ((row >= 3 && row <= 4) && (col >= 3 && col <= 4)) {
                        value += 10;
                    }
                    
                    // Bonus for being on higher layers (more strategic positions)
                    value += layer * 5;
                    
                    if (board->squares[layer][row][col].color == WHITE) {
                        score += value;
                    } else {
                        score -= value;
                    }
                }
            }
        }
    }
    
    return (board->current_player == WHITE) ? score : -score;
}

bool is_game_over(const struct Board *board) {
    bool white_king = false;
    bool black_king = false;
    
    for (int layer = 0; layer < BOARD_LAYERS; layer++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            for (int col = 0; col < BOARD_SIZE; col++) {
                if (board->squares[layer][row][col].piece == KING) {
                    if (board->squares[layer][row][col].color == WHITE) {
                        white_king = true;
                    } else {
                        black_king = true;
                    }
                }
            }
        }
    }
    
    return !white_king || !black_king;
}

int minimax(struct Board *board, int depth, int alpha, int beta, bool maximizing_player) {
    if (depth == 0 || is_game_over(board)) {
        return evaluate_board(board);
    }
    
    struct Move moves[MAX_MOVES];
    int move_count = generate_all_moves(board, moves);
    
    if (maximizing_player) {
        int max_eval = -INFINITY;
        for (int i = 0; i < move_count; i++) {
            make_move(board, &moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, false);
            undo_move(board, &moves[i]);
            
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > eval) ? alpha : eval;
            if (beta <= alpha) {
                break;
            }
        }
        return max_eval;
    } else {
        int min_eval = INFINITY;
        for (int i = 0; i < move_count; i++) {
            make_move(board, &moves[i]);
            int eval = minimax(board, depth - 1, alpha, beta, true);
            undo_move(board, &moves[i]);
            
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < eval) ? beta : eval;
            if (beta <= alpha) {
                break;
            }
        }
        return min_eval;
    }
}

void ai_make_move(struct Board *board, int difficulty) {
    struct Move moves[MAX_MOVES];
    int move_count = generate_all_moves(board, moves);
    
    if (move_count == 0) return;
    
    int best_move_index = 0;
    int best_eval = (board->current_player == WHITE) ? -INFINITY : INFINITY;
    
    int depth;
    switch (difficulty) {
        case 1: depth = 1; break;
        case 2: depth = 3; break;
        case 3: depth = 4; break;
        default: depth = 2; break;
    }
    
    for (int i = 0; i < move_count; i++) {
        make_move(board, &moves[i]);
        int eval = minimax(board, depth - 1, -INFINITY, INFINITY, board->current_player == BLACK);
        undo_move(board, &moves[i]);
        
        moves[i].score = eval;
        
        if (board->current_player == WHITE) {
            if (eval > best_eval) {
                best_eval = eval;
                best_move_index = i;
            }
        } else {
            if (eval < best_eval) {
                best_eval = eval;
                best_move_index = i;
            }
        }
    }
    
    make_move(board, &moves[best_move_index]);
    printf("AI moves from %c%d (layer %d) to %c%d (layer %d)\n", 
           'a' + moves[best_move_index].from_col, 
           8 - moves[best_move_index].from_row,
           moves[best_move_index].from_layer + 1,
           'a' + moves[best_move_index].to_col, 
           8 - moves[best_move_index].to_row,
           moves[best_move_index].to_layer + 1);
}

bool parse_move(const char *input, struct Move *move) {
    if (strlen(input) < 5) return false;
    
    move->from_layer = input[0] - '1';
    move->from_col = tolower(input[1]) - 'a';
    move->from_row = 8 - (input[2] - '0');
    move->to_layer = input[3] - '1';
    move->to_col = tolower(input[4]) - 'a';
    move->to_row = 8 - (input[5] - '0');
    move->promotion = EMPTY;
    
    if (strlen(input) == 7) {
        switch (tolower(input[6])) {
            case 'q': move->promotion = QUEEN; break;
            case 'r': move->promotion = ROOK; break;
            case 'b': move->promotion = BISHOP; break;
            case 'n': move->promotion = KNIGHT; break;
            default: return false;
        }
    }
    
    return is_valid_position(move->from_layer, move->from_row, move->from_col) && 
           is_valid_position(move->to_layer, move->to_row, move->to_col);
}

bool is_move_valid(const struct Board *board, const struct Move *move) {
    struct Move moves[MAX_MOVES];
    int move_count = generate_all_moves(board, moves);
    
    for (int i = 0; i < move_count; i++) {
        if (moves[i].from_layer == move->from_layer &&
            moves[i].from_row == move->from_row &&
            moves[i].from_col == move->from_col &&
            moves[i].to_layer == move->to_layer &&
            moves[i].to_row == move->to_row &&
            moves[i].to_col == move->to_col &&
            moves[i].promotion == move->promotion) {
            return true;
        }
    }
    
    return false;
}

void play_game(int difficulty, bool player_is_white) {
    struct Board board;
    init_board(&board);
    
    char input[20];
    
    while (!is_game_over(&board)) {
        print_board(&board);
        
        if ((board.current_player == WHITE && player_is_white) ||
            (board.current_player == BLACK && !player_is_white)) {
            printf("Your move (format: layer from pos from layer to pos to, e.g., 1e2e4 or 1e7e8q for promotion): ");
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = '\0';
            
            struct Move move;
            if (!parse_move(input, &move) || !is_move_valid(&board, &move)) {
                printf("Invalid move. Try again.\n");
                continue;
            }
            
            make_move(&board, &move);
        } else {
            printf("AI is thinking...\n");
            ai_make_move(&board, difficulty);
        }
    }
    
    print_board(&board);
    printf("Game over!\n");
}

int main() {
    printf("3D Chess Game\n");
    display_piece_legend();
    
    printf("Select difficulty:\n");
    printf("1. Easy\n");
    printf("2. Medium\n");
    printf("3. Hard\n");
    printf("Your choice: ");
    
    int difficulty;
    scanf("%d", &difficulty);
    getchar();
    
    if (difficulty < 1 || difficulty > 3) {
        printf("Invalid choice. Defaulting to Medium.\n");
        difficulty = 2;
    }
    
    printf("Choose your color:\n");
    printf("1. White (play first)\n");
    printf("2. Black (play second)\n");
    printf("Your choice: ");
    
    int color_choice;
    scanf("%d", &color_choice);
    getchar();
    
    bool player_is_white = true;
    if (color_choice == 2) {
        player_is_white = false;
    }
    
    play_game(difficulty, player_is_white);
    
    return 0;
}
