#include "fs.h"
#include "string.h"

// --- TAR Specific Structures ---
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
} __attribute__((packed)) tar_header_t;

// --- Helpers ---

unsigned int parse_octal(const char *str, int len) {
    unsigned int n = 0;
    const char *c = str;
    while (len-- > 0 && *c >= '0' && *c <= '7') {
        n = n * 8 + (*c - '0');
        c++;
    }
    return n;
}

// --- VFS Memory Management (Simple Static Pool) ---
fs_node_t *fs_root = 0;
fs_node_t node_pool[32]; // Maximum 32 open files/nodes for now
int node_ptr = 0;

fs_node_t* alloc_node() {
    if (node_ptr >= 32) return 0;
    return &node_pool[node_ptr++];
}

// --- RAM Filesystem (Write Support) ---
#define RAM_FILE_COUNT 32
#define RAM_DISK_SIZE  (1024 * 1024) // 1MB RAM Disk

typedef struct {
    char name[128];
    uint32_t flags;
    uint32_t size;
    uint32_t offset; // Offset in ram_data
} ram_file_t;

ram_file_t ram_files[RAM_FILE_COUNT];
int ram_file_count = 0;

uint8_t ram_data[RAM_DISK_SIZE];
uint32_t ram_data_ptr = 0;

// Write function for RAM files
uint32_t ram_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    ram_file_t* rf = (ram_file_t*)node->impl;
    
    // Simple append-only/overwrite logic for now
    // Ideally we would manage blocks, but for a hobby OS simple linear alloc is fine
    uint32_t end_pos = rf->offset + offset + size;
    if (end_pos > RAM_DISK_SIZE) return 0; // Out of memory

    for(uint32_t i = 0; i < size; i++) {
        ram_data[rf->offset + offset + i] = buffer[i];
    }

    if (offset + size > rf->size) rf->size = offset + size;
    node->length = rf->size;
    
    return size;
}

uint32_t ram_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    ram_file_t* rf = (ram_file_t*)node->impl;
    if (offset > rf->size) return 0;
    if (offset + size > rf->size) size = rf->size - offset;
    
    for(uint32_t i = 0; i < size; i++) {
        buffer[i] = ram_data[rf->offset + offset + i];
    }
    return size;
}

// Create a new file/directory in RAM
fs_node_t* root_create_entry(fs_node_t *parent __attribute__((unused)), char *name, uint32_t flags) {
    if (ram_file_count >= RAM_FILE_COUNT) return 0;

    // Allocate a RAM file entry
    ram_file_t* rf = &ram_files[ram_file_count++];
    strcpy(rf->name, name);
    rf->flags = flags;
    rf->size = 0;
    rf->offset = ram_data_ptr;

    // Reserve some space? For now we just point to the end.
    // Proper allocators would be better, but this works for small text files.
    // We'll bump the pointer when writing usually, but for simplicity let's assume
    // files are written sequentially or pre-allocate small chunks.
    // Actually, simply giving each file a 4KB chunk is safer for now.
    ram_data_ptr += 4096; 

    fs_node_t* node = alloc_node();
    if (!node) return 0;
    
    strcpy(node->name, name);
    node->flags = flags;
    node->length = 0;
    node->impl = (uintptr_t)rf;
    node->write = ram_write;
    node->read = ram_read;
    
    return node;
}

fs_node_t* root_mkdir(fs_node_t *node, char *name) {
    return root_create_entry(node, name, FS_DIRECTORY);
}

fs_node_t* root_create(fs_node_t *node, char *name) {
    return root_create_entry(node, name, FS_FILE);
}

// --- TAR Driver Implementation ---
uintptr_t tar_start_address;

// Forward decl
uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    tar_header_t* header = (tar_header_t*)node->impl;
    uint32_t file_size = parse_octal(header->size, 11);

    if (offset >= file_size) return 0;
    if (offset + size > file_size) size = file_size - offset;

    // Data is directly after the 512 byte header
    uint8_t* bin = (uint8_t*)(node->impl + 512 + offset);
    for(uint32_t i=0; i<size; i++) {
        buffer[i] = bin[i];
    }
    return size;
}

static dirent_t static_dirent;

