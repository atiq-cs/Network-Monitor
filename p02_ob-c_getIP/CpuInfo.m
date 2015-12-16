//
//  CpuInfo.m
//  p02_ob-c_getIP
//
//  Created by Milan Mia on 12/12/15.
//  Copyright © 2015 Social Network Team. All rights reserved.
//

#import "CpuInfo.h"

@implementation CpuInfo

typedef struct kinfo_proc kinfo_proc;
- (NSDictionary *)netStatsForInterval:(NSTimeInterval)sampleInterval {
    // Get sizing info from sysctl and resize as needed.
    int	mib[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
    size_t currentSize = 0;
    if (sysctl(mib, 6, NULL, &currentSize, NULL, 0) != 0) return nil;
    if (!sysctlBuffer || (currentSize > sysctlBufferSize)) {
        if (sysctlBuffer) free(sysctlBuffer);
        sysctlBufferSize = 0;
        sysctlBuffer = malloc(currentSize);
        if (!sysctlBuffer) return nil;
        sysctlBufferSize = currentSize;
    }
    
    // Read in new data
    if (sysctl(mib, 6, sysctlBuffer, &currentSize, NULL, 0) != 0) return nil;
    
    // Walk through the reply
    uint8_t *currentData = sysctlBuffer;
    uint8_t *currentDataEnd = sysctlBuffer + currentSize;
    NSMutableDictionary	*newStats = [NSMutableDictionary dictionary];
    while (currentData < currentDataEnd) {
        // Expecting interface data
        struct if_msghdr *ifmsg = (struct if_msghdr *)currentData;
        if (ifmsg->ifm_type != RTM_IFINFO) {
            currentData += ifmsg->ifm_msglen;
            continue;
        }
        // Must not be loopback
        if (ifmsg->ifm_flags & IFF_LOOPBACK) {
            currentData += ifmsg->ifm_msglen;
            continue;
        }
        // Only look at link layer items
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)(ifmsg + 1);
        if (sdl->sdl_family != AF_LINK) {
            currentData += ifmsg->ifm_msglen;
            continue;
        }
        // Build the interface name to string so we can key off it
        // (using NSData here because initWithBytes is 10.3 and later)
        NSString *interfaceName = [[NSString alloc]
                                    initWithData:[NSData dataWithBytes:sdl->sdl_data length:sdl->sdl_nlen]
                                    encoding:NSASCIIStringEncoding];
        if (!interfaceName) {
            currentData += ifmsg->ifm_msglen;
            continue;
        }
        // Load in old statistics for this interface
        NSDictionary *oldStats = [lastData objectForKey:interfaceName];
        
            // Not a PPP connection
            if (oldStats && (ifmsg->ifm_flags & IFF_UP)) {
                // Non-PPP data is sized at u_long, which means we need to deal
                // with 32-bit and 64-bit differently
                uint64_t lastTotalIn = [[oldStats objectForKey:@"totalin"] unsignedLongLongValue];
                uint64_t lastTotalOut = [[oldStats objectForKey:@"totalout"] unsignedLongLongValue];
                // New totals
                uint64_t totalIn = 0, totalOut = 0;
                // Values are always 32 bit and can overflow
                uint32_t lastifIn = [[oldStats objectForKey:@"ifin"] unsignedIntValue];
                uint32_t lastifOut = [[oldStats objectForKey:@"ifout"] unsignedIntValue];
                if (lastifIn > ifmsg->ifm_data.ifi_ibytes) {
                    totalIn = lastTotalIn + ifmsg->ifm_data.ifi_ibytes + UINT_MAX - lastifIn + 1;
                } else {
                    totalIn = lastTotalIn + (ifmsg->ifm_data.ifi_ibytes - lastifIn);
                }
                if (lastifOut > ifmsg->ifm_data.ifi_obytes) {
                    totalOut = lastTotalOut + ifmsg->ifm_data.ifi_obytes + UINT_MAX - lastifOut + 1;
                } else {
                    totalOut = lastTotalOut + (ifmsg->ifm_data.ifi_obytes - lastifOut);
                }
                // New deltas (64-bit overflow guard, full paranoia)
                uint64_t deltaIn = (totalIn > lastTotalIn) ? (totalIn - lastTotalIn) : 0;
                uint64_t deltaOut = (totalOut > lastTotalOut) ? (totalOut - lastTotalOut) : 0;
                // Peak
                double peak = [[oldStats objectForKey:@"peak"] doubleValue];
                if (sampleInterval > 0) {
                    if (peak < (deltaIn / sampleInterval)) peak = deltaIn / sampleInterval;
                    if (peak < (deltaOut / sampleInterval)) peak = deltaOut / sampleInterval;
                }
                [newStats setObject:[NSDictionary dictionaryWithObjectsAndKeys:
                                     [NSNumber numberWithUnsignedInt:ifmsg->ifm_data.ifi_ibytes],
                                     @"ifin",
                                     [NSNumber numberWithUnsignedInt:ifmsg->ifm_data.ifi_obytes],
                                     @"ifout",
                                     [NSNumber numberWithUnsignedLongLong:deltaIn],
                                     @"deltain",
                                     [NSNumber numberWithUnsignedLongLong:deltaOut],
                                     @"deltaout",
                                     [NSNumber numberWithUnsignedLongLong:totalIn],
                                     @"totalin",
                                     [NSNumber numberWithUnsignedLongLong:totalOut],
                                     @"totalout",
                                     [NSNumber numberWithDouble:peak],
                                     @"peak",
                                     nil]
                             forKey:interfaceName];
            } else {
                [newStats setObject:[NSDictionary dictionaryWithObjectsAndKeys:
                                     // Paranoia, is this where the neg numbers came from?
                                     [NSNumber numberWithUnsignedInt:ifmsg->ifm_data.ifi_ibytes],
                                     @"ifin",
                                     [NSNumber numberWithUnsignedInt:ifmsg->ifm_data.ifi_obytes],
                                     @"ifout",
                                     [NSNumber numberWithUnsignedLongLong:ifmsg->ifm_data.ifi_ibytes],
                                     @"totalin",
                                     [NSNumber numberWithUnsignedLongLong:ifmsg->ifm_data.ifi_obytes],
                                     @"totalout",
                                     [NSNumber numberWithDouble:0],
                                     @"peak",
                                     nil]
                             forKey:interfaceName];
        
        // Continue on
        currentData += ifmsg->ifm_msglen;
    }
    }
    return newStats;
} // netStatsForInterval

