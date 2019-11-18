#include <linux/init.h>	
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include <linux/fb.h>
#include <linux/switch.h>

/*add by Wenke Ma, for hall switch key code*/
#define KEY_HALL_OPEN                0x288
#define KEY_HALL_CLOSE               0x289
#define KEY_HALL_HOLDER_OPEN                0x289
#define KEY_HALL_HOLDER_CLOSE               0x288

#define HALL_COVER_IRQ  "hall-cover"
#define HALL_HOLDER_IRQ "hall-holder"
#define CONFIG_HALL_SYS

struct hall_switch_irq
{
    int gpio;
    int irq;
    int state;
    char irq_name[32];
    struct switch_dev sdev;
    struct work_struct hall_switch_work;
};

struct hall_switch_info
{
	struct mutex io_lock;
	struct hall_switch_irq cover;
    struct hall_switch_irq holder;
    u32 irq_gpio;
    int hall_switch_state;
	struct input_dev *ipdev;
	struct workqueue_struct *workq;
#ifdef CONFIG_HALL_SYS
	struct class *hall_sys_class;
#endif
};
  static int hall_irq_value;
struct hall_switch_info *global_hall_info;
//struct hall_switch_irq *global_irq_info;
static irqreturn_t hall_interrupt(int irq, void *data)
{
	struct hall_switch_irq *hall = data;
	int hall_gpio;
       struct hall_switch_info *hall_info = NULL;

	disable_irq_nosync(irq);
	hall_gpio = gpio_get_value(hall->gpio);
	pr_info("hall irq interrupt name = %s  irq = %d state = %d\n", hall->irq_name, irq, hall_gpio);

	if(hall_gpio == hall->state){
            enable_irq(irq);
	     return IRQ_HANDLED;
	}else{
	     hall->state = hall_gpio;
            //switch_set_state(&hall->sdev, hall->state);
	}
	hall_irq_value = hall->state;
       if (!strcmp(hall->irq_name, HALL_COVER_IRQ)) {
            hall_info = container_of(hall, struct hall_switch_info, cover);
            if (hall->state) {
                input_report_key(hall_info->ipdev, KEY_HALL_OPEN, 1);
                input_report_key(hall_info->ipdev, KEY_HALL_OPEN, 0);
                input_sync(hall_info->ipdev);
            }else{
                input_report_key(hall_info->ipdev, KEY_HALL_CLOSE, 1);
                input_report_key(hall_info->ipdev, KEY_HALL_CLOSE, 0);
                input_sync(hall_info->ipdev);
            }
       } else if (!strcmp(hall->irq_name, HALL_HOLDER_IRQ)) {
            hall_info = container_of(hall, struct hall_switch_info, holder);
	     if (hall->state) {
                input_report_key(hall_info->ipdev, KEY_HALL_HOLDER_CLOSE, 1);
                input_report_key(hall_info->ipdev, KEY_HALL_HOLDER_CLOSE, 0);
                input_sync(hall_info->ipdev);
             }else{
                input_report_key(hall_info->ipdev, KEY_HALL_HOLDER_OPEN, 1);
                input_report_key(hall_info->ipdev, KEY_HALL_HOLDER_OPEN, 0);
                input_sync(hall_info->ipdev);
            }
       } else {
            pr_err("hall irq interrupt irq name not found!name = %s \n", hall->irq_name);
       }
       if (hall_info != NULL && hall_info->workq)
            queue_work(hall_info->workq, &hall->hall_switch_work);

	 enable_irq(irq);
//	 mutex_unlock(&hall_info->io_lock);
     return IRQ_HANDLED;
}

static void hall_switch_work_func(struct work_struct *work)
{
    struct hall_switch_irq *hall = NULL;

    hall = container_of(work, struct hall_switch_irq, hall_switch_work);
    pr_info("hall_switch_work_func name = %s  state = %d\n", hall->irq_name,hall->state);
    switch_set_state(&hall->sdev, hall->state);
}