dirent_t *tar_readdir(fs_node_t *node __attribute__((unused)), uint32_t index) {
    // 1. Check RAM files first
    if (index < (uint32_t)ram_file_count) {
        strcpy(static_dirent.name, ram_files[index].name);
        static_dirent.ino = index;
        static_dirent.flags = ram_files[index].flags;
        return &static_dirent;
    }
    
    // 2. Adjust index for TAR files
    index -= ram_file_count;

    uintptr_t addr = tar_start_address;
    uint32_t i = 0;
    while(1) {
        tar_header_t* header = (tar_header_t*)addr;
        if (header->name[0] == '\0') break;

        if (i == index) {
            const char* fname = header->name;
            if (fname[0] == '.' && fname[1] == '/') fname += 2;
            strcpy(static_dirent.name, fname);
            static_dirent.ino = i;
            if (header->typeflag == '5') {
                static_dirent.flags = FS_DIRECTORY;
            } else {
                static_dirent.flags = FS_FILE;
            }
            return &static_dirent;
        }

        unsigned int size = parse_octal(header->size, 11);
        addr += 512 + ((size + 511) & ~511);
        i++;
    }
    return 0;
}

fs_node_t *tar_finddir(fs_node_t *node __attribute__((unused)), char *name) {
    // 1. Search RAM files
    for(int i = 0; i < ram_file_count; i++) {
        if (strcmp(ram_files[i].name, name) == 0) {
            fs_node_t* node = alloc_node();
            strcpy(node->name, ram_files[i].name);
            node->flags = ram_files[i].flags;
            node->length = ram_files[i].size;
            node->impl = (uintptr_t)&ram_files[i];
            node->read = ram_read;
            node->write = ram_write;
            return node;
        }
    }

    // In our flat TAR FS, we ignore the directory node passed in
    // and just search the linear archive.
    if (tar_start_address == 0) return 0;
    uintptr_t addr = tar_start_address;
    for (int limit = 0; limit < 512; limit++) { // Safety limit: max 512 files
        tar_header_t* header = (tar_header_t*)addr;
        if (header->name[0] == '\0') break; // End of archive

        // Compare filenames (TAR includes "./" prefix usually)
        // If the file is "./hello.txt", header->name+2 is "hello.txt"
        const char* fname = header->name;
        if (fname[0] == '.' && fname[1] == '/') fname += 2;

        if (strcmp(fname, name) == 0) {
            fs_node_t *file_node = alloc_node();
            file_node->flags = FS_FILE;
            file_node->impl = addr; // Store address of header
            file_node->length = parse_octal(header->size, 11);
            file_node->read = tar_read;
            file_node->readdir = 0; // Files don't have readdir
            return file_node;
        }

        unsigned int size = parse_octal(header->size, 11);
        addr += 512 + ((size + 511) & ~511);
    }
    return 0;
}

void init_fs(uintptr_t initrd_location) {
    tar_start_address = initrd_location;
    fs_root = alloc_node();
    fs_root->flags = FS_DIRECTORY;
    fs_root->finddir = tar_finddir;
    fs_root->readdir = tar_readdir;
    fs_root->mkdir = root_mkdir;
    fs_root->create = root_create;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node->read) return node->read(node, offset, size, buffer);
    return 0;
}
fs_node_t *finddir_fs(fs_node_t *node, char *name) {
    if (node->finddir) return node->finddir(node, name);
    return 0;
}
dirent_t *readdir_fs(fs_node_t *node, uint32_t index) {
    if ((node->flags & FS_DIRECTORY) && node->readdir) return node->readdir(node, index);
    return 0;
}

uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    // Security Check: Block writes to system-flagged nodes
    if (node->flags & FS_SYSTEM) {
        return 0; 
    }

    if (node->write) return node->write(node, offset, size, buffer);
    return 0;
}
void open_fs(fs_node_t *node) {
    if (node->open) node->open(node);
}
void close_fs(fs_node_t *node) {
    if (node->close) node->close(node);
}
fs_node_t *mkdir_fs(fs_node_t *node, char *name) {
    if (node->mkdir) return node->mkdir(node, name);
    return 0;
}
fs_node_t *create_fs(fs_node_t *node, char *name) {
    if (node->create) return node->create(node, name);
    return 0;
}