#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/interrupt.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Sergio Tanzilli");
MODULE_DESCRIPTION("Driver for HC-SR04 ultrasonic sensor");

// Change these two lines to use differents GPIOs
#define HCSR04_ECHO		53 // IRIS-18 / sodimm-85 53
#define HCSR04_TRIGGER		50 // IRIS-17 / sodimm-97 50
//#define HCSR04_TEST  	 	5 // J4.28 -   PA5

// adaptation for kernels >= 4.1.0
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
    #define  IRQF_DISABLED 0
#endif

static int gpio_irq=-1;
static int valid_value = 0;

static ktime_t echo_start;
static ktime_t echo_end;
 
// This function is called when you write something on /sys/class/hcsr04/value
static ssize_t hcsr04_value_write(struct class *class, struct class_attribute *attr, const char *buf, size_t len) {
	printk(KERN_INFO "Buffer len %d bytes\n", len);
	return len;
}

// This function is called when you read /sys/class/hcsr04/value
static ssize_t hcsr04_value_read(struct class *class, struct class_attribute *attr, char *buf) {
	int counter;

	//printk(KERN_INFO "chegou aqui hehehe");
	// Send a 10uS impulse to the TRIGGER line
	gpio_set_value(HCSR04_TRIGGER,1);
	udelay(10);
	gpio_set_value(HCSR04_TRIGGER,0);
	valid_value=0;

	counter=0;
	while (valid_value==0) {
		// Out of range
		if (++counter>23200) {
			return sprintf(buf, "%d\n", -1);;
		}
		udelay(1);
	}
	
	//printk(KERN_INFO "Sub: %lld\n", ktime_to_us(ktime_sub(echo_end,echo_start)));
	return sprintf(buf, "%lld\n", ktime_to_us(ktime_sub(echo_end,echo_start)));;
}

// Sysfs definitions for hcsr04 class
static struct class_attribute hcsr04_class_attrs[] = {
	__ATTR(value,	S_IRUGO | S_IWUSR, hcsr04_value_read, hcsr04_value_write),
	__ATTR_NULL,
};

// Name of directory created in /sys/class
static struct class hcsr04_class = {
	.name =			"hcsr04",
	.owner =		THIS_MODULE,
	.class_attrs =	hcsr04_class_attrs,
};

// Interrupt handler on ECHO signal
static irqreturn_t gpio_isr(int irq, void *data)
{
	ktime_t ktime_dummy;

	//gpio_set_value(HCSR04_TEST,1);

	if (valid_value==0) {
		ktime_dummy=ktime_get();
		if (gpio_get_value(HCSR04_ECHO)==1) {
			echo_start=ktime_dummy;
		} else {
			echo_end=ktime_dummy;
			valid_value=1;
		}
	}

	//gpio_set_value(HCSR04_TEST,0);
	return IRQ_HANDLED;
}

static int __init hcsr04_init(void)
{	
	int rtc;
	
	printk(KERN_INFO "HC-SR04 driver v0.32 initializing.\n");

	if (class_register(&hcsr04_class)<0) goto fail;

	//rtc=gpio_request(HCSR04_TEST,"TEST");
	//if (rtc!=0) {
	//	printk(KERN_INFO "Error %d\n",__LINE__);
	//	goto fail;
	//}

	//rtc=gpio_direction_output(HCSR04_TEST,0);
	//if (rtc!=0) {
	//	printk(KERN_INFO "Error %d\n",__LINE__);
	//	goto fail;
	//}

	rtc=gpio_request(HCSR04_TRIGGER,"TRIGGER");
	if (rtc!=0) {
		printk(KERN_INFO "Error %d - line %d - trigger pin request\n",rtc, __LINE__);
		goto fail;
	}

	rtc=gpio_request(HCSR04_ECHO,"ECHO");
	if (rtc!=0) {
		printk(KERN_INFO "Error %d - line %d - echo pin request\n",rtc, __LINE__);
		goto fail;
	}

	rtc=gpio_direction_output(HCSR04_TRIGGER,0);
	if (rtc!=0) {
		printk(KERN_INFO "Error %d - line %d - trigger pin direction set\n",rtc, __LINE__);
		goto fail;
	}

	rtc=gpio_direction_input(HCSR04_ECHO);
	if (rtc!=0) {
		printk(KERN_INFO "Error %d - line %d - echo pin direction set\n",rtc, __LINE__);
		goto fail;
	}

	// http://lwn.net/Articles/532714/
	rtc=gpio_to_irq(HCSR04_ECHO);
	if (rtc<0) {
		printk(KERN_INFO "Error %d - line %d - echo pin IRQ\n",rtc, __LINE__);
		goto fail;
	} else {
		gpio_irq=rtc;
	}

	rtc = request_irq(gpio_irq, gpio_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED , "hc-sr04.trigger", NULL);

	if(rtc) {
		//printk(KERN_ERR "Unable to request IRQ: %d\n", rtc);
		goto fail;
	}


	return 0;

fail:
	return -1;
}
 
static void __exit hcsr04_exit(void)
{
	if (gpio_irq!=-1) {	
		free_irq(gpio_irq, NULL);
	}
	gpio_free(HCSR04_TRIGGER);
	gpio_free(HCSR04_ECHO);
	class_unregister(&hcsr04_class);
	printk(KERN_INFO "HC-SR04 disabled.\n");
}

module_init(hcsr04_init);
module_exit(hcsr04_exit);
