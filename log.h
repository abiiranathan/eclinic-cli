#ifndef E8D7CD4F_5A96_4A8C_934F_46D4F21C2CE5
#define E8D7CD4F_5A96_4A8C_934F_46D4F21C2CE5
#include <stdio.h>
#include <stdlib.h>

extern void cleanup();

#define LOG_ERROR(fmt, ...)                                                                        \
    fprintf(stderr, "[ERROR] %s:%d:%s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...)                                                                        \
    do {                                                                                           \
        LOG_ERROR(fmt, ##__VA_ARGS__);                                                             \
        cleanup();                                                                                 \
        exit(EXIT_FAILURE);                                                                        \
    } while (0)

#endif /* E8D7CD4F_5A96_4A8C_934F_46D4F21C2CE5 */
