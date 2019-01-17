//
// Created by Reborn on 2019/1/17.
//

#ifndef COS_DESIGN_FOR_CPP_FILESYSTEM_H
#define COS_DESIGN_FOR_CPP_FILESYSTEM_H

#endif //COS_DESIGN_FOR_CPP_FILESYSTEM_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef linux
#include <malloc.h>
#endif
#ifdef _UNIX
#include <malloc/malloc.h>
#endif

#define BLOCK_SIZE 1024
#define BLOCK_NUM 102400
#define INODE_NUM 102400
#define SYSTEM_NAME "FileSystem.img"

void createFileSystem();

void openFileSystem();

void createFile(char *name, int flag);

void list();

int findINodeId(char *name, int flag);

void cd(char *name);

void pathCurrent();

int analyseInput(char *str);

void command();

void updateResource();

void stopHandle(int sig);

/**
 * 文件系统块结构
*/
typedef struct {
    unsigned short blockSize;   // 2B，文件块大小
    unsigned int blockNum;      // 4B，文件块数量
    unsigned int iNodeNum;      // 4B，inode节点数量,即文件数量
    unsigned int blockFree;     // 4B，空闲块数量
} SuperBlock;

/**
 * 文件inode节点结构
 * 100Ｂ
*/
typedef struct {
    unsigned int id;        // 4B，inode节点索引
    char name[30];          // 30B，文件名，最大长度29
    unsigned char isDir;    // 1B，文件类型 0-file 1-dir
    unsigned int parent;    // 4B，父目录inode节点索引
    unsigned int length;    // 4B，文件长度，unsigned int最大2^32-1(4GB-1B)，目录文件则为子文件项数量，即if file->filesize  if dir->filenum
    unsigned int addr[12];  // 12*4B，文件内容索引，文件前10项为直接索引，目录前11项为直接索引
    unsigned int blockId;   // 文件项所在的目录数据块的id，便于删除时定位
} INode, *PtrINode;

/**
  文件部分信息节点，用于简要显示文件信息，主要用途：将一个目录下的所有文件(包括目录)写入到该目录对应的Block中
  文件名 文件／目录　文件长度　文件权限　修改时间
*/
typedef struct {
    unsigned int id;     //4B
    char name[30];       //30B
    unsigned char isDir; //1B
    unsigned int blockId; //4B,在目录数据块中的位置0-255
} Fcb, *PtrFcb;

char *blockBitmap; // 文件块使用图，块分配情况
char *iNodeBitmap; // iNode节点使用图，索引节点分配情况


unsigned short currentDir; //current iNodeId
SuperBlock superBlock;
FILE *fp;

char *fullPath; // 当前完整路径

// 模拟接收命令参数
int argc; // 表示命令的参数个数
char *argv[5]; // 表示命令的参数序列或指针，并且第一个参数argv[0]一定是程序的名称

// 大小
const unsigned short superBlockSize = sizeof(superBlock);
const unsigned short blockBitmapSize = sizeof(blockBitmap);
const unsigned short inodeBitmapSize = sizeof(iNodeBitmap);
const unsigned short inodeSize = sizeof(INode);
const unsigned short fcbSize = sizeof(Fcb);

/* 创建文件系统 */
void createFileSystem() {
    long len;
    PtrINode fileINode;
    if ((fp = fopen(SYSTEM_NAME, "wb+")) == NULL) {
        printf("open file %s error...\n", SYSTEM_NAME);
        exit(1);
    }

//    blockBitmap = malloc(BLOCK_NUM * sizeof(char));
//    iNodeBitmap = malloc(INODE_NUM * sizeof(char));

    // init bitmap
    for (len = 0; len < BLOCK_NUM; len++)
        blockBitmap[len] = 0;

    for (len = 0; len < INODE_NUM; len++)
        iNodeBitmap[len] = 0;

    // memory set
    for (len = 0; len < (superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODE_NUM +
                         BLOCK_SIZE * BLOCK_NUM); len++) {
        fputc(0, fp); // 往缓冲区填充0
    }
    rewind(fp);

    // init superBlock
    superBlock.blockNum = BLOCK_NUM;
    superBlock.blockSize = BLOCK_SIZE;
    superBlock.iNodeNum = INODE_NUM;
    superBlock.blockFree = BLOCK_NUM - 1;

    fwrite(&superBlock, superBlockSize, 1, fp); // 写入超级块信息到文件

    // create root
    fileINode = (PtrINode) malloc(inodeSize);
    fileINode->id = 0;
    strcpy(fileINode->name, "/");
    fileINode->isDir = 1;
    fileINode->parent = 0;
    fileINode->length = 0;
    fileINode->blockId = 0;

    // write "/" info to file
    iNodeBitmap[0] = 1;
    blockBitmap[0] = 1;
    fseek(fp, superBlockSize, SEEK_SET);
    fwrite(blockBitmap, blockBitmapSize, 1, fp);    // 写入文件块使用图信息到文件
    fseek(fp, superBlockSize + blockBitmapSize, SEEK_SET);
    fwrite(iNodeBitmap, inodeBitmapSize, 1, fp);    // 写入inode节点使用图信息到文件
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize, SEEK_SET);
    fwrite(fileINode, inodeSize, 1, fp);     // 写入inode节点信息到文件
    fflush(fp); // 刷新缓冲区，貌似只有Windows下有效？