static int hall_init_irq(struct hall_switch_irq *hall_switch)
{
    int rc = -1;
    struct hall_switch_irq *hall = hall_switch;
    if (gpio_is_valid(hall->gpio)) {
		rc = gpio_request(hall->gpio, hall->irq_name);
		if (rc < 0) {
		        pr_err("hall_probe: gpio_request fail rc=%d\n", rc);
		        return rc ;
		}

		rc = gpio_direction_input(hall->gpio);
		if (rc < 0) {
		        pr_err("hall_probe: gpio_direction_input fail rc=%d\n", rc);
		        return rc ;
		}
		hall->state = gpio_get_value(hall->gpio);
          hall_irq_value = hall->state;
		hall->irq = gpio_to_irq(hall->gpio);
		pr_err("Macle irq = %d\n", hall->irq);
		irq_set_irq_wake(hall->irq,1);
		rc = request_threaded_irq(hall->irq, hall_interrupt, NULL,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			hall->irq_name, hall);
		if (rc < 0) {
			pr_err("hall_probe: request_irq fail rc=%d\n", rc);
			return rc ;
		}

              hall->sdev.name = hall->irq_name;
              rc = switch_dev_register(&hall->sdev);
              if (rc) {
                    pr_err("Failed to setup switch dev for %s", hall->irq_name);
                    return rc;
              }
              switch_set_state(&hall->sdev, hall->state);
              INIT_WORK(&hall->hall_switch_work, hall_switch_work_func);
	}else{
		pr_err("Macle irq gpio not provided\n");
		return rc ;
	}
	  //global_irq_info = hall;
	  
       return 0;
}

static int hall_parse_dt(struct device *dev, struct hall_switch_info *pdata)
{
	struct device_node *np = dev->of_node;
       enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	pdata->cover.gpio = of_get_named_gpio_flags(np, "hall,irq-cover",
				0, &flags);
	if (pdata->cover.gpio < 0) {
        pr_warn("Macle hall get cover gpio fail\n");
    } else {
        strcpy(pdata->cover.irq_name,HALL_COVER_IRQ);
    }

    pdata->holder.gpio = of_get_named_gpio_flags(np, "hall,irq-holder",
				0, &flags);
	if (pdata->holder.irq < 0) {
        pr_warn("Macle hall get holder gpio fail\n");
    } else {
        strcpy(pdata->holder.irq_name,HALL_HOLDER_IRQ);
    }

	pr_info("Macle hall_parse_dt success \n");
	return 0;
}
#ifdef CONFIG_HALL_SYS
static ssize_t hall_state_show(struct class *class,
		struct class_attribute *attr, char *buf)
{	
	int state;
	if (hall_irq_value) {
		state = 3;
	}else{
		state = 2;
	}
	pr_err("Macle hall_state_show state = %d, custome_state=%d\n",hall_irq_value, state);
	return sprintf(buf, "%d\n", state);
}

static struct class_attribute hall_state =
	__ATTR(hall_state, S_IRUGO, hall_state_show, NULL);

static int hall_register_class_dev(struct hall_switch_info *hall_info){
	int err;
	if (!hall_info->hall_sys_class) {
		hall_info->hall_sys_class = class_create(THIS_MODULE, "hall_class");
		if (IS_ERR(hall_info->hall_sys_class)){
			hall_info->hall_sys_class = NULL;
			printk(KERN_ERR "could not allocate hall_class\n");
			return -1;
		}
	}

	err = class_create_file(hall_info->hall_sys_class, &hall_state);
	if(err < 0){
		class_destroy(hall_info->hall_sys_class);
		return -1;
	}
	return 0;
}
#endif

