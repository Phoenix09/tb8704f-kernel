#include <linux/module.h>  
#include <linux/version.h>  
#include <linux/init.h>  
#include <linux/kernel.h>  
#include <linux/types.h>  
#include <linux/interrupt.h>  
#include <linux/list.h>  
#include <linux/timer.h>  
#include <linux/init.h>  
#include <linux/serial_core.h>  
#include <linux/platform_device.h>

static void lct_mic_release(struct device * dev)  
{  
}  
          
static struct platform_device lct_mic_device = {  
    .id         = -1,  
    .name       = "lct_mic",
    .dev            ={  
        .release    = lct_mic_release,  
    },  
};  

static int lct_mic_dev_init(void)  
{  
    return platform_device_register(&lct_mic_device);  
}  
  
static void lct_mic_dev_exit(void)  
{  
    platform_device_unregister(&lct_mic_device);  
}  
  
module_init(lct_mic_dev_init);  
module_exit(lct_mic_dev_exit);  
  
MODULE_LICENSE("GPL");  
