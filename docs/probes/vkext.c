#include <vulkan/vulkan.h>
#include <stdio.h>
#include <string.h>
int main(void){
    uint32_t n=0; vkEnumerateInstanceExtensionProperties(NULL,&n,NULL);
    VkExtensionProperties *e=calloc(n,sizeof(*e));
    vkEnumerateInstanceExtensionProperties(NULL,&n,e);
    printf("=== %u instance extensions ===\n",n);
    int surface=0,wayland=0,headless=0,display=0,xcb=0,xlib=0;
    for(uint32_t i=0;i<n;i++){
        printf("  %s\n",e[i].extensionName);
        if(!strcmp(e[i].extensionName,"VK_KHR_surface"))surface=1;
        if(!strcmp(e[i].extensionName,"VK_KHR_wayland_surface"))wayland=1;
        if(!strcmp(e[i].extensionName,"VK_EXT_headless_surface"))headless=1;
        if(!strcmp(e[i].extensionName,"VK_KHR_display"))display=1;
        if(!strcmp(e[i].extensionName,"VK_KHR_xcb_surface"))xcb=1;
        if(!strcmp(e[i].extensionName,"VK_KHR_xlib_surface"))xlib=1;
    }
    printf("summary: surface=%d wayland=%d headless=%d display=%d xcb=%d xlib=%d\n",
        surface,wayland,headless,display,xcb,xlib);
    return 0;
}
