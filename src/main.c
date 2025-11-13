#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "raylib.h"
#define CCFUNCS_IMPLEMENTATION
#include "CCFuncs.h"

#define PANEL_COLS 6 // total columns per row in a panel
#define PANEL_ROWS 12 // visible rows of a panel, in practice it could have infinite rows

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum {
    BLOCK_NONE = 0,
    BLOCK_YELLOW,
    BLOCK_GREEN,
    BLOCK_BLUE,
    BLOCK_RED,
    BLOCK_PURPLE,
} BlockType;

typedef struct {
    BlockType type;
    bool isPartOfCombo; // used by the combo system
    bool falling;
} Block;

// this colors are in the same order as the BlockType enum
const Color BLOCK_COLORS[] = {{0, 0, 0, 0}, YELLOW, GREEN, BLUE, RED, PURPLE};

typedef struct {
    Block items[PANEL_COLS];
} Row;

typedef struct {
    // NOTE: the panel blocks are stored in the array from bottom to top, it means that the first row in the array
    // is the bottom row in the panel.
    struct {
        // a dynamic array of rows with PANEL_COLS columns each
        Row *items;
        size_t count;
        size_t capacity;
    } rows;

    Vector2 pos;
    Vector2 size;
    struct {
        int x;
        int y;
    } cursor;
} Panel;

bool is_block_outbounds(Panel *panel, int row, int col) {
    return row < 0 || row >= panel->rows.count || col < 0 || col >= PANEL_COLS;
}

Block *get_block(Panel *panel, int row, int col) {
    if(is_block_outbounds(panel, row, col)) return NULL;
    return &panel->rows.items[row].items[col];
}

bool can_block_combo(Panel *panel, int row, int col, BlockType type) {
    Block *b = get_block(panel, row, col);
    if(b == NULL) return false;

    return b->type == type && !b->falling;
}

void swap_blocks(Panel *panel) {
    int row = PANEL_ROWS - panel->cursor.y - 1;
    int col = panel->cursor.x;

    if(row < panel->rows.count) {
        // swap blocks
        Block *leftBlock = get_block(panel, row, col);
        if(leftBlock == NULL) {
            log_error("Left block of the cursor is NULL (row: %d, col: %d)", row, col);
            return;
        }

        Block *rightBlock = get_block(panel, row, col + 1);
        if(rightBlock == NULL) {
            log_error("Right block of the cursor is NULL (row: %d, col: %d)", row, col);
            return;
        }

        BlockType t = leftBlock->type;
        leftBlock->type = rightBlock->type;
        rightBlock->type = t;
    }
}

void update_cursor(Panel *panel) {
    if(IsKeyPressed(KEY_RIGHT)) {
        panel->cursor.x = MIN(panel->cursor.x + 1, PANEL_COLS - 2);
    } else if(IsKeyPressed(KEY_LEFT)) {
        panel->cursor.x = MAX(panel->cursor.x - 1, 0);
    }

    if(IsKeyPressed(KEY_UP)) {
        panel->cursor.y = MAX(panel->cursor.y - 1, 0);
    } else if(IsKeyPressed(KEY_DOWN)) {
        panel->cursor.y = MIN(panel->cursor.y + 1, PANEL_ROWS - 1);
    }

    if(IsKeyPressed(KEY_X)) swap_blocks(panel);
}

void update_combos(Panel *panel) {
    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *b = get_block(panel, row, col);

            if(b->type == BLOCK_NONE || b->falling) continue;

            // xCount describes matching block in the x axis
            // yCount describes matching block in the y axis
            int xCount = 1, yCount = 1;

            while(can_block_combo(panel, row, col + xCount, b->type)) {
                xCount++;
            }

            while(can_block_combo(panel, row + yCount, col, b->type)) {
                yCount++;
            }

            if(xCount >= 3) {
                // skip the current (first) block
                for(int i = 1; i < xCount; i++) {
                    Block *block = get_block(panel, row, col + i);
                    block->isPartOfCombo = true;
                }
            }

            if(yCount >= 3) {
                // skip the current (first) block
                for(int i = 1; i < yCount; i++) {
                    Block *block = get_block(panel, row + i, col);
                    block->isPartOfCombo = true;
                }
            }

            if(!b->isPartOfCombo) {
                b->isPartOfCombo = xCount >= 3 || yCount >= 3;
            }
        }
    }

    // remove all blocks that form combos
    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *b = get_block(panel, row, col);
            if(!b->isPartOfCombo) continue;
            b->type = BLOCK_NONE;
            b->isPartOfCombo = false;
        }
    }
}

void update_gravity(Panel *panel) {
    // TODO: this is temporal!
    static float t = 0;
    t += GetFrameTime();

    if(t < 0.5) {
        return;
    }

    t = 0;

    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *block = get_block(panel, row, col);
            block->falling = false;

            if(row > 0) {
                Block *botBlock = get_block(panel, row - 1, col);
                block->falling = botBlock->falling || botBlock->type == BLOCK_NONE;
            }

            if(block->type != BLOCK_NONE) continue;

            // can go outbunds but it's handled correctly
            Block *topBlock = get_block(panel, row + 1, col);
            if(topBlock == NULL || topBlock->type == BLOCK_NONE) continue;

            block->type = topBlock->type;
            topBlock->type = BLOCK_NONE;
        }
    }
}

void update_panel(Panel *panel) {
    update_cursor(panel);
    update_combos(panel);
    update_gravity(panel);
}

void draw_panel(Panel *panel) {
    Vector2 blockSize = {
        .x = panel->size.x / PANEL_COLS,
        .y = panel->size.y / PANEL_ROWS,
    };

    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *block = get_block(panel, row, col);
            if(block->type == BLOCK_NONE) continue;

            Color color = BLOCK_COLORS[block->type];

            int x = panel->pos.x + col * blockSize.x;
            int y = panel->pos.y + panel->size.y - (row + 1) * blockSize.y;

            DrawRectangle(x, y, blockSize.x, blockSize.y, color);
        }
    }

    Rectangle cursorRec = {
        .x = panel->pos.x + panel->cursor.x * blockSize.x,
        .y = panel->pos.y + panel->cursor.y * blockSize.y,
        .width = blockSize.x * 2,
        .height = blockSize.y,
    };
    DrawRectangleLinesEx(cursorRec, 5, WHITE);
}

int main(void) {
    InitWindow(1280, 720, "C Tetris");
    SetTargetFPS(60);

    Vector2 panelSize = {GetScreenHeight() / PANEL_ROWS * PANEL_COLS, GetScreenHeight()};
    Panel panel = {
        .pos = {
            .x = GetScreenWidth() / 2 - panelSize.x / 2,
            .y = GetScreenHeight() / 2 - panelSize.y / 2,
        },
        .size = panelSize,
    };

    srand(time(NULL));
    for(int j = 0; j < 10; j++) {
        Row row = {0};

        for(int i = 0; i < PANEL_COLS; i++) {
            row.items[i].type = rand() % 5 + 1;
        }

        da_append(&panel.rows, row);
    }

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        update_panel(&panel);
        draw_panel(&panel);

        EndDrawing();
    }

    CloseWindow();
}