static int hall_probe(struct platform_device *pdev)
{
	int rc = 0;
	int err;
	struct hall_switch_info *hall_info;
	pr_err("Macle hall_probe\n");
		
	if (pdev->dev.of_node) {
		hall_info = kzalloc(sizeof(struct hall_switch_info), GFP_KERNEL);		  
		if (!hall_info) {
			pr_err("Macle %s: failed to alloc memory for module data\n",__func__);
			return -ENOMEM;
		}
		err = hall_parse_dt(&pdev->dev, hall_info);
		if (err) {
			dev_err(&pdev->dev, "Macle hall_probe DT parsing failed\n");
			goto free_struct;
		}
	} else{
		return -ENOMEM;
	}
	mutex_init(&hall_info->io_lock);

    hall_info->workq = create_workqueue("hall_workq");
	if (!hall_info->workq) {
		pr_err("%s: hall_workq creation failed.\n", __func__);
		rc = -EPERM;
		goto free_struct;
	}

	platform_set_drvdata(pdev, hall_info);

/*input system config*/
	hall_info->ipdev = input_allocate_device();
	if (!hall_info->ipdev) {
		pr_err("hall_probe: input_allocate_device fail\n");
		goto input_error;
	}
	hall_info->ipdev->name = "hall-switch-input";
	input_set_capability(hall_info->ipdev, EV_KEY, KEY_HALL_OPEN);
	input_set_capability(hall_info->ipdev, EV_KEY, KEY_HALL_CLOSE);
	input_set_capability(hall_info->ipdev, EV_KEY, KEY_HALL_HOLDER_OPEN);
	input_set_capability(hall_info->ipdev, EV_KEY, KEY_HALL_HOLDER_CLOSE);
	set_bit(INPUT_PROP_NO_DUMMY_RELEASE, hall_info->ipdev->propbit);
	rc = input_register_device(hall_info->ipdev);
	if (rc) {
		pr_err("hall_probe: input_register_device fail rc=%d\n", rc);
		goto input_error;
	}

/*interrupt config*/
    if (hall_info->cover.gpio > 0)
	   rc = hall_init_irq(&hall_info->cover);
       
    if (hall_info->holder.gpio > 0)
       rc |= hall_init_irq(&hall_info->holder);
       
    if (rc) {
       pr_err("hall_probe: hall_init_irq fail rc=%d\n", rc);
       goto err_irq;
    }
    pr_err("hall_probe end\n");
#ifdef CONFIG_HALL_SYS
	hall_register_class_dev(hall_info);
#endif
   global_hall_info = hall_info;
    return 0;
err_irq:
	gpio_free(hall_info->cover.gpio);
    gpio_free(hall_info->holder.gpio);
	input_unregister_device(hall_info->ipdev);
input_error:
	platform_set_drvdata(pdev, NULL);
       if (hall_info->workq)
		destroy_workqueue(hall_info->workq);
free_struct:
	kfree(hall_info);

	return rc;
}

static int hall_remove(struct platform_device *pdev)
{
	struct hall_switch_info *hall = platform_get_drvdata(pdev);
	pr_err("hall_remove\n");
	if (hall->workq)
	   destroy_workqueue(hall->workq);
#ifdef CONFIG_HALL_SYS
	class_destroy(hall->hall_sys_class);
#endif
	free_irq(hall->cover.irq, hall->ipdev);
    free_irq(hall->holder.irq, hall->ipdev);
	gpio_free(hall->cover.gpio);
    gpio_free(hall->holder.gpio);
    switch_dev_unregister(&hall->cover.sdev);
    switch_dev_unregister(&hall->holder.sdev);
	input_unregister_device(hall->ipdev);
	return 0;
}


static struct of_device_id sn_match_table[] = {
	{ .compatible = "hall-switch,ah1887", },
	{ },
}; 

static struct platform_driver hall_driver = {
	.probe                = hall_probe,
	.remove                = hall_remove,
	.driver                = {
		.name        = "hall-switch",
		.owner        = THIS_MODULE,
		.of_match_table = sn_match_table,
	},
};

static int __init hall_init(void)
{
	return platform_driver_register(&hall_driver);
}

static void __exit hall_exit(void)
{
	platform_driver_unregister(&hall_driver);
}

//module_platform_driver(hall_driver);

module_init(hall_init);
module_exit(hall_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("mawenke");
