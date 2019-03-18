#include <stdio.h>
#include <unistd.h>

#include "split.h"

#define WIDTH 80
#define HEIGHT 40

// these are just for the compiler
workspace_t *g_workspace = &(workspace_t){0};
screen_t **g_screens;
size_t g_screens_size;
size_t g_nscreens;
workspace_t **g_workspaces;
size_t g_workspaces_size;
size_t g_nworkspaces;
//

void draw_split(char grid[HEIGHT][WIDTH+1], split_t *split,
                float t, float b, float l, float r){
    if(split->isleaf) return;
    if(split->isvertical){
        float line = t + (b-t)*split->fraction;
        int idx = frac_of(line, HEIGHT);
        for(int i = frac_of(l, WIDTH) + 1; i < frac_of(r, WIDTH); i++){
            grid[idx][i] = '-';
        }
        draw_split(grid, split->frames[0], t, line, l, r);
        draw_split(grid, split->frames[1], line, b, l, r);
    }else{
        float line = l + (r-l)*split->fraction;
        int idx = frac_of(line, WIDTH);
        for(int i = frac_of(t, HEIGHT) + 1; i < frac_of(b, HEIGHT); i++){
            grid[i][idx] = '|';
        }
        draw_split(grid, split->frames[0], t, b, l, line);
        draw_split(grid, split->frames[1], t, b, line, r);
    }
}

void draw_highlight(char grid[HEIGHT][WIDTH+1], split_t *highlight, char c){
    if(!highlight) return;
    sides_t sides = get_sides(highlight);
    for(int row = frac_of(sides.t, HEIGHT) + 1;
            row < frac_of(sides.b, HEIGHT); row ++){
        for(int col = frac_of(sides.l, WIDTH) + 1;
                col < frac_of(sides.r, WIDTH); col ++){
            grid[row][col] = c;
        }
    }
}

void draw_layout(split_t *root, split_t *highlight){
    char grid[HEIGHT][WIDTH+1];
    // set the zeros
    for(int row = 1; row < HEIGHT-1; row++){
        for(int col = 1; col < WIDTH-1; col++){
            grid[row][col] = ' ';
        }
    }
    // set the sides
    for(int row = 1; row < HEIGHT-1; row++){
        grid[row][0] = '|';
        grid[row][WIDTH-1] = '|';
        grid[row][WIDTH] = '\n';
    }
    for(int col = 1; col < WIDTH-1; col++){
        grid[0][col] = '-';
        grid[HEIGHT-1][col] = '-';
    }
    // set the corners
    grid[0][0] = '+';
    grid[0][WIDTH-1] = '+';
    grid[0][WIDTH] = '\n';
    grid[HEIGHT-1][0] = '+';
    grid[HEIGHT-1][WIDTH-1] = '+';
    grid[HEIGHT-1][WIDTH] = '\n';

    draw_split(grid, root, 0.0, 1.0, 0.0, 1.0);

    // highlight the special one
    draw_highlight(grid, highlight, '0');

    highlight = split_move_right(highlight);
    draw_highlight(grid, highlight, '1');

    highlight = split_move_down(highlight);
    draw_highlight(grid, highlight, '2');

    highlight = split_move_up(highlight);
    draw_highlight(grid, highlight, '3');

    highlight = split_move_up(highlight);
    draw_highlight(grid, highlight, '4');

    highlight = split_move_right(highlight);
    draw_highlight(grid, highlight, '5');

    highlight = split_move_up(highlight);
    draw_highlight(grid, highlight, '6');

    highlight = split_move_left(highlight);
    draw_highlight(grid, highlight, '7');

    highlight = split_move_right(highlight);
    draw_highlight(grid, highlight, '8');

    highlight = split_move_right(highlight);
    draw_highlight(grid, highlight, '9');

    highlight = split_move_left(highlight);
    draw_highlight(grid, highlight, '*');

    // clear screen
    write(1, "\x1b[2J", 4);
    // draw grid
    write(1, &grid, HEIGHT*(WIDTH+1));
}

#define vsplit(split, fraction) split_do_split(split, true, fraction)
#define hsplit(split, fraction) split_do_split(split, false, fraction)

int main(){
    split_t *root = split_new(NULL);
    split_t *highlight = NULL;
    if(!root) return 1;
    if(hsplit(root, .5)) goto fail;
    split_t *sptr = root->frames[0];
    if(vsplit(sptr, .6)) goto fail;
    sptr = sptr->frames[0];
    if(hsplit(sptr, .5)) goto fail;
    sptr = sptr->frames[1];
    if(vsplit(sptr, .6)) goto fail;
    sptr = sptr->frames[1];
    if(hsplit(sptr, .5)) goto fail;
    sptr = sptr->frames[0];
    if(vsplit(sptr, .5)) goto fail;
    sptr = sptr->frames[0];
    if(hsplit(sptr, .6)) goto fail;
    sptr = sptr->frames[1];
    if(vsplit(sptr, .6)) goto fail;
    sptr = sptr->frames[1];

    highlight = sptr;

    draw_layout(root, highlight);
    split_free(root);
    return 0;

fail:
    split_free(root);
    return 1;
}
