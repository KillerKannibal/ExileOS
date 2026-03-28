#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_SYSTEM      0x04  // Protected system file

struct fs_node;

typedef struct dirent {
    char name[128];
    uint32_t ino; // Inode number
    uint32_t flags; // FS_FILE or FS_DIRECTORY
} dirent_t;

// Function pointer typedefs for filesystem operations
typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef struct fs_node* (*finddir_type_t)(struct fs_node*, char *name);
typedef struct dirent* (*readdir_type_t)(struct fs_node*, uint32_t index);
typedef struct fs_node* (*mkdir_type_t)(struct fs_node*, char *name);
typedef struct fs_node* (*create_type_t)(struct fs_node*, char *name);

typedef struct fs_node {
    char name[128];
    uint32_t flags;       // FS_FILE or FS_DIRECTORY
    uint32_t length;      // Size of the file
    uintptr_t impl;       // Driver specific data (e.g., address of TAR header)
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    finddir_type_t finddir;
    readdir_type_t readdir;
    mkdir_type_t mkdir;
    create_type_t create;
} fs_node_t;

extern fs_node_t *fs_root;

void init_fs(uintptr_t initrd_location);
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void open_fs(fs_node_t *node);
void close_fs(fs_node_t *node);
fs_node_t *finddir_fs(fs_node_t *node, char *name);
dirent_t *readdir_fs(fs_node_t *node, uint32_t index);
fs_node_t *mkdir_fs(fs_node_t *node, char *name);
fs_node_t *create_fs(fs_node_t *node, char *name);

#endif