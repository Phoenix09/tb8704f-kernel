#include <linux/module.h>  
#include <linux/init.h>  
#include <linux/fs.h>  
#include <linux/interrupt.h>  
#include <linux/irq.h>  
#include <linux/sched.h>  
#include <linux/pm.h>  
#include <linux/sysctl.h>  
#include <linux/proc_fs.h>  
#include <linux/delay.h>  
#include <linux/platform_device.h>  
#include <linux/input.h>  
#include <linux/gpio_keys.h>  
#include <asm/uaccess.h>   // copy_from_user  
#include <asm/io.h>  // ioremap  
#include "lct_mic_drv.h"

#ifdef CONFIG_KERNEL_CUSTOM_P3590
static int us_euro_gpio_value = 0;
#else
static int us_euro_gpio_value = 1;
#endif

static struct kobject *msm_mic_device;

void lct_mic_set(int val)
{
    us_euro_gpio_value = val;
	printk("[lijianhui] val = %d\n", val);
}

static ssize_t 
show_msm8952_us_euro_gpio(struct device *dev, struct device_attribute *da, char *buf)
{
	return sprintf(buf, "%d\n", us_euro_gpio_value);
}

static DEVICE_ATTR(us_eu, S_IRUGO,show_msm8952_us_euro_gpio, NULL);
  
static int lct_mic_probe(struct platform_device *pdev)  
{  
	int ret;

	msm_mic_device = kobject_create_and_add("android_mic", NULL);
	if (msm_mic_device == NULL) {
		pr_info("%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret ;
	}
  
	ret = sysfs_create_file(msm_mic_device ,&dev_attr_us_eu.attr);
	if (ret) {
		pr_info("%s: sysfs_create_file failed\n", __func__);
		kobject_del(msm_mic_device);
	}
		
    return 0;  
}  
static int lct_mic_remove(struct platform_device *pdev)  
{   
    return 0;  
}  
  
static struct platform_driver lct_mic_driver = {  
    .probe      = lct_mic_probe,  
    .remove     = lct_mic_remove,  
    .driver     = {  
        .name   = "lct_mic",  
        .owner  = THIS_MODULE,  
    }  
};  
 
static int __init led_drv_init(void)  
{  
    return platform_driver_register(&lct_mic_driver);  
}  
  
static void __exit led_drv_exit(void)  
{  
    platform_driver_unregister(&lct_mic_driver);  
}  
  
module_init(led_drv_init);  
module_exit(led_drv_exit);  
  
MODULE_LICENSE("GPL");  
//MODULE_AUTHOR("lijianhui");  
//MODULE_DESCRIPTION("Just for Demo"); 
