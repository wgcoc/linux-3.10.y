#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/types.h>

#define DEVICE_MINOR_NUM 2
#define DEVICE_NAME "udef_am2120"

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

static unsigned int _virt_gpio_mux_base = 0;   
static unsigned int _virt_gpio13_base = 0;   // 对应 0x12220000 的虚拟地址

static int major = 100;
static int minor = 0;
static dev_t devno;
static struct class *cls;
static struct device *test_device;

#define _1wire_config_output() REG_BIT_SET(_virt_gpio13_base + 0x400, 0x08)
#define _1wire_config_input()  REG_BIT_RESET(_virt_gpio13_base + 0x400, 0x08)
#define _1wire_set()           REG_BIT_SET(_virt_gpio13_base + (0x08<<2), 0x08)
#define _1wire_reset()         REG_BIT_RESET(_virt_gpio13_base + (0x08<<2), 0x08)
#define _1wire_get_value()     ((REG_READ(_virt_gpio13_base + (0x08<<2)) & 0x08)?1:0)

static unsigned int am2120_reset(void)
{
    bool is_tmout = false;
    struct timeval tv,tv2,tv3={0,0};
    
    _1wire_config_output();

    _1wire_set(); 
    udelay(10);

    _1wire_reset();
    udelay(1000); // 1ms

    _1wire_set();
    udelay(15);
    _1wire_config_input();

    /*
     * Trel 响应低电平时间测量
    */
    do_gettimeofday(&tv);
    do_gettimeofday(&tv2);
    while((_1wire_get_value()==0) && !(is_tmout = ((tv3.tv_usec = (tv2.tv_usec-tv.tv_usec))>85)?true:false)){
	do_gettimeofday(&tv2);
    }
    // Trel 响应低电平时间(打印可能影响时序检测，单独测试该电平方可开启此打印)
    // printk(KERN_INFO "reset: bit low level tv3.tv_usec(%ld)us is_tmout(%d)\n",tv3.tv_usec,is_tmout);
    
    // 判断Trel 响应低电平时间是否在约定范围
    if((tv3.tv_usec<75) || (tv3.tv_usec>85)) return -1;


    /*
     * Treh 响应高电平时间测量
    */
    is_tmout = false;
    tv3.tv_usec = 0;
    do_gettimeofday(&tv);
    do_gettimeofday(&tv2);
    while((_1wire_get_value()==1) && !(is_tmout = ((tv3.tv_usec = (tv2.tv_usec-tv.tv_usec))>85)?true:false)){
        do_gettimeofday(&tv2);
    }
    // Treh 响应高电平时间(打印可能影响时序检测，单独测试该电平方可开启此打印)
    //printk(KERN_INFO "reset: bit high level tv3.tv_usec(%ld)us is_tmout(%d)\n",tv3.tv_usec,is_tmout);

    // 判断Treh 响应高电平时间是否在约定范围
    if((tv3.tv_usec<75) || (tv3.tv_usec>85)) return -1;

    return 0;
}

static unsigned char am2120_read_byte(void){
    bool is_tmout = false;
    struct timeval tv,tv2,tv3={0,0};
    int i = 0;
    unsigned char data = 0;    

    for(i=0; i<8 ;i++){
        /*
         * bit位低电平时间测量
        */
        is_tmout = false;
        tv3.tv_usec = 0;
        do_gettimeofday(&tv);
        do_gettimeofday(&tv2);
        while((_1wire_get_value()==0) && !(is_tmout = ((tv3.tv_usec = (tv2.tv_usec-tv.tv_usec))>55)?true:false)){
            do_gettimeofday(&tv2);
        }

        // 判断bit位低电平时间是否在约定范围
        if((tv3.tv_usec<48) || (tv3.tv_usec>55)) {
            printk(KERN_INFO "err : bit low level, i(%d), tv3.tv_usec(%ld) \n",i,tv3.tv_usec);
	    data = 0;
	    break;
	}


        /*
         * bit位高电平时间测量
        */
        is_tmout = false;
        tv3.tv_usec = 0;
        do_gettimeofday(&tv);
        do_gettimeofday(&tv2);
        while((_1wire_get_value()==1) && !(is_tmout = ((tv3.tv_usec = (tv2.tv_usec-tv.tv_usec))>75)?true:false)){
            do_gettimeofday(&tv2);
        }

        // 判断bit位低电平时间是否在约定范围
	if((tv3.tv_usec>=22) && (tv3.tv_usec<=30)){
	    // bit0
	    data <<= 1;
	}else if((tv3.tv_usec>=68) && (tv3.tv_usec<75)){
	    // bit1
	    data <<= 1;
	    data |= 1;
	}else{
            printk(KERN_INFO "err : bit low level, i(%d), tv3.tv_usec(%ld) \n",i,tv3.tv_usec);
	    data = 0;
	    break;
	}
    }

    return data;
}

