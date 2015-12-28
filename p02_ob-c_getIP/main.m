//
//  main.m
//  p02_ob-c_getIP
//
//  Created by Atiq Rahman on 10/9/15.
//  Copyright (c) 2015
//
// To terminate from XCode press Command+.
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

NSString* getCPULoad() {
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
        unsigned long long totalTicks = 0;
        for(int i=0; i<CPU_STATE_MAX; i++) totalTicks += cpuinfo.cpu_ticks[i];
        float sysLoadPercentage = CalculateCPULoad(cpuinfo.cpu_ticks[CPU_STATE_IDLE], totalTicks);
        NSString *str = [NSString stringWithFormat:@"\nCPU Load Info\n=================\nCPU Usage: %f\n", sysLoadPercentage];
        return str;
    }
    else return @"-1";
}

NSString* getRamUses() {
    NSString *str;
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
            str = [NSString stringWithFormat:@"\nMain Memory Usage\n=================\nFree: %lld\nUsed: %lld\n", free_memory, used_memory];
    }
    return str;
}

NSString* getNetworkInfo() {
    struct ifaddrs *allInterfaces;
    NSString *str;
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
                    str = [NSString stringWithFormat:@"\nNetwork Info\n===========================\nInterface name: %s\n", interface->ifa_name];
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
                if (addr->sa_family == AF_INET){
                    NSString *temp = [NSString stringWithFormat:@"IPv4 address:%s\n", host];
                    str = [NSString stringWithFormat:@"%@%@", str, temp];
                }
                else if (addr->sa_family == AF_INET6){
                    NSString *temp = [NSString stringWithFormat:@"IPv6 address:%s\n", host];
                    str = [NSString stringWithFormat:@"%@%@", str, temp];
                }
            }
        }
        freeifaddrs(allInterfaces);
    }
    return str;
}

/**
 * Entry point of the application
 */
int main(int argc, const char * argv[]) {
    // automatically release allocated items
    @autoreleasepool {
        NSLog(@"\nMACOSX - System Info Extraction Program\nWriting log file..\n");
        CpuInfo *cpuInfo = [[CpuInfo alloc]init];
        
        /**
         * This is one of the ways to show volume space info. Here is a ref:
         * http://stackoverflow.com/questions/3545102/how-to-enumerate-volumes-on-mac-os-x
         * mount point info query starts
         */
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
            NSString *str = [NSString stringWithFormat:@"\npath=%@\nname=%@\nremovable=%d\nwritable=%d\nunmountable=%d\n""description=%@\ntype=%@, size=%@\nfree Space=%lf MB\n\n",path, name, removable, writable, unmountable, description, type, size, freeSpaceMB];
            [cpuInfo writeToFile:str];
        }
        // mount point info query ends here!

        // read from top starts here
        NSPipe *pipe = [NSPipe pipe];
        NSFileHandle *newFile = pipe.fileHandleForReading;
        
        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/usr/bin/top";
        task.arguments = @[@"-l1"];
        task.standardOutput = pipe;
        
        [task launch];
        NSData *data = [newFile readDataToEndOfFile];
        [newFile closeFile];
        // read from top ends here
        
        /**
         * Get more info and write those info to file
         *  1. Network info
         *  2. CPU Load
         *  3. RAM Use
         */
        NSString *grepOutput = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
        [cpuInfo writeToFile:grepOutput];
        [cpuInfo writeToFile:getNetworkInfo()];
        [cpuInfo writeToFile:getCPULoad()];
        [cpuInfo writeToFile:getRamUses()];

        [cpuInfo writeToFile:[NSString stringWithFormat:@"\nContinuous Network Traffic and CPU Load Info\n=============================================\n"]];
        [cpuInfo applicationDidFinishLaunching];
        NSLog(@"Extraction complete.\n");
    }
    return 0;
}
