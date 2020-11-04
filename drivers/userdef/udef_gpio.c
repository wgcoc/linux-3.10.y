#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#define CMD_SIMCOM_3D8VOL_SUPPLY  10
#define CMD_SIMCOM_POWER_KEY      11
#define CMD_USBHUB1_RESET         12
#define CMD_USBHUB2_RESET         13
#define CMD_CP2108_RESET          14
#define CMD_RS485_TR_CTL          15
#define CMD_RAIN_SNOW_SIG         16
#define CMD_EMERGENCY_STOP_SIG    17
#define CMD_SRM815_3D8VOL_SUPPLY  18
#define CMD_SRM815_POWER_KEY      19
#define CMD_SRM815_RESET          20
#define CMD_SRM815_WAKE_ON        21
#define CMD_SRM815_FLIGHT         22
    

#define DEVICE_MINOR_NUM 2
#define DEVICE_NAME "udef_gpio"

#define GPIO_MUX_BASE(offset)    (0x120f0000 + (offset))      // 管脚复用寄存器基地址
#define GPIO_NUM_BASE(num)       (0x12150000 + 0x10000*(num)) // GPIO基地址

/*
 * 写/读寄存器
*/
#define REG_WRITE(addr,value)  ((*(volatile unsigned int *)(addr)) = (value))
#define REG_READ(Addr)         (*(volatile unsigned int *)(Addr))

#define REG_BIT_SET(addr,value) do { \
		unsigned int reg; \
		reg = REG_READ((addr)); \
		reg |= (value); \
		(*(volatile unsigned int *)(addr)) = reg; \
	}while(0);

#define REG_BIT_RESET(addr,value) \
	do { \
		unsigned int reg; \
		reg = REG_READ((addr)); \
		reg &= ~(value); \
		(*(volatile unsigned int *)(addr)) = reg; \
	}while(0);

static unsigned int _virt_gpio_mux_base = 0; // 对应 0x120f0000 的虚拟地址
static unsigned int _virt_gpio0_base = 0;    // 对应 0x12150000 的虚拟地址
static unsigned int _virt_gpio4_base = 0;    // 对应 0x12190000 的虚拟地址
static unsigned int _virt_gpio5_base = 0;    // 对应 0x121A0000 的虚拟地址
static unsigned int _virt_gpio11_base = 0;   // 对应 0x12200000 的虚拟地址
static unsigned int _virt_gpio13_base = 0;   // 对应 0x12220000 的虚拟地址

static int major = 99;
static int minor = 0;
static dev_t devno;
static struct class *cls;
static struct device *test_device;

