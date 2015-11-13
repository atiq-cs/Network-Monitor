//
//  main.m
//  p02_ob-c_getIP
//
//  Created by Atiq Rahman on 10/9/15.
//  Copyright (c) 2015
//

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>


int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
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
                        printf("Interface name: %s\n", interface->ifa_name);
                     */
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
    return 0;
}
