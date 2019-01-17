#include <stdio.h>
#include <signal.h>
#include "../head/FileSystem.h"


int main() {
    printf("正在打开文件系统，请稍候...\n");
    signal(SIGINT, stopHandle); //捕获中断信号，如 ctrl-C，通常由用户生成。
    openFileSystem();
    command();
    return 0;
}