static int GetBSDProcessList(kinfo_proc **procList, size_t *procCount)
// Returns a list of all BSD processes on the system.  This routine
// allocates the list and puts it in *procList and a count of the
// number of entries in *procCount.  You are responsible for freeing
// this list (use "free" from System framework).
// On success, the function returns 0.
// On error, the function returns a BSD errno value.
{
    int                 err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;
    
    //    assert( procList != NULL);
    //    assert(*procList == NULL);
    //    assert(procCount != NULL);
    
    *procCount = 0;
    
    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.
    
    result = NULL;
    done = false;
    do {
        assert(result == NULL);
        
        // Call sysctl with a NULL buffer.
        
        length = 0;
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                     NULL, &length,
                     NULL, 0);
        if (err == -1) {
            err = errno;
        }
        
        // Allocate an appropriately sized buffer based on the results
        // from the previous call.
        
        if (err == 0) {
            result = malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }
        
        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.
        
        if (err == 0) {
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                         result, &length,
                         NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && ! done);
    
    // Clean up and establish post conditions.
    
    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }
    *procList = result;
    if (err == 0) {
        *procCount = length / sizeof(kinfo_proc);
    }
    
    assert( (err == 0) == (*procList != NULL) );
    
    return err;
}

- (NSArray*)getBSDProcessList
{
    kinfo_proc *mylist =NULL;
    size_t mycount = 0;
    GetBSDProcessList(&mylist, &mycount);
    
    NSMutableArray *processes = [NSMutableArray arrayWithCapacity:(int)mycount];
    
    for (int i = 0; i < mycount; i++) {
        struct kinfo_proc *currentProcess = &mylist[i];
        struct passwd *user = getpwuid(currentProcess->kp_eproc.e_ucred.cr_uid);
        NSMutableDictionary *entry = [NSMutableDictionary dictionaryWithCapacity:4];
        
        NSNumber *processID = [NSNumber numberWithInt:currentProcess->kp_proc.p_pid];
        NSString *processName = [NSString stringWithFormat: @"%s",currentProcess->kp_proc.p_comm];
        if (processID)[entry setObject:processID forKey:@"processID"];
        if (processName)[entry setObject:processName forKey:@"processName"];
        
        if (user){
            NSNumber *userID = [NSNumber numberWithUnsignedInt:currentProcess->kp_eproc.e_ucred.cr_uid];
            NSString *userName = [NSString stringWithFormat: @"%s",user->pw_name];
            
            if (userID)[entry setObject:userID forKey:@"userID"];
            if (userName)[entry setObject:userName forKey:@"userName"];
        }
        
        [processes addObject:[NSDictionary dictionaryWithDictionary:entry]];
    }
    free(mylist);
    
    return [NSArray arrayWithArray:processes];
}
- (void) getNetworkUses {
    int mib[] = {
        CTL_NET,
        PF_ROUTE,
        0,
        0,
        NET_RT_IFLIST2,
        0
    };
    size_t len;
    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
        fprintf(stderr, "sysctl: %s\n", strerror(errno));
        exit(1);
    }
    char *buf = (char *)malloc(len);
    if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
        fprintf(stderr, "sysctl: %s\n", strerror(errno));
        exit(1);
    }
    char *lim = buf + len;
    char *next = NULL;
    u_int64_t totalipackets = 0;
    u_int64_t totalopackets = 0;
    u_int64_t totalibytes = 0;
    u_int64_t totalobytes = 0;
    for (next = buf; next < lim; ) {
        struct if_msghdr *ifm = (struct if_msghdr *)next;
        next += ifm->ifm_msglen;
        if (ifm->ifm_type == RTM_IFINFO2) {
            struct if_msghdr2 *if2m = (struct if_msghdr2 *)ifm;
            //            totalibytes += if2m->ifm_data.ifi_ibytes;
            //            totalobytes += if2m->ifm_data.ifi_obytes;
            totalipackets += if2m->ifm_data.ifi_ipackets;
            totalopackets += if2m->ifm_data.ifi_opackets;
            totalibytes += if2m->ifm_data.ifi_ibytes;
            totalobytes += if2m->ifm_data.ifi_obytes;
            
        }
    }
    printf("\nTotal iPackets %qu\toPackets %qu", totalipackets, totalopackets);
    printf("\nTotal iBytes %qu\toBytes %qu\n", totalibytes, totalobytes);
}

