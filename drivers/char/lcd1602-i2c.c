#include "lcd1602-i2c.h"

#define WR_VALUE    _IOW('l','w',char*)
#define RD_VALUE    _IOR('l','r',char*)
#define CLEAR       _IO('l','c')
#define BACKSPACE   _IO('l','b')
#define ENTER       _IO('l','e')

char c = 0;
dev_t dev = 0;
static struct class *dev_class;
static struct cdev lcd1602_cdev;

int backspace = 0;
/*
** Function Prototypes
*/
static int      lcd1602_open(struct inode *inode, struct file *file);
static int      lcd1602_release(struct inode *inode, struct file *file);
static ssize_t  lcd1602_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  lcd1602_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long     lcd1602_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/*
** File operation sturcture
*/
static struct file_operations fops =
{
    .owner          = THIS_MODULE,
    .read           = lcd1602_read,
    .write          = lcd1602_write,
    .open           = lcd1602_open,
    .unlocked_ioctl = lcd1602_ioctl,
    .release        = lcd1602_release,
};
/*
** This function will be called when we open the Device file
*/
static int lcd1602_open(struct inode *inode, struct file *file)
{
    dev_info(&lcd1602_client->dev, "Device File Opened...!!!\n");
    return 0;
}
/*
** This function will be called when we close the Device file
*/
static int lcd1602_release(struct inode *inode, struct file *file)
{
    dev_info(&lcd1602_client->dev, "Device File Closed...!!!\n");
    return 0;
}
/*
** This function will be called when we "cat" the Device file
*/
static ssize_t lcd1602_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    size_t len_str = strlen(lcd_str);

    // Check if at the end of the string
    if (*off >= len_str) {
        return 0;
    }

    if(len_str > len) {
        len_str = len;
    }

    if (copy_to_user(buf, lcd_str, len_str)) {
        return -EFAULT;
    }

    *off += len_str;

    return len_str;
}
/*
** This function will be called when we "echo" the Device file
*/
static ssize_t lcd1602_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    // char *kbuf;
    int ret;

    // Copy data from user space to kernel space
    if (copy_from_user(lcd_str, buf, len)) {
        return -EFAULT;
    }

    lcd_str[len-1] = '\0';

    ret = lcd1602_print(lcd_str);

    if (ret)
        return ret;

    return len;
}
/*
** This function will be called when we write IOCTL on the Device file
*/
static long lcd1602_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;
    
    switch(cmd) {
        case WR_VALUE:
            if( copy_from_user(&c ,(char*) arg, sizeof(c)) )
            {
                pr_err("Data Write : Err!\n");
            }

            if(c >= 0 && c <= 0x1F){
                dev_info(&lcd1602_client->dev, "Unexpected character: ASCII number %d\n", c);
                break;
            }
            
            ret = lcd1602_data(c);
            if(ret)
                return ret;
            dev_info(&lcd1602_client->dev, "Character = %c\n", c);

            lcd_str[pos] = c;

            if(++pos > 31)
                pos = 31;

            if(pos == 16)
                lcd1602_gotoXY(2,0);
            break;

        case RD_VALUE:
            if( copy_to_user((int32_t*) arg, &c, sizeof(c)) )
            {
                pr_err("Data Read : Err!\n");
            }
            break;

        case CLEAR:
            dev_info(&lcd1602_client->dev, "Clear display\n");
            lcd1602_clear();
            memset(lcd_str, 0, sizeof(lcd_str));
            pos = 0;
            break;

        case BACKSPACE:
            backspace++;
            if(--pos < 0){
                lcd1602_clear();
                memset(lcd_str, 0, sizeof(lcd_str));
                pos = 0;
                break;
            }
            
            lcd_str[pos] = '\0';

            ret = lcd1602_print(lcd_str);

            if(ret)
                return ret;
            break;
        
        case ENTER:
            if(backspace){
                backspace = 0;
                break;
            }
            if(pos < 16){
                while(pos < 16){
                    lcd_str[pos] = 32;
                    pos++;
                }
                lcd1602_gotoXY(2,0);
            }
            break;

        default:
            dev_info(&lcd1602_client->dev, "Do nothing\n");
            break;
    }

    return 0;
}

static int lcd1602_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    lcd1602_client = client;
    lcd1602_init();

    lcd1602_print("Hello");

    /*Allocating Major number*/
    if((alloc_chrdev_region(&dev, 0, 1, "lcd1602_Dev")) <0){
        pr_err("Cannot allocate major number\n");
        return -1;
    }
    dev_info(&lcd1602_client->dev, "Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

    /*Creating cdev structure*/
    cdev_init(&lcd1602_cdev,&fops);

    /*Adding character device to the system*/
    if((cdev_add(&lcd1602_cdev,dev,1)) < 0){
        pr_err("Cannot add the device to the system\n");
        goto r_class;
    }

    /*Creating struct class*/
    if(IS_ERR(dev_class = class_create(THIS_MODULE,"lcd1602_class"))){
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    /*Creating device*/
    if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"lcd1602_device"))){
        pr_err("Cannot create the Device 1\n");
        goto r_device;
    }
    dev_info(&lcd1602_client->dev, "LCD1602 Driver Insert ... Done\n");
    return 0;
 
r_device:
    class_destroy(dev_class);
r_class:
    unregister_chrdev_region(dev,1);
    return -1;
}

static int lcd1602_remove(struct i2c_client *client)
{
    device_destroy(dev_class,dev);
    class_destroy(dev_class);
    cdev_del(&lcd1602_cdev);
    unregister_chrdev_region(dev, 1);

    dev_info(&lcd1602_client->dev, "LCD1602 Driver Remove ... Done\n");
    return 0;
}

static const struct of_device_id lcd1602_of_match[] = {
    { .compatible = "hd4478,lcd1602" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lcd1602_of_match);

static struct i2c_driver lcd1602_driver = {
    .driver = {
        .name = "lcd1602",
        .of_match_table = of_match_ptr(lcd1602_of_match),
    },
    .probe = lcd1602_probe,
    .remove = lcd1602_remove,
};

module_i2c_driver(lcd1602_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Duc Lee");
MODULE_DESCRIPTION("I2C driver for LCD1602");
