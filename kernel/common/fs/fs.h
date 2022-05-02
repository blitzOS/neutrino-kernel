#pragma once
#include <stdint.h>
#include <size_t.h>

#define MAX_PATH 128
#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

typedef uintptr_t (*FsNodeRead)(struct __fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uintptr_t (*FsNodeWrite)(struct __fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*FsNodeOpen)(struct __fs_node*);
typedef void (*FsNodeClose)(struct __fs_node*);
typedef struct __dirent* (*FsNodeReadDir)(struct __fs_node*, uint32_t);
typedef struct __fs_node* (*FsNodeFindDir)(struct __fs_node*, char*);

struct __dirent {
    char name[MAX_PATH];
    uint32_t ino;       // Inode number
}

typedef struct __fs_node {
    char name[MAX_PATH];
    
    uint64_t flags;
    uint32_t perm_mask;
    uint32_t user_id;
    uint32_t group_id;
    size_t length;

    uint32_t inode;
    uint32_t impl;

    FsNodeRead read;   
    FsNodeWrite write;
    FsNodeOpen open;
    FsNodeClose close;
    FsNodeReadDir readdir; //? Returns the n'th child of a directory
    FsNodeFindDir finddir; //? Try to find a child in a directory by name

    struct __fs_node* ptr;
} FsNode;

#define IsDirectory(node) ((node->flags & 0x7) == FS_DIRECTORY)

extern FsNode* root;

uintptr_t fs_read(FsNode* node, uint32_t offset, uint32_t size, uint8_t* buf);
uintptr_t fs_write(FsNode* node, uint32_t offset, uint32_t size, uint8_t* buf);
void fs_open(FsNode* node, uint8_t read, uint8_t write);
void fs_close(FsNode* node);
struct __dirent* fs_readdir(FsNode* node, uint32_t index);
FsNode* fs_finddir(FsNode* node, char* name);