static ssize_t am2120_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    int ret =0,i;
    
    if( (ret = am2120_reset()) < 0){
        printk(KERN_INFO "reset fault ret(%d) \n",ret);
        return -1;
    }
   
    for(i=0; i<5; i++){
	buf[i] = am2120_read_byte();
    }

    if((buf[0]+buf[1]+buf[2]+buf[3]) == buf[4]){
        printk(KERN_INFO "read suc data(%02X %02X %02X %02X %02X) \n",buf[0],buf[1],buf[2],buf[3],buf[4]);
        return (count>5)?5:count;
    }else{
        printk(KERN_INFO "read err data(%02X %02X %02X %02X %02X) \n",buf[0],buf[1],buf[2],buf[3],buf[4]);
        return 0;
    }
}

static int am2120_open(struct inode *inode, struct file *filep){
    // 重映射GPIO复用寄存器基地址
    if(( _virt_gpio_mux_base = (unsigned int)ioremap_nocache(GPIO_MUX_BASE(0),512)) == 0){
    	printk(KERN_INFO "0x120f0000 ioremap failed \n");
    }

    // 重映射GPIO13基地址
    if(( _virt_gpio13_base = (unsigned int)ioremap_nocache(GPIO_NUM_BASE(13),0x10000)) == 0){
    	printk(KERN_INFO "0x12220000 ioremap failed \n");
    }
    REG_WRITE(_virt_gpio_mux_base + 0x1bc, 0x00); // 0x1bc 是GPIO13_3的复用地址
    REG_BIT_SET(_virt_gpio13_base + 0x400, 0x08); // 配置GPIO13_3方向(output)

    printk(KERN_INFO "am2120_open()\n");
    return 0;
}

static int am2120_release(struct inode *inode, struct file *filep){
        printk(KERN_INFO "am2120_release()\n");
	return 0;
}

static struct file_operations am2120_ops =
{
	.owner   = THIS_MODULE,
        .open    = am2120_open,
	.read	 = am2120_read,
        .release = am2120_release, //当执行close()函数时会调用release()
};

static int __init  am2120_init(void){
    int ret;
    if(major){ //静态申请方法
        devno = MKDEV(major, minor);
        ret = register_chrdev(major,DEVICE_NAME, &am2120_ops);
    }else{ //动态申请方法
        ret = alloc_chrdev_region(&devno,minor,DEVICE_MINOR_NUM,DEVICE_NAME); //这个好像还没有添加file_operation
        major = MAJOR(devno);
        minor = MINOR(devno);
        ret = register_chrdev(major,DEVICE_NAME, &am2120_ops);
        printk(KERN_INFO "alloc region %d \n",major);
    }

    cls = class_create(THIS_MODULE, "am2120_dev"); //创建类
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
        printk(KERN_ERR "create /dev/am2120 failed \n");
        return -EBUSY;
    }
    printk(KERN_INFO "create /dev/am2120 successs \n");
    printk(KERN_WARNING "[%s %s %d] minor = %u\n", __FILE__, __func__, __LINE__,0xFF);
    return 0;
}

static void __exit am2120_exit(void){
    iounmap((void*)_virt_gpio_mux_base);
    iounmap((void*)_virt_gpio13_base);
    printk(KERN_WARNING "[%s %s %d] minor = %u\n", __FILE__, __func__, __LINE__,0xFF);
}

module_init(am2120_init);
module_exit(am2120_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("wgco");
