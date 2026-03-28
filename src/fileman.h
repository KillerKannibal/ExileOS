#ifndef FILEMAN_H
#define FILEMAN_H

#include "fs.h"

#define FILEMAN_MAX_ENTRIES 50

extern dirent_t fileman_entries[FILEMAN_MAX_ENTRIES];
extern int fileman_num_entries;

void fileman_refresh();

#endif