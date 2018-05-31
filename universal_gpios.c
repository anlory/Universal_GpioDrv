#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/err.h>
#include <linux/version.h>   
#include <linux/proc_fs.h>   
#include <linux/fb.h>
#include <linux/rk_fb.h>
#include <linux/display-sys.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <dt-bindings/rkfb/rk_fb.h>
#endif

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/string.h>


#define DEBUG_XD
#ifdef DEBUG_XD
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

static dev_t igpiosDevNum;
static struct cdev *ptgpiosCdev;
static struct class * ptgpiosCls;
static struct device * ptgpiosDevFile;
static struct device *g_dev;
struct GpioDesc {
	const char *pcName;
	int  iGpio;
	int  iEnableLevel;
	int  iDefaultStatus;
	int  iPowerDownStatus;
};

struct GpioDescGrp{
	int iGpioNum;
	const char *pcName;
	struct kobj_attribute * gpio_attr;
	struct attribute_group attr_group;
	struct GpioDesc *tGpioDesc;
};
struct xd_gpios_info {
	int iGpioGrpNum;
	struct mutex tMutexLock;
	struct GpioDescGrp *tGpioDescGrp;
};

static struct xd_gpios_info *ptXdgpiosData = NULL;


static struct kobject *k_obj = NULL;



ssize_t xd_gpios_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	
	int i,j;
	int gpio_val = -1;
	struct GpioDesc * tmp_gpioDesc = NULL;
	DBG("%s %d\n",__FUNCTION__,__LINE__);

	DBG("Node:%s \n",attr->attr.name);

	for(i = 0; i < ptXdgpiosData->iGpioGrpNum; ++i)
	{
		for( j = 0; j < ptXdgpiosData->tGpioDescGrp[i].iGpioNum; ++j)
		{
				tmp_gpioDesc = &ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j];
				if( 0 == strcmp(tmp_gpioDesc->pcName,attr->attr.name)){
					gpio_val = gpio_get_value(tmp_gpioDesc->iGpio);
					break;
				}

		}
		if(gpio_val != -1)
			break;
	}
	return sprintf(buf, "%s IO Val:%d\n",attr->attr.name,gpio_val );


}
ssize_t xd_gpios_store(struct kobject *kobj, struct kobj_attribute *attr,
		 const char *buf, size_t count)
{
	int i,j;
	int gpio_val = -1;
	int ret;
	struct GpioDesc * tmp_gpioDesc = NULL;
	DBG("%s %d\n",__FUNCTION__,__LINE__);
	sscanf(buf,"%d",&gpio_val);
	DBG("%s will set gpio val: %d\n",attr->attr.name,gpio_val);
	for(i = 0; i < ptXdgpiosData->iGpioGrpNum; ++i)
	{
		for( j = 0; j < ptXdgpiosData->tGpioDescGrp[i].iGpioNum; ++j)
		{
				tmp_gpioDesc = &ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j];
				if( 0 == strcmp(tmp_gpioDesc->pcName,attr->attr.name)){
					gpio_set_value(tmp_gpioDesc->iGpio,!!gpio_val);
					
					DBG("%s seted to %d !!!\n",tmp_gpioDesc->pcName,gpio_val);
					break;
				}
		}
	}
	return count;

}
		 



static int  parse_gpios_to_attr(void){
	int i,j;
	struct GpioDescGrp *tmp_grp = NULL;

	
	DBG("%s %d\n",__FUNCTION__,__LINE__);
	if ((k_obj = kobject_create_and_add("xd_gpios", NULL)) == NULL ) {
        DBG("xd_gpios sys node create error \n");
		return -1;
    }


	for(i = 0; i < ptXdgpiosData->iGpioGrpNum; ++i)
	{
		
		tmp_grp = &ptXdgpiosData->tGpioDescGrp[i];
		tmp_grp->attr_group.name = tmp_grp->pcName ;
		tmp_grp->attr_group.attrs = devm_kzalloc(g_dev, (tmp_grp->iGpioNum + 1)* sizeof(struct attribute *),
			     GFP_KERNEL);
		if(NULL == tmp_grp->attr_group.attrs){
			DBG("devm_kzalloc Error %d\n",__LINE__);
			return -1;
		}
		tmp_grp->attr_group.attrs[tmp_grp->iGpioNum] = NULL;
		tmp_grp->gpio_attr = devm_kzalloc(g_dev, (tmp_grp->iGpioNum)* sizeof(struct kobj_attribute),
			     GFP_KERNEL);
		if(NULL == tmp_grp->gpio_attr){
			DBG("devm_kzalloc Error %d\n",__LINE__);
			return -1;
		}
		j = 0;
		for(j = 0; j < ptXdgpiosData->tGpioDescGrp[i].iGpioNum; ++j)
		{
			tmp_grp->gpio_attr[j].attr.name = tmp_grp->tGpioDesc[j].pcName;
			tmp_grp->gpio_attr[j].attr.mode = 0x777 ;
			tmp_grp->gpio_attr[j].show = xd_gpios_show;
			tmp_grp->gpio_attr[j].store = xd_gpios_store;
			
			tmp_grp->attr_group.attrs[j] = &(tmp_grp->gpio_attr[j].attr);
			
			DBG("Looper: %s %d\n",tmp_grp->gpio_attr[j].attr.name, __LINE__);
			
			DBG("Looper: %s %d\n",tmp_grp->attr_group.attrs[j]->name, __LINE__);
		}

		if(sysfs_create_group(k_obj, &(tmp_grp->attr_group))) {
	        DBG("sysfs_create_group failed\n");
	        return -1;
    	}

	}
	return 0;
}

static int xd_gpios_probe(struct platform_device *pdev)
{
    int iRet = -1;
    int i = 0;
	int j = 0;
    int iFlags = 0;
    struct device *dev = &pdev->dev;
	struct device_node *ptgpiosNode = pdev->dev.of_node;
	struct device_node *ptgpiosChildNode;
	struct device_node *ptgpiosChildNode1;

	int iGpioGrpNum = of_get_child_count(ptgpiosNode);
	g_dev = dev;
	if (iGpioGrpNum == 0)
		DBG("no gpio defined\n");
	ptXdgpiosData = devm_kzalloc(dev, sizeof(struct xd_gpios_info),
			     GFP_KERNEL);
	if (!ptXdgpiosData) {
		iRet = -ENOMEM;
		goto Faigpios;
	}
	ptXdgpiosData->tGpioDescGrp = devm_kzalloc(dev, iGpioGrpNum * sizeof(struct GpioDescGrp),
			     GFP_KERNEL);
	if (!ptXdgpiosData->tGpioDescGrp) {
		iRet = -ENOMEM;
		devm_kfree(dev,ptXdgpiosData);
		goto Faigpios;
	}
	memset(ptXdgpiosData, 0, sizeof(ptXdgpiosData));
	ptXdgpiosData->iGpioGrpNum = iGpioGrpNum; 
	for_each_child_of_node(ptgpiosNode, ptgpiosChildNode)
	{
		ptXdgpiosData->tGpioDescGrp[i].pcName = of_get_property(ptgpiosChildNode, "label", NULL);
		ptXdgpiosData->tGpioDescGrp[i].iGpioNum = of_get_child_count(ptgpiosChildNode);
		DBG("dingxb GpioGrp %d: Name:%s  Num %d\n",i,ptXdgpiosData->tGpioDescGrp[i].pcName,ptXdgpiosData->tGpioDescGrp[i].iGpioNum);
		
		
		ptXdgpiosData->tGpioDescGrp[i].tGpioDesc = devm_kzalloc(dev, ptXdgpiosData->tGpioDescGrp[i].iGpioNum * sizeof(struct GpioDesc),
			     GFP_KERNEL);
		j = 0;
		for_each_child_of_node(ptgpiosChildNode, ptgpiosChildNode1)
		{
			ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName = of_get_property(ptgpiosChildNode1, "label", NULL);
			if(of_property_read_u32(ptgpiosChildNode1, "default", &(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iDefaultStatus)))
			{
				DBG("fail to get default status, default set 0\n");
				ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iDefaultStatus = 0;
			}
			if(of_property_read_u32(ptgpiosChildNode1, "power_down_status", &(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iPowerDownStatus)))
			{
				DBG("fail to get power down status, default set 0\n");
				ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iPowerDownStatus = 0;
			}
			ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio = of_get_gpio_flags(ptgpiosChildNode1, 0, ( enum of_gpio_flags *)&iFlags);
			if (ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio < 0) 
			{
				iRet = ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio;
				if (iRet != -EPROBE_DEFER)
					DBG("Faigpios to get %s gpio flags, error: %d\n", ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName, iRet);
				goto Faigpios0;
			}
			else
			{
				DBG("get %s gpio flags\n", ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName);
			}
			ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iEnableLevel = (iFlags == OF_GPIO_ACTIVE_LOW)? 0:1;
			
			iRet = gpio_request(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio, ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName);
			if (iRet != 0) {
				int k;
				//DBG("%s request gpio faigpios\r\n",ptXdgpiosData->tGpioDesc[i].pcName);			
				for(k = j - 1; k >= 0; k--)
				{
					gpio_free(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[k].iGpio);
				}			
				goto Faigpios0;
				
			}
			gpio_direction_output(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio, 
					   ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iDefaultStatus);
			DBG("%s  %d\r\n",ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName,gpio_get_value(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio));
			j++;
		}
		
		i++;
	}

	platform_set_drvdata(pdev, (void *)ptXdgpiosData);


	if (0 != parse_gpios_to_attr())
		goto Faigpios0;
	
	return 0;  //return Ok
Faigpios0:
	devm_kfree(dev,ptXdgpiosData);
Faigpios:	
	return iRet;
}