//    free(blockBitmap);
//    free(iNodeBitmap);

    // point to currentDir
    currentDir = 0;
}

/*如果SYSTEM_NAME可读，则代表之前已有信息，并读取相应数据；如果不可读，则创建文件系统 */
void openFileSystem() {

    blockBitmap = malloc(BLOCK_NUM * sizeof(char));
    iNodeBitmap = malloc(INODE_NUM * sizeof(char));

    if ((fp = fopen(SYSTEM_NAME, "rb")) == NULL) {
        createFileSystem();
    } else {
        if ((fp = fopen(SYSTEM_NAME, "rb+")) == NULL) {
            printf("open file %s error...\n", SYSTEM_NAME);
            exit(1);
        }
        rewind(fp);
        //read superBlock from file
        fread(&superBlock, superBlockSize, 1, fp);

        //read bitmap from file
        fread(blockBitmap, blockBitmapSize, 1, fp);
        fread(iNodeBitmap, inodeBitmapSize, 1, fp);

        //init current dir
        currentDir = 0;
    }
}

// flag = 0 -> create file
//      = 1 -> create directory
void createFile(char *name, int flag) {
    int i, nowBlockNum = -1, nowInodeNum = -1;
    PtrINode fileINode = (PtrINode) malloc(inodeSize);
    PtrINode parentINode = (PtrINode) malloc(inodeSize);
    PtrFcb fcb = (PtrFcb) malloc(fcbSize);

    // the available blockNumber
    for (i = 0; i < BLOCK_NUM; i++) {
        if (blockBitmap[i] == 0) {
            nowBlockNum = i;
            break;
        }

    }

    // the available inodeNumber
    for (i = 0; i < INODE_NUM; i++) {
        if (iNodeBitmap[i] == 0) {
            nowInodeNum = i;
            break;
        }

    }

    // init fileINode struct
    fileINode->blockId = (unsigned int) nowBlockNum; // 分配可用的块号
    strcpy(fileINode->name, name);
    fileINode->id = (unsigned int) nowInodeNum; // 分配可用的结点号
    fileINode->parent = currentDir;
    if (flag == 0) {
        fileINode->isDir = 0;
    } else {
        fileINode->isDir = 1;
    }
    fileINode->length = 0;

    // write fileInfo to file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * nowInodeNum,
          SEEK_SET); // 文件指针回到头部，再偏移到对应结点位置
    fwrite(fileINode, inodeSize, 1, fp);

    // update superBlock and bitmap
    superBlock.blockFree -= 1;
    blockBitmap[nowBlockNum] = 1;
    iNodeBitmap[nowInodeNum] = 1;

    // init fcb info
    strcpy(fcb->name, fileINode->name);
    fcb->id = fileINode->id;
    fcb->isDir = fileINode->isDir;

    // update to file ...

    // update parent dir block info
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentINode, inodeSize, 1, fp); // 获取当前文件/文件夹的父目录
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + INODE_NUM * inodeSize +
              parentINode->blockId * BLOCK_SIZE + parentINode->length * fcbSize, SEEK_SET);
    fwrite(fcb, fcbSize, 1, fp);

    // update parent dir inode info
    parentINode->length += 1; // 文件数量加1
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fwrite(parentINode, inodeSize, 1, fp);

    // update map
    fseek(fp, superBlockSize, SEEK_SET);
    fwrite(blockBitmap, blockBitmapSize, 1, fp);    // 写入文件块使用图信息到文件
    fseek(fp, superBlockSize + blockBitmapSize, SEEK_SET);
    fwrite(iNodeBitmap, inodeBitmapSize, 1, fp);    // 写入inode节点使用图信息到文件

    // free resource
    free(fileINode);
    free(parentINode);
    free(fcb);
}

void list() {
    int i;
    PtrFcb fcb = (PtrFcb) malloc(fcbSize);
    PtrINode parentInode = (PtrINode) malloc(inodeSize);

    // read parent inode info from file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentInode, inodeSize, 1, fp);

    // point to parent dir block
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODE_NUM +
              parentInode->blockId * BLOCK_SIZE, SEEK_SET);

    // list info
    printf("Filename\tINodeNumber\t\tType\n");
    for (i = 0; i < parentInode->length; i++) {
        fread(fcb, fcbSize, 1, fp);
        printf("%-10s\t", fcb->name);
        printf("%-2d\t", fcb->id);
        if (fcb->isDir == 1) {
            printf("\t\t\tdir\n");
        } else {
            printf("\t\t\tfile\n");
        }
    }

    // free resource
    free(fcb);
    free(parentInode);
}

