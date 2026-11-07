#pragma once

#include <libc_shim.h>

namespace shim {

    namespace bionic {

        // Bionic/Linux 内核布局：6 个 65 字节字段。Darwin 为 5 个 256 字节字段，不能直接透传。
        struct utsname {
            char sysname[65];
            char nodename[65];
            char release[65];
            char version[65];
            char machine[65];
            char domainname[65];
        };
        static_assert(sizeof(utsname) == 390, "bionic utsname layout");

        // Bionic/Linux 内核 struct sysinfo；内存字段以 mem_unit 为单位。
        struct sysinfo {
            long uptime;
            unsigned long loads[3];
            unsigned long totalram;
            unsigned long freeram;
            unsigned long sharedram;
            unsigned long bufferram;
            unsigned long totalswap;
            unsigned long freeswap;
            unsigned short procs;
            unsigned short pad;
            unsigned long totalhigh;
            unsigned long freehigh;
            unsigned int mem_unit;
#if !defined(__LP64__)
            char _f[20 - 2 * sizeof(long) - sizeof(int)];
#endif
        };
        static_assert(sizeof(sysinfo) == (sizeof(long) == 8 ? 112 : 64), "bionic sysinfo layout");

    }

    int uname(bionic::utsname *buf);

    int sysinfo(bionic::sysinfo *info);

    void add_host_info_shimmed_symbols(std::vector<shimmed_symbol> &list);

}