static int udef_gpio_open(struct inode *inode, struct file *filep){
    // 重映射GPIO复用寄存器基地址
    if(( _virt_gpio_mux_base = (unsigned int)ioremap_nocache(GPIO_MUX_BASE(0),512)) == 0){
    	printk(KERN_INFO "0x120f0000 ioremap failed \n");
    }

    // 重映射GPIO0基地址
    if(( _virt_gpio0_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(0),0x10000)) == 0){
    	printk(KERN_INFO "0x12150000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x148, 0x00); // 0x148 是GPIO0_1的复用地址
    REG_BIT_SET(_virt_gpio0_base + 0x400, 0x02); // 配置GPIO0_1方向(output)
    //REG_BIT_RESET(_virt_gpio0_base + (0x02<<2), 0x02); // 配置GPIO0_1输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x150, 0x00); // 0x150 是GPIO0_2的复用地址
    REG_BIT_SET(_virt_gpio0_base + 0x400, 0x04); // 配置GPIO0_2方向(output)
    //REG_BIT_RESET(_virt_gpio0_base + (0x04<<2), 0x04); // 配置GPIO0_2输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x158, 0x00); // 0x158 是GPIO0_4的复用地址
    REG_BIT_SET(_virt_gpio0_base + 0x400, 0x10);  //配置GPIO0_4方向(output
    //REG_BIT_RESET(_virt_gpio0_base + (0x10<<2), 0x10); // 配置GPIO0_4输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x15c, 0x00); // 0x15c 是GPIO0_6的复用地址
    REG_BIT_SET(_virt_gpio0_base + 0x400, 0x40); // 配置GPIO0_6方向(output)
    //REG_BIT_RESET(_virt_gpio0_base + (0x40<<2), 0x40); // 配置GPIO0_6输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x160, 0x00); // 0x160 是GPIO0_7的复用地址
    REG_BIT_SET(_virt_gpio0_base + 0x400, 0x80); // 配置GPIO0_7方向(output)
    //REG_BIT_RESET(_virt_gpio0_base + (0x80<<2), 0x80); // 配置GPIO0_7输出电平

    // 重映射GPIO4基地址
    if(( _virt_gpio4_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(4),0x10000)) == 0){
    	printk(KERN_INFO "0x12190000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x170, 0x01); // 0x170 是GPIO4_1的复用地址
    REG_BIT_SET(_virt_gpio4_base + 0x400, 0x02); // 配置GPIO4_1方向(output)
    //REG_BIT_RESET(_virt_gpio4_base + (0x02<<2), 0x02); // 配置GPIO4_1输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x174, 0x01); // 0x174 是GPIO4_2的复用地址
    REG_BIT_SET(_virt_gpio4_base + 0x400, 0x04); // 配置GPIO4_2方向(output)
    //REG_BIT_RESET(_virt_gpio4_base + (0x04<<2), 0x04); // 配置GPIO4_2输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x178, 0x01); // 0x178 是GPIO4_3的复用地址
    REG_BIT_SET(_virt_gpio4_base + 0x400, 0x08); // 配置GPIO4_3方向(output)
    //REG_BIT_RESET(_virt_gpio4_base + (0x08<<2), 0x08); // 配置GPIO4_3输出电平

    // 重映射GPIO5基地址
    if(( _virt_gpio5_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(5),0x10000)) == 0){
    	printk(KERN_INFO "0x121A0000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x10C, 0x00); // 0x10C 是GPIO5_5的复用地址
    REG_BIT_SET(_virt_gpio5_base + 0x400, 0x20); // 配置GPIO5_5方向(output)
    //REG_BIT_RESET(_virt_gpio5_base + (0x20<<2), 0x20); // 配置GPIO5_5输出电平

    // 重映射GPIO11基地址
    if(( _virt_gpio11_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(11),0x10000)) == 0){
    	printk(KERN_INFO "0x12200000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x1d4, 0x00); // 0x1d4 是GPIO11_1的复用地址
    REG_BIT_RESET(_virt_gpio11_base + 0x400, 0x02); // 配置GPIO11_1方向(input)
    //REG_BIT_RESET(_virt_gpio11_base + (0x02<<2)); // 读取GPIO11_1输入电平

    REG_WRITE(_virt_gpio_mux_base + 0x1dc, 0x00); // 0x1dc 是GPIO11_3的复用地址
    REG_BIT_SET(_virt_gpio11_base + 0x400, 0x08); // 配置GPIO11_3方向(output)
    //REG_BIT_RESET(_virt_gpio11_base + (0x08<<2), 0x08); // 配置GPIO11_3输出电平

    // 重映射GPIO13基地址
    if(( _virt_gpio13_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(13),0x10000)) == 0){
    	printk(KERN_INFO "0x12220000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x1b4, 0x02); // 0x1b4 是GPIO13_1的复用地址
    REG_BIT_SET(_virt_gpio13_base + 0x400, 0x02); // 配置GPIO13_1方向(output)
    //REG_BIT_RESET(_virt_gpio13_base + (0x02<<2), 0x02); // 配置GPIO13_1输出电平

    REG_WRITE(_virt_gpio_mux_base + 0x1c4, 0x00); // 0x1c4 是GPIO13_5的复用地址
    REG_BIT_RESET(_virt_gpio13_base + 0x400, 0x20); // 配置GPIO13_5方向(input)
    //REG_READ(_virt_gpio13_base + (0x20<<2)); // 读取GPIO13_5输入电平

    REG_WRITE(_virt_gpio_mux_base + 0x1CC, 0x00); // 0x1CC 是GPIO13_7的复用地址
    REG_BIT_SET(_virt_gpio13_base + 0x400, 0x80); // 配置GPIO13_7方向(output)
    //REG_BIT_RESET(_virt_gpio13_base + (0x80<<2), 0x80); // 配置GPIO13_7输出电平

    printk(KERN_INFO "udef_gpio_open()\n");
    return 0;
}

static int udef_gpio_release(struct inode *inode, struct file *filep){
        printk(KERN_INFO "udef_gpio_release()\n");
	return 0;
}

static long udef_gpio_ioctl(struct file *file,unsigned int cmd,unsigned long arg){
    printk(KERN_INFO "gpio_ctl: cmd = %d, arg = %lu \n",cmd,arg);

    if(cmd == CMD_SIMCOM_3D8VOL_SUPPLY){
	if(arg){ REG_BIT_SET(_virt_gpio13_base + (0x80<<2), 0x80); }
        else{  REG_BIT_RESET(_virt_gpio13_base + (0x80<<2), 0x80); } //GPIO13_7
    }else if(cmd == CMD_SIMCOM_POWER_KEY){
	if(arg){ REG_BIT_SET(_virt_gpio0_base + (0x02<<2), 0x02); }
        else{  REG_BIT_RESET(_virt_gpio0_base + (0x02<<2), 0x02); } //GPIO0_1
    }else if(cmd == CMD_USBHUB1_RESET){
	if(arg){ REG_BIT_SET(_virt_gpio0_base + (0x10<<2), 0x10); }
        else{  REG_BIT_RESET(_virt_gpio0_base + (0x10<<2), 0x10); }  //GPIO0_4
    }else if(cmd == CMD_USBHUB2_RESET){
	if(arg){ REG_BIT_SET(_virt_gpio13_base + (0x02<<2), 0x02); }
        else{  REG_BIT_RESET(_virt_gpio13_base + (0x02<<2), 0x02); } //GPIO13_1
    }else if(cmd == CMD_CP2108_RESET){
	if(arg){ REG_BIT_SET(_virt_gpio0_base + (0x40<<2), 0x40); }
        else{  REG_BIT_RESET(_virt_gpio0_base + (0x40<<2), 0x40); } //GPIO0_6
    }else if(cmd == CMD_RS485_TR_CTL){
	if(arg){ REG_BIT_SET(_virt_gpio5_base + (0x20<<2), 0x20); }
        else{  REG_BIT_RESET(_virt_gpio5_base + (0x20<<2), 0x20); } //GPIO5_5
    }else if(cmd == CMD_RAIN_SNOW_SIG){
        return (REG_READ(_virt_gpio11_base + (0x02<<2)) & 0x02)?1:0; // GPIO11_1
    }else if(cmd == CMD_EMERGENCY_STOP_SIG){
        return (REG_READ(_virt_gpio13_base + (0x20<<2)) & 0x20)?1:0; // GPIO13_5
    }else if(cmd == CMD_SRM815_3D8VOL_SUPPLY){
	if(arg){ REG_BIT_SET(_virt_gpio11_base + (0x08<<2), 0x08); } // GPIO11_3
	else{  REG_BIT_RESET(_virt_gpio11_base + (0x08<<2), 0x08); }
    }else if(cmd == CMD_SRM815_POWER_KEY){
	if(arg){ REG_BIT_SET(_virt_gpio0_base + (0x80<<2), 0x80); } // GPIO0_7
	else{  REG_BIT_RESET(_virt_gpio0_base + (0x80<<2), 0x80); }
    }else if(cmd == CMD_SRM815_RESET){
	if(arg){ REG_BIT_SET(_virt_gpio4_base + (0x08<<2), 0x08); } // GPIO4_3
	else{  REG_BIT_RESET(_virt_gpio4_base + (0x08<<2), 0x08); }
    }else if(cmd == CMD_SRM815_WAKE_ON){
	if(arg){ REG_BIT_SET(_virt_gpio4_base + (0x02<<2), 0x02); } // GPIO4_1
	else{  REG_BIT_RESET(_virt_gpio4_base + (0x02<<2), 0x02); }
    }else if(cmd == CMD_SRM815_FLIGHT){
	if(arg){ REG_BIT_SET(_virt_gpio0_base + (0x04<<2), 0x04); } // GPIO0_2
	else{  REG_BIT_RESET(_virt_gpio0_base + (0x04<<2), 0x04); }
    }else{
	return -1;
    }

    return 0;
}

static struct file_operations udef_gpio_ops =
{
        .open    = udef_gpio_open,
        .release = udef_gpio_release, //当执行close()函数时会调用release()
	.unlocked_ioctl = udef_gpio_ioctl,
};


static int __init  udef_gpio_init(void){
    int ret;
    if(major){ //静态申请方法
        devno = MKDEV(major, minor);
        ret = register_chrdev(major,DEVICE_NAME, &udef_gpio_ops);
    }else{ //动态申请方法
        ret = alloc_chrdev_region(&devno,minor,DEVICE_MINOR_NUM,DEVICE_NAME); //这个好像还没有添加file_operation
        major = MAJOR(devno);
        minor = MINOR(devno);
        ret = register_chrdev(major,DEVICE_NAME, &udef_gpio_ops);
        printk(KERN_INFO "alloc region %d \n",major);
    }

    cls = class_create(THIS_MODULE, "myclass"); //创建类
    if(IS_ERR(cls)){
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "class create failed \n");
        return -EBUSY;
    }
    printk(KERN_INFO "class create successs \n");

    //创建设备节点
    test_device = device_create(cls, NULL, devno, NULL, DEVICE_NAME); //mknod /dev/led
    if (IS_ERR(test_device)) {
        class_destroy(cls);
        unregister_chrdev(major,DEVICE_NAME);
        printk(KERN_ERR "create /dev/udef_gpio failed \n");
        return -EBUSY;
    }
    printk(KERN_INFO "create /dev/udef_gpio successs \n");
    printk(KERN_WARNING "[%s %s %d] minor = %u\n", __FILE__, __func__, __LINE__,0xFF);
    return 0;
}

static void __exit udef_gpio_exit(void){
    iounmap((void*)_virt_gpio_mux_base);
    iounmap((void*)_virt_gpio0_base);
    iounmap((void*)_virt_gpio4_base);
    iounmap((void*)_virt_gpio5_base);
    iounmap((void*)_virt_gpio11_base);
    iounmap((void*)_virt_gpio13_base);
    printk(KERN_WARNING "[%s %s %d] minor = %u\n", __FILE__, __func__, __LINE__,0xFF);
}

module_init(udef_gpio_init);
module_exit(udef_gpio_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("wgco");