static int xd_gpios_remove(struct platform_device *pdev)
{ 
 	int i;
 	struct device *dev = &pdev->dev;
/*
	for(i = 0; i < ptXdgpiosData->iGpioNum; i++)
	{
		gpio_free(ptXdgpiosData->tGpioDesc[i].iGpio);
	}
	devm_kfree(dev,ptXdgpiosData);

	if (k_obj) {
        if (usb_obj) {
            sysfs_remove_group(usb_obj, &sysfs_usb_attr_group);
            kobject_put(usb_obj);
        }
        kobject_put(k_obj);
    }
*/		
	DBG("%s %d\n",__FUNCTION__,__LINE__);
    return 0;
}

static void xd_gpios_shutdown(struct platform_device *pdev)
{
	int i,j;
	
	for(i = 0; i < ptXdgpiosData->iGpioGrpNum; i++)
		for(j = 0; j < ptXdgpiosData->tGpioDescGrp[i].iGpioNum; j++)
		{
			gpio_set_value(ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio, 
					   ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iPowerDownStatus);//RFID_PDOWN
			DBG("%s  %d\r\n",ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].pcName,ptXdgpiosData->tGpioDescGrp[i].tGpioDesc[j].iGpio);
		}
	return;
}


#ifdef CONFIG_OF
static const struct of_device_id of_rk_xd_gpios_match[] = {
	{ .compatible = "xd,gpios" },
	{ /* Sentinel */ }
};
#endif

static struct platform_driver xd_gpios_driver = {
	.probe		= xd_gpios_probe,
	.remove		= xd_gpios_remove,
	.shutdown   = xd_gpios_shutdown,
	.driver		= {
		.name	= "xd,gpios",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_rk_xd_gpios_match,
#endif
	},

};


static int __init xd_gpios_init(void)
{
    DBG("Enter %s\n", __FUNCTION__);
    return platform_driver_register(&xd_gpios_driver);
}

static void __exit xd_gpios_exit(void)
{
	platform_driver_unregister(&xd_gpios_driver);
    DBG("Exit %s\n", __FUNCTION__);
}

module_init(xd_gpios_init);
module_exit(xd_gpios_exit);

MODULE_DESCRIPTION("xd power ctl driver");
MODULE_LICENSE("GPL");


