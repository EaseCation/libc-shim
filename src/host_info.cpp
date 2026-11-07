#include "no-fortify.h"
#include "host_info.h"
#include "common.h"
#include "errno.h"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/utsname.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

int shim::uname(bionic::utsname *buf) {
    ::utsname host {};
    if(::uname(&host) != 0)
        return -1;
    // 按字段截断复制，保证只写入调用方的 390 字节。
    std::memset(buf, 0, sizeof(*buf));
    bionic::strlcpy(buf->sysname, host.sysname, sizeof(buf->sysname));
    bionic::strlcpy(buf->nodename, host.nodename, sizeof(buf->nodename));
    bionic::strlcpy(buf->release, host.release, sizeof(buf->release));
    bionic::strlcpy(buf->version, host.version, sizeof(buf->version));
    bionic::strlcpy(buf->machine, host.machine, sizeof(buf->machine));
#if defined(__linux__) && defined(_GNU_SOURCE)
    bionic::strlcpy(buf->domainname, host.domainname, sizeof(buf->domainname));
#endif
    return 0;
}

int shim::sysinfo(bionic::sysinfo *info) {
    if(!info) {
        errno = EFAULT;
        return -1;
    }
    std::memset(info, 0, sizeof(*info));
#ifdef __linux__
    struct ::sysinfo host {};
    if(::sysinfo(&host) != 0)
        return -1;
    info->uptime = host.uptime;
    for(int i = 0; i < 3; ++i)
        info->loads[i] = host.loads[i];
    info->totalram = host.totalram;
    info->freeram = host.freeram;
    info->sharedram = host.sharedram;
    info->bufferram = host.bufferram;
    info->totalswap = host.totalswap;
    info->freeswap = host.freeswap;
    info->procs = host.procs;
    info->totalhigh = host.totalhigh;
    info->freehigh = host.freehigh;
    info->mem_unit = host.mem_unit;
    return 0;
#elif defined(__APPLE__)
    // 字段取自宿主内核；无法获得的值保持 0，mem_unit 固定为 1 字节。
    uint64_t memory = 0;
    size_t size = sizeof(memory);
    if(::sysctlbyname("hw.memsize", &memory, &size, nullptr, 0) != 0)
        return -1;
    info->totalram = (unsigned long)memory;
    info->mem_unit = 1;
    ::timeval boot {};
    size = sizeof(boot);
    int bootMib[2] = {CTL_KERN, KERN_BOOTTIME};
    if(::sysctl(bootMib, 2, &boot, &size, nullptr, 0) == 0 && boot.tv_sec > 0)
        info->uptime = (long)(::time(nullptr) - boot.tv_sec);
    double loads[3];
    if(::getloadavg(loads, 3) == 3) {
        for(int i = 0; i < 3; ++i)
            info->loads[i] = (unsigned long)(loads[i] * 65536.0); // SI_LOAD_SHIFT
    }
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm {};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if(host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS)
        info->freeram = (unsigned long)((uint64_t)vm.free_count * vm_kernel_page_size);
    mach_port_deallocate(mach_task_self(), host);
    xsw_usage swap {};
    size = sizeof(swap);
    if(::sysctlbyname("vm.swapusage", &swap, &size, nullptr, 0) == 0) {
        info->totalswap = (unsigned long)swap.xsu_total;
        info->freeswap = (unsigned long)swap.xsu_avail;
    }
    int processes = ::proc_listallpids(nullptr, 0);
    if(processes > 0)
        info->procs = (unsigned short)(processes > 0xffff ? 0xffff : processes);
    return 0;
#else
    errno = ENOSYS;
    return -1;
#endif
}

void shim::add_host_info_shimmed_symbols(std::vector<shim::shimmed_symbol> &list) {
    list.insert(list.end(), {
        {"uname", WithErrnoUpdate(shim::uname)},
        {"sysinfo", WithErrnoUpdate(shim::sysinfo)},
        // char* 返回值与 Bionic ABI 一致，可直接使用宿主实现。
        {"getlogin", WithErrnoUpdate(::getlogin)},
    });
}
