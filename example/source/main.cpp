// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"

// Include the main libnx system header, for Switch development
#include <switch.h>
#include <scsi/scsi_context.h>

Result search_and_open_endpoint(UsbHsClientIfSession *session, struct usb_endpoint_descriptor *descs, UsbHsClientEpSession *endpoint)
{
    Result res = 0;
    struct usb_endpoint_descriptor *ep_desc;
    int epi;

    for(epi=0; epi<15; epi++)
    {
        ep_desc = &descs[epi];
        if(ep_desc->bLength != 0) {
            res = usbHsIfOpenUsbEp(session, endpoint, 1, ep_desc->wMaxPacketSize, ep_desc);
            break;
        }
    }

    return res;
}

extern void *fatfs_block_dev;

Result usb_test()
{
    UsbHsInterfaceFilter iface_filter;
    UsbHsInterface interfaces[8];
    UsbHsClientIfSession session;
    UsbHsClientEpSession in_endpoint;
    UsbHsClientEpSession out_endpoint;
    
    
    memset(&iface_filter, 0, sizeof(iface_filter));
    memset(&interfaces, 0, sizeof(interfaces));
    memset(&session, 0, sizeof(session));
    memset(&in_endpoint, 0, sizeof(in_endpoint));
    memset(&out_endpoint, 0, sizeof(out_endpoint));

    usbHsInitialize();

    iface_filter.Flags = UsbHsInterfaceFilterFlags_bInterfaceClass | UsbHsInterfaceFilterFlags_bInterfaceSubClass | UsbHsInterfaceFilterFlags_bInterfaceProtocol;
    iface_filter.bInterfaceClass = 8;
    iface_filter.bInterfaceSubClass = 6;
    iface_filter.bInterfaceProtocol = 80;

    Event e;
    Result res = usbHsCreateInterfaceAvailableEvent(&e, true, 0, &iface_filter);
    if(R_FAILED(res))
    {
        printf("usbHsCreateInterfaceAvailableEvent failed %08x\n", res);
        return res;
    }
    
    eventWait(&e, -1);
    printf("Looking for usb devices...\n");


    s32 num_devices;
    res = usbHsQueryAvailableInterfaces(&iface_filter, interfaces, sizeof(interfaces), &num_devices);

    if(R_FAILED(res))
    {
        printf("usbHsQueryAvailableInterfaces failed %08x\n", res);
        goto cleanup_event;
    }

    if(num_devices > 0)
    {
        printf("woah we found at least 1 interface wtf %i\n", num_devices);
        printf("%s?\n", interfaces[0].pathstr);
    }
    else
    {
        printf("no interfaces :(\n");
        goto cleanup_event;
    }

    res = usbHsAcquireUsbIf(&session, &interfaces[0]);
    if(R_FAILED(res))
    {
        printf("usbHsAcquireUsbIf failed %08x\n", res);
        goto cleanup_event;
    }

    /*res = usbHsIfResetDevice(&session); // Reset the device!
    if(R_FAILED(res))
    {
        printf("usbHsIfResetDevice failed %08x\n", res);
        goto cleanup_session;
    }

    res = usbHsAcquireUsbIf(&session, &interfaces[0]);
    if(R_FAILED(res))
    {
        printf("usbHsAcquireUsbIf 2nd time failed %08x\n", res);
        goto cleanup_session;
    }*/
    
    res = search_and_open_endpoint(&session, session.inf.inf.input_endpoint_descs, &in_endpoint);
    if(R_FAILED(res))
    {
        printf("search_and_open_endpoint failed %08x!\n", res);
        goto cleanup_session;
    }

    res = search_and_open_endpoint(&session, session.inf.inf.output_endpoint_descs, &out_endpoint);
    if(R_FAILED(res))
    {
        printf("search_and_open_endpoint failed %08x!\n", res);
        goto cleanup_in;
    }

    {
        ScsiDevice d(&session, &in_endpoint, &out_endpoint);
        
        // hardcode it
        // ew
        printf("Using partition 0..\n");
        ScsiBlock block_dev(&d);
        printf("part 0 starts at %08x, %08x\n", block_dev.partition_infos[0].start_lba, block_dev.partition_infos[0].num_sectors);

        fatfs_block_dev = &block_dev.partitions[0];

        FATFS fatfs;
        FRESULT r = f_mount(&fatfs, "0", 0);
        if(r != FR_OK)
        {
            printf("f_mount failed %i\n", r);
        }

        DIR dirinfo;
        r = f_opendir(&dirinfo, "0:/");
        if(r != FR_OK)
        {
            printf("f_opendir failed %i\n", r);
        }

        FILINFO finfo;
        do
        {
            r = f_readdir(&dirinfo, &finfo);
            if(r != FR_OK){
                printf("f_readdir failed! %i\n", r);
                break;
            }
            if(finfo.fname != nullptr)
            {
                printf("%s\n", finfo.fname);
            }
        }
        while(finfo.fname != nullptr && strlen(finfo.fname) != 0);
        f_closedir(&dirinfo);
    }

//cleanup_out:
    usbHsEpClose(&out_endpoint);
cleanup_in:
    usbHsEpClose(&in_endpoint);
cleanup_session:
    usbHsIfClose(&session);
cleanup_event:
    usbHsDestroyInterfaceAvailableEvent(&e, 0);
    usbHsExit();

    return res;
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    socketInitializeDefault();
    nxlinkStdio();
    // This example uses a text console, as a simple way to output text to the screen.
    // If you want to write a software-rendered graphics application,
    //   take a look at the graphics/simplegfx example, which uses the libnx Framebuffer API instead.
    // If on the other hand you want to write an OpenGL based application,
    //   take a look at the graphics/opengl set of examples, which uses EGL instead.
    consoleInit(NULL);

    // Other initialization goes here. As a demonstration, we print hello world.
    printf("Hello World!\n");
    usb_test();
    // Main loop
    while (appletMainLoop())
    {
        // Scan all the inputs. This should be done once for each frame
        hidScanInput();

        // hidKeysDown returns information about which buttons have been
        // just pressed in this frame compared to the previous one
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS)
            break; // break in order to return to hbmenu
        // Your code goes here

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    socketExit();

    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    return 0;
}
