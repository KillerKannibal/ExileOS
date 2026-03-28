#include "fileman.h"
#include "string.h"
#include "fs.h"

extern char cwd[128]; // From main

dirent_t fileman_entries[FILEMAN_MAX_ENTRIES];
int fileman_num_entries = 0;

void fileman_refresh() {
    fileman_num_entries = 0;

    // Add ".." entry if we are not at root
    if (strcmp(cwd, "/") != 0) {
        strcpy(fileman_entries[0].name, "..");
        fileman_entries[0].ino = 0;
        fileman_entries[0].flags = FS_DIRECTORY;
        fileman_num_entries++;
    }

    int i = 0;
    dirent_t* de;
    while((de = readdir_fs(fs_root, i)) != 0 && fileman_num_entries < FILEMAN_MAX_ENTRIES) {
        // Filter entries based on CWD
        int show = 0;

        if (strcmp(cwd, "/") == 0) {
            // Root: Show files that do not contain '/'
            int has_slash = 0;
            for(int k=0; de->name[k]; k++) if(de->name[k] == '/') has_slash = 1;
            if (!has_slash) show = 1;
        } else {
            // Subdir: Show files that start with "cwd/"
            // cwd is like "/folder", de->name is "folder/file.txt"
            char prefix[128];
            strcpy(prefix, cwd + 1); // Skip leading '/'
            strcat(prefix, "/");

            if (strncmp(de->name, prefix, strlen(prefix)) == 0) {
                // And ensure it's a direct child (no extra slashes)
                char* remainder = de->name + strlen(prefix);
                int has_slash = 0;
                for(int k=0; remainder[k]; k++) if(remainder[k] == '/') has_slash = 1;
                if (!has_slash) show = 1;
            }
        }

        if (show) {
            strcpy(fileman_entries[fileman_num_entries].name, de->name);
            fileman_entries[fileman_num_entries].ino = de->ino;
            fileman_entries[fileman_num_entries].flags = de->flags;
            fileman_num_entries++;
        }
        i++;
    }
}