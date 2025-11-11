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
    BLOCK_BLUE,
    BLOCK_RED,
    BLOCK_PURPLE,
} BlockType;

typedef struct {
    BlockType type;
    bool combo; // used by the combo system
} Block;

// this colors are in the same order as the BlockType enum
const Color BLOCK_COLORS[] = {{0, 0, 0, 0}, YELLOW, BLUE, RED, PURPLE};

typedef struct {
    Block items[PANEL_COLS];
} Row;

typedef struct {
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

Block *panel_get_block(Panel *panel, int row, int col) {
    if(row < 0 || row > panel->rows.count - 1 || col < 0 || col >= PANEL_COLS) {
        return NULL;
    }

    return &panel->rows.items[row].items[col];
}

bool panel_is_block(Panel *panel, int row, int col, BlockType type) {
    if(row < 0 || row > panel->rows.count - 1 || col < 0 || col >= PANEL_COLS) {
        return false;
    }

    Block b = panel->rows.items[row].items[col];
    // TODO: the name of the function doesn't describe or let know the use of b.combo
    return !b.combo && b.type == type;
}

void update_panel(Panel *panel) {
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

    if(IsKeyPressed(KEY_X)) {
        int row = PANEL_ROWS - panel->cursor.y - 1;
        int col = panel->cursor.x;

        if(row < panel->rows.count) {
            // swap blocks
            // TODO: check for NULL
            Block *leftBlock = panel_get_block(panel, row, col);
            Block *rightBlock = panel_get_block(panel, row, col + 1);

            BlockType t = leftBlock->type;
            leftBlock->type = rightBlock->type;
            rightBlock->type = t;
        }
    }

    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *b = panel_get_block(panel, row, col);

            if(b->type == BLOCK_NONE) continue;

            // xCount describes matching block in the x axis
            // yCount describes matching block in the y axis
            // this algorithm only check the blocks in the right and the top
            int xCount = 1, yCount = 1;

            while(panel_is_block(panel, row, col + xCount, b->type)) {
                xCount++;
            }

            while(panel_is_block(panel, row + yCount, col, b->type)) {
                yCount++;
            }

            if(xCount >= 3) {
                // skip the current (first) block
                for(int i = 1; i < xCount; i++) {
                    Block *block = panel_get_block(panel, row, col + i);
                    block->combo = true;
                }
            }

            if(yCount >= 3) {
                // skip the current (first) block
                for(int i = 1; i < yCount; i++) {
                    Block *block = panel_get_block(panel, row + i, col);
                    block->combo = true;
                }
            }

            if(xCount >= 3 || yCount >= 3) {
                b->combo = true;
            }
        }
    }

    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *b = panel_get_block(panel, row, col);
            if(!b->combo) continue;
            b->type = BLOCK_NONE;
            b->combo = false;
        }
    }

    // TODO: make this better!
    static float t = 0;
    t += GetFrameTime();

    if(t < 0.5) {
        return;
    }

    t = 0;

    for(int row = 0; row < panel->rows.count - 1; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *botBlock = panel_get_block(panel, row, col);
            if(botBlock->type != BLOCK_NONE) continue;

            Block *topBlock = panel_get_block(panel, row + 1, col);
            if(topBlock->type == BLOCK_NONE) continue;

            botBlock->type = topBlock->type;
            topBlock->type = BLOCK_NONE;
        }
    }
}

void draw_panel(Panel *panel) {
    Vector2 blockSize = {
        .x = panel->size.x / PANEL_COLS,
        .y = panel->size.y / PANEL_ROWS,
    };

    for(int row = 0; row < panel->rows.count; row++) {
        for(int col = 0; col < PANEL_COLS; col++) {
            Block *block = panel_get_block(panel, row, col);
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

        for(int i = 0; i < PANEL_COLS - 1; i++) {
            row.items[i].type = rand() % 5;
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
