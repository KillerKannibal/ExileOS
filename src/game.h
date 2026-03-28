#ifndef GAME_H
#define GAME_H

#define MAP_W 16
#define MAP_H 16
// 1 = Red Wall, 2 = Green Wall, 0 = Empty
extern int world_map[MAP_W][MAP_H];

// Fixed point math (16.16) to avoid floating point issues
#define F_SHIFT 16
#define F_MUL 65536
#define TO_INT(x) ((x) >> F_SHIFT)
#define TO_FIX(x) ((x) << F_SHIFT)
#define ABS(x) ((x) < 0 ? -(x) : (x))

// Player start position (3.5, 3.5)
extern int p_x, p_y, p_dir_x, p_dir_y, p_plane_x, p_plane_y;

#endif