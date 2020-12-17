/*
 * Basic kernel module taking commandline arguments.
 *
 * Author:
 * 	Stefan Wendler (devnull@kaltpost.de)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

/*
 * The module commandline arguments ...
 */
static int speed = 5;
static int ioEdge = 5;
static int myintArray[2] = {-1, -1};
static int arr_argc = 0;
static int count = 0;

module_param(speed, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(speed, "An integer");
module_param_array(myintArray, int, &arr_argc, 0000);
MODULE_PARM_DESC(myintArray, "An array of integers");
module_param(ioEdge, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ioEdge, "An integer");

// #define LED1 4

static struct timer_list blink_timer;
static long led1 = 0;
static long led2 = 0;
static long led3 = 0;

static struct gpio leds[] = {
	{4, GPIOF_OUT_INIT_LOW, "LED 1"},
};

/* Define GPIOs for BUTTONS */
static struct gpio buttons[] = {
	{17, GPIOF_IN, "BUTTON 1"}, // turns LED on
	{18, GPIOF_IN, "BUTTON 2"}, // turns LED off
};

/* Later on, the assigned IRQ numbers for the buttons are stored here */
static int button_irqs[] = {-1, -1};

/*
 * The interrupt service routine called on button presses
 */
static irqreturn_t button_isr(int irq, void *data)
{
	if (irq == button_irqs[0])
	{
		gpio_set_value(leds[0].gpio, led3);
		led3 = !led3;
		count = count + 1;
		if (ioEdge == 17)
		{
			printk(KERN_INFO "count button on pin 4 is: %d\n", count);
		}
	}
	else if (irq == button_irqs[1] && gpio_get_value(leds[0].gpio))
	{
		gpio_set_value(leds[0].gpio, 0);
	}
	mdelay(1);
	return IRQ_HANDLED;
}

/*
 * Timer function called periodically
 */
static void blink_timer_func(struct timer_list *t)
{
	printk(KERN_INFO "%s\n", __func__);

	gpio_set_value(myintArray[0], led1);
	led1 = !led1;
	gpio_set_value(myintArray[1], led2);
	led2 = !led2;
	/* schedule next execution */
	//blink_timer.data = !data;						// makes the LED toggle
	blink_timer.expires = jiffies + (speed * HZ); // 1 sec.
	add_timer(&blink_timer);
}

/*
 * Module init function
 */
static int __init clargmod_init(void)
{
	int i;
	int ret = 0;

	printk(KERN_INFO "speed is an integer: %d\n", speed);
	printk(KERN_INFO "ioEdge is: %d\n", ioEdge);

	for (i = 0; i < (sizeof myintArray / sizeof(int)); i++)
	{
		printk(KERN_INFO "myintArray[%d] = %d\n", i, myintArray[i]);
	}

	printk(KERN_INFO "got %d arguments for myintArray.\n", arr_argc);

	printk(KERN_INFO "%s\n", __func__);

	// register, turn off
	ret = gpio_request_one(myintArray[0], GPIOF_OUT_INIT_LOW, "led1");
	ret = gpio_request_one(myintArray[1], GPIOF_OUT_INIT_LOW, "led2");
	if (ret)
	{
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
		return ret;
	}
	if (ret)
	{
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
		return ret;
	}

	/* init timer, add timer function */
	//init_timer(&blink_timer);
	timer_setup(&blink_timer, blink_timer_func, 0);

	blink_timer.function = blink_timer_func;
	//blink_timer.data = 1L;							// initially turn LED on
	blink_timer.expires = jiffies + (speed * HZ); // 1 sec.
	add_timer(&blink_timer);

	ret = gpio_request_array(leds, ARRAY_SIZE(leds));

	if (ret)
	{
		printk(KERN_ERR "Unable to request GPIOs for LEDs: %d\n", ret);
		return ret;
	}

	// register BUTTON gpios
	ret = gpio_request_array(buttons, ARRAY_SIZE(buttons));

	if (ret)
	{
		printk(KERN_ERR "Unable to request GPIOs for BUTTONs: %d\n", ret);
		goto fail1;
	}

	printk(KERN_INFO "Current button1 value: %d\n", gpio_get_value(buttons[0].gpio));

	ret = gpio_to_irq(buttons[0].gpio);

	if (ret < 0)
	{
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail2;
	}

	button_irqs[0] = ret;

	printk(KERN_INFO "Successfully requested BUTTON1 IRQ # %d\n", button_irqs[0]);

	ret = request_irq(button_irqs[0], button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#button1", NULL);

	if (ret)
	{
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail2;
	}

	ret = gpio_to_irq(buttons[1].gpio);

	if (ret < 0)
	{
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail2;
	}

	button_irqs[1] = ret;
	printk(KERN_INFO "Successfully requested BUTTON2 IRQ # %d\n", button_irqs[1]);

	ret = request_irq(button_irqs[1], button_isr, IRQF_TRIGGER_RISING /*| IRQF_DISABLED*/, "gpiomod#button2", NULL);

	if (ret)
	{
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	return 0;

// cleanup what has been setup so far
fail3:
	free_irq(button_irqs[0], NULL);

fail2:
	gpio_free_array(buttons, ARRAY_SIZE(leds));

fail1:
	gpio_free_array(leds, ARRAY_SIZE(leds));

	return ret;
}

/*
 * Exit function
 */
static void __exit clargmod_exit(void)
{
	int i;

	printk(KERN_INFO "%s\n", __func__);

	// deactivate timer if running
	del_timer_sync(&blink_timer);

	// turn LED off
	gpio_set_value(myintArray[0], 0);

	// unregister GPIO
	gpio_free(myintArray[0]);
	gpio_set_value(myintArray[1], 0);

	// unregister GPIO
	gpio_free(myintArray[1]);

	// free irqs
	free_irq(button_irqs[0], NULL);
	free_irq(button_irqs[1], NULL);

	// turn all LEDs off
	for (i = 0; i < ARRAY_SIZE(leds); i++)
	{
		gpio_set_value(leds[i].gpio, 0);
	}

	// unregister
	gpio_free_array(leds, ARRAY_SIZE(leds));
	gpio_free_array(buttons, ARRAY_SIZE(buttons));
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andries Deklerck");
MODULE_DESCRIPTION("Basic Linux Kernel module taking command line arguments");

module_init(clargmod_init);
module_exit(clargmod_exit);