- (void)applicationDidFinishLaunching
{
    int mib[2U] = { CTL_HW, HW_NCPU };
    size_t sizeOfNumCPUs = sizeof(numCPUs);
    int status = sysctl(mib, 2U, &numCPUs, &sizeOfNumCPUs, NULL, 0U);
    if(status)
        numCPUs = 1;
    
    CPUUsageLock = [[NSLock alloc] init];
    updateTimer = [NSTimer scheduledTimerWithTimeInterval:3
                                                    target:self
                                                  selector:@selector(updateInfo:)
                                                  userInfo:nil
                                                   repeats:YES];

    NSDate *timeout = [NSDate dateWithTimeIntervalSinceNow:60];
    while ([timeout timeIntervalSinceNow]>0) {
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:1]];
    }
}

- (void)updateInfo:(NSTimer *)timer
{
    //Show network Info
    [self getNetworkUses];
    NSLog(@"%@", [self netStatsForInterval:0.1]);
    natural_t numCPUsU = 0U;
    kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);
    if(err == KERN_SUCCESS) {
        [CPUUsageLock lock];
        
        for(unsigned i = 0U; i < numCPUs; ++i) {
            float inUse, total;
            if(prevCpuInfo) {
                inUse = (
                         (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER]   - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER])
                         + (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM])
                         + (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE]   - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE])
                         );
                total = inUse + (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE]);
            } else {
                inUse = cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE];
                total = inUse + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE];
            }
            
            NSLog(@"Core: %u Usage: %f", i, inUse / total);
        }
        [CPUUsageLock unlock];
        
        if(prevCpuInfo) {
            size_t prevCpuInfoSize = sizeof(integer_t) * numPrevCpuInfo;
            vm_deallocate(mach_task_self(), (vm_address_t)prevCpuInfo, prevCpuInfoSize);
        }
        
        prevCpuInfo = cpuInfo;
        numPrevCpuInfo = numCpuInfo;
        
        cpuInfo = NULL;
        numCpuInfo = 0U;
    } else {
        NSLog(@"Error!");
    }
}

@end
