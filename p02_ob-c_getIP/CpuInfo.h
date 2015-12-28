//
//  CpuInfo.h
//  p02_ob-c_getIP
//
//  Copyright © 2015 Social Network Team. All rights reserved.
//
// reference manual: https://developer.apple.com/library/ios/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sysctl.3.html
// Where is the header file guard??
//  Same header file might be included multiple times!!

#import <Foundation/Foundation.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
//Uses related
#include <netinet/in.h>
#include <net/route.h>
//System Network info related
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
//System memory related
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>

#include <pwd.h>
//menumeters
#import <sys/types.h>
#import <net/if_dl.h>
#import <net/if_var.h>
#import <net/route.h>
#import <limits.h>

@interface CpuInfo : NSObject {
    processor_info_array_t cpuInfo, prevCpuInfo;
    mach_msg_type_number_t numCpuInfo, numPrevCpuInfo;
    unsigned numCPUs;
    NSTimer *updateTimer;
    NSLock *CPUUsageLock;
    
    // Old data for containing prior reads
    NSMutableDictionary		*lastData;
    // Buffer we keep around
    size_t					sysctlBufferSize;
    uint8_t					*sysctlBuffer;
}

- (void)applicationDidFinishLaunching;
- (void)updateInfo:(NSTimer *)timer;
- (void)writeToFile:(NSString*)string;
@end