// flag = 0 -> find file
//      = 1 -> find dir
int findINodeId(char *name, int flag) {
    int i, fileINodeId = -1;
    PtrINode parentINode = (PtrINode) malloc(inodeSize);
    PtrFcb fcb = (PtrFcb) malloc(fcbSize);

    //read current inode info from file
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(parentINode, inodeSize, 1, fp);

    //read the fcb in the current dir block
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + inodeSize * INODE_NUM +
              parentINode->blockId * BLOCK_SIZE, SEEK_SET);

    for (i = 0; i < parentINode->length; i++) {
        fread(fcb, fcbSize, 1, fp);
        if (flag == 0) {
            if ((fcb->isDir == 0) && (strcmp(name, fcb->name) == 0)) {
                fileINodeId = fcb->id;
                break;
            }

        } else {
            if ((fcb->isDir == 1) && (strcmp(name, fcb->name) == 0)) {
                fileINodeId = fcb->id;
                break;
            }

        }
    }

    if (i == parentINode->length) // 若没找到，则返回-1，以便后面判断
        fileINodeId = -1;

    free(fcb);
    free(parentINode);
    return fileINodeId;
}

void cd(char *name) {
    int fileINodeId;
    PtrINode fileINode = (PtrINode) malloc(inodeSize);
    if (strcmp(name, "..") != 0) {
        fileINodeId = findINodeId(name, 1);
        if (fileINodeId == -1)
            printf("cd: no such file or directory: %s\n", name);
        else {
            currentDir = (unsigned short) fileINodeId;
        }
    } else {
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
        fread(fileINode, inodeSize, 1, fp);
        currentDir = (unsigned short) fileINode->parent;

    }
    free(fileINode);
}


char *stringJoin(char *s1, char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2) + 1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    if (result == NULL) exit(1);

    strcpy(result, s1);
    strcat(result, s2);

    return result;
}

void pathCurrent() {
    PtrINode curINode = (PtrINode) malloc(inodeSize);
    fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + currentDir * inodeSize, SEEK_SET);
    fread(curINode, inodeSize, 1, fp);
    printf("user@localhost:%s#", curINode->name);

    // 获取完整路径
    fullPath = malloc(sizeof(char));
    strcpy(fullPath, curINode->name); // 临时存当前路径
    while (curINode->id != 0) {
        fseek(fp, superBlockSize + blockBitmapSize + inodeBitmapSize + curINode->parent * inodeSize, SEEK_SET);
        fread(curINode, inodeSize, 1, fp);
        if (strcmp(curINode->name, "/") != 0) {
            fullPath = stringJoin( "/", fullPath);
            fullPath = stringJoin(curINode->name, fullPath);
        }
    }
    if (fullPath[0] != '/') {
        fullPath = stringJoin( "/", fullPath);
        fullPath = stringJoin(fullPath,  "/");
    }
//    printf("user@localhost:%s#", fullPath);

    free(curINode);
}

char* getFullPath(){
    if (fullPath == NULL) {
        return "读取当前路径错误！";
    } else {
        return fullPath;
    }
}

// 解析输入的命令
int analyseInput(char *str) {
    int i;
    char temp[20];
    char *ptr_char;
    char *syscmd[] = {"pwd", "ls", "cd", "mkdir", "touch", "exit"};
    argc = 0;
    for (i = 0, ptr_char = str; *ptr_char != '\0'; ptr_char++) {
        if (*ptr_char != ' ') {
            while (*ptr_char != ' ' && (*ptr_char != '\0'))
                temp[i++] = *ptr_char++;
            argv[argc] = (char *) malloc((size_t) (i + 1));
            strncpy(argv[argc], temp, i);   // 把temp所指向的字符串中以temp地址开始的前i个字节复制到argv所指的数组中，并返回被复制后的argv
            argv[argc][i] = '\0'; // 字符数组最后一个位置添加空字符
            argc++;
            i = 0;
            if (*ptr_char == '\0')
                break;
        }
    }
    if (argc != 0) {
        for (i = 0; (i < 6) && strcmp(argv[0], syscmd[i]); i++);
        return i;
    } else
        return 6;
    return 0;
}

void command() {
    char cmd[20];
    do {
        pathCurrent();
        gets(cmd);
        switch (analyseInput(cmd)) {
            case 0:
                printf("%s\n", getFullPath());
                break;
            case 1:
                list();
                break;
            case 2:
                cd(argv[1]);
                break;
            case 3:
                createFile(argv[1], 1); // 1->dir
                break;
            case 4:
                createFile(argv[1], 0); // 0->file
                break;
            case 5:
                updateResource();
                exit(0);
                break;
            default:
                break;
        }
    } while (1);
}


void updateResource() {
    rewind(fp);
    fwrite(&superBlock, superBlockSize, 1, fp);
    fwrite(blockBitmap, blockBitmapSize, 1, fp);
    fwrite(iNodeBitmap, inodeBitmapSize, 1, fp);
    free(blockBitmap);
    free(iNodeBitmap);
    fclose(fp);
}

/**
 * 捕获中断信号后的操作
 *
 * 由于用户随时可能中断，
 * 所以我们在中断之前必须先更新用户之前操作过的资源，
 * 如文件系统块结构SuperBlock的修改以及文件块和节点使用图
 * */
void stopHandle(int sig) {
    printf("\nPlease wait..., update resource\n");
    updateResource();
    exit(0);
}
