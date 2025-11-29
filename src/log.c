#include "log.h"

void print_current_mem(const char* tag)
{
    long rss_mb = 0;
    long vm_mb = 0;

#if defined(__linux__)
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        long size, resident;
        if (fscanf(fp, "%ld %ld", &size, &resident) == 2) {
            long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
            rss_mb = (resident * page_size_kb) / 1024;
            vm_mb = (size * page_size_kb) / 1024;
        }
        fclose(fp);
    }

#elif defined(__APPLE__)
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS) {
        rss_mb = t_info.resident_size / (1024 * 1024);
        vm_mb = t_info.virtual_size / (1024 * 1024);
    }
#endif

    printf("[MEM CHECK - %s] RSS (RAM fisica): %ld MB | VM: %ld MB\n", tag, rss_mb, vm_mb);
}
