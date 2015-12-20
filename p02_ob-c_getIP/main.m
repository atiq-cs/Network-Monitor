//
//  main.m
//  p02_ob-c_getIP
//
//  Created by Atiq Rahman on 10/9/15.
//  Copyright (c) 2015
//
// Reference link on Networking: http://stackoverflow.com/questions/1126790/how-to-get-network-adapter-stats-in-linux-mac-osx
// TODO:
//      1. Get Disk Info as well
//      2. Get all info every few seconds
//      3. May be find some ways to terminate the program

#import <Foundation/Foundation.h>
#import "CpuInfo.h"
@import AppKit;

static unsigned long long _previousTotalTicks = 0;
static unsigned long long _previousIdleTicks = 0;

// Returns 1.0f for "CPU fully pinned", 0.0f for "CPU idle", or somewhere in between
// You'll need to call this at regular intervals, since it measures the load between
// the previous call and the current one.
float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks) {
    unsigned long long totalTicksSinceLastTime = totalTicks-_previousTotalTicks;
    unsigned long long idleTicksSinceLastTime  = idleTicks-_previousIdleTicks;
    float ret = 1.0f-((totalTicksSinceLastTime > 0) ? ((float)idleTicksSinceLastTime)/totalTicksSinceLastTime : 0);
    _previousTotalTicks = totalTicks;
    _previousIdleTicks  = idleTicks;
    return ret;
}

float getCPULoad() {
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
        unsigned long long totalTicks = 0;
        for(int i=0; i<CPU_STATE_MAX; i++) totalTicks += cpuinfo.cpu_ticks[i];
        float sysLoadPercentage = CalculateCPULoad(cpuinfo.cpu_ticks[CPU_STATE_IDLE], totalTicks);
        printf("sysLoadPercentage: %f\n", sysLoadPercentage);
        return 1;
    }
    else return -1.0f;
}

void getRamUses() {
    vm_size_t page_size;
    mach_port_t mach_port;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;
    
    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                          (host_info64_t)&vm_stats, &count)) {
            long long free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
            
            long long used_memory = ((int64_t)vm_stats.active_count +
                                     (int64_t)vm_stats.inactive_count +
                                     (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
            printf("free memory: %lld\nused memory: %lld\n", free_memory, used_memory);
        }
}

void getNetworkInfo() {
    NSLog(@"\nMACOSX - System Info Extraction Program!\n");
    struct ifaddrs *allInterfaces;
    
    // Get list of all interfaces on the local machine:
    if (getifaddrs(&allInterfaces) == 0) {
        struct ifaddrs *interface;
        
        // For each interface ...
        for (interface = allInterfaces; interface != NULL; interface = interface->ifa_next) {
            unsigned int flags = interface->ifa_flags;
            struct sockaddr *addr = interface->ifa_addr;
            
            // Check for running IPv4, IPv6 interfaces. Skip the loopback interface.
            if ((flags & (IFF_UP|IFF_RUNNING|IFF_LOOPBACK)) == (IFF_UP|IFF_RUNNING)) {
                char host[NI_MAXHOST];
                if (addr->sa_family == AF_INET || addr->sa_family == AF_INET6) {
                    printf("Interface name: %s\n", interface->ifa_name);
                    getnameinfo(addr, addr->sa_len, host, sizeof(host), NULL, 0, NI_NUMERICHOST);
                }
                /* Probable inactive devices, uncomment to list all
                else
                    printf("Interface name: %s\n", interface->ifa_name); */
                // AF_INET - ipv4
                // AF_INET - ipv6
                // if (addr->sa_family == AF_INET) {
                
                // For the same interface it is showing IP info
                // Convert interface address to a human readable string:
                if (addr->sa_family == AF_INET)
                    printf("IPv4 address:%s\n", host);
                else if (addr->sa_family == AF_INET6)
                    printf("IPv6 address:%s\n", host);
            }
        }
        freeifaddrs(allInterfaces);
    }
}
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        //http://stackoverflow.com/questions/3545102/how-to-enumerate-volumes-on-mac-os-x
        NSWorkspace   *ws = [NSWorkspace sharedWorkspace];
        NSArray     *vols = [ws mountedLocalVolumePaths];
        NSFileManager *fm = [NSFileManager defaultManager];
        
        for (NSString *path in vols)
        {
            NSDictionary* fsAttributes;
            NSString *description, *type, *name;
            BOOL removable, writable, unmountable, res;
            NSNumber *size;
            double freeSpaceMB = -1.;
            res = [ws getFileSystemInfoForPath:path
                                   isRemovable:&removable
                                    isWritable:&writable
                                 isUnmountable:&unmountable
                                   description:&description
                                          type:&type];
            if (!res) continue;
            fsAttributes = [fm fileSystemAttributesAtPath:path];
            name         = [fm displayNameAtPath:path];
            size         = [fsAttributes objectForKey:NSFileSystemSize];
            NSNumber *freeSpace = [fsAttributes objectForKey:NSFileSystemFreeSize];
            freeSpaceMB = (freeSpace.doubleValue * 1.0)/ (1024 * 1024);
            NSLog(@"path=%@\nname=%@\nremovable=%d\nwritable=%d\nunmountable=%d\n"
                  "description=%@\ntype=%@, size=%@\nfree Space=%lf MB\n",
                  path, name, removable, writable, unmountable, description, type, size, freeSpaceMB);
        }
        //int pid = [[NSProcessInfo processInfo] processIdentifier];
        NSPipe *pipe = [NSPipe pipe];
        NSFileHandle *file = pipe.fileHandleForReading;
        
        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/usr/bin/top";
        task.arguments = @[@"-l1"];
        task.standardOutput = pipe;
        
        [task launch];
        
        NSData *data = [file readDataToEndOfFile];
        [file closeFile];
        
        NSString *grepOutput = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
        NSLog (@"Top returned:\n%@", grepOutput);
        getNetworkInfo();
        getCPULoad();
        getRamUses();
        CpuInfo *cpuInfo = [[CpuInfo alloc]init];
        //print List of running processes
        NSLog(@"%@",[cpuInfo getBSDProcessList]);
        [cpuInfo applicationDidFinishLaunching];
    }
    return 0;
}
