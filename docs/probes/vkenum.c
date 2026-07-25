#include <vulkan/vulkan.h>
#include <stdio.h>
int main(void){
    VkApplicationInfo ai={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="vkenum",.apiVersion=VK_API_VERSION_1_1};
    VkInstanceCreateInfo ci={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&ai};
    VkInstance inst;
    VkResult r=vkCreateInstance(&ci,NULL,&inst);
    if(r!=VK_SUCCESS){printf("vkCreateInstance failed: %d\n",r);return 1;}
    uint32_t n=0; vkEnumeratePhysicalDevices(inst,&n,NULL);
    printf("physical devices: %u\n",n);
    if(n){VkPhysicalDevice d[8]; if(n>8)n=8; vkEnumeratePhysicalDevices(inst,&n,d);
        for(uint32_t i=0;i<n;i++){VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(d[i],&p);
            printf("  [%u] %s  (apiVersion %u.%u.%u, type %d)\n",i,p.deviceName,
                VK_VERSION_MAJOR(p.apiVersion),VK_VERSION_MINOR(p.apiVersion),VK_VERSION_PATCH(p.apiVersion),p.deviceType);}}
    vkDestroyInstance(inst,NULL);
    return n?0:2;
}
