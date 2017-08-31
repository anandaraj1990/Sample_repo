#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>


#define GPIO_HIGH		1
#define GPIO_LOW		0

#define ILI9488_LCD_CS		(128)	/* chip select: P8_15*/
#define ILI9488_LCD_SDI_A	(129)	/* SDI/SDA: P9_0, bidirectional*/
#define ILI9488_LCD_SDO		(130)	/* SDO: P9_1, input*/
#define ILI9488_LCD_SCK		(127)	/* Clock : P8_14*/
#define ILI9488_LCD_D_CX	(80)	/* Command/Data: P5_10*/



struct LCD_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

#if 1
#define DATA_EMPTY	0x00
#define DELAY		0x78
#define END_LCD_CONFIG	0x87
static struct LCD_setting_table lcd_initialization_setting[] = {
	{0x01,
	DATA_EMPTY, {}},
	{DELAY,
	30, {}},
	{0xE0,
	15,{0x00, 0x13, 0x18, 0x04, 0x0F, 0x06, 0x3A, 0x56, 0x4D, 0x03, 0x0A, 0x06, 0x30, 0x3E, 0x0F}},
	{0xE1, // N-gamma,
	15, {0x00, 0x13, 0x18, 0x01, 0x11, 0x06, 0x38, 0x34, 0x4d, 0x06, 0x0D, 0x0B, 0x31, 0x37, 0x0F}},
	{0xC0, // Power control1
	2, {0x18, 0x17}},
	{0xC1, // Power control2
	1, {0x44}},
	{0xC5, // Power control3
	3, {0x00, 0x6A, 0x80}},
	{0x36, // Memory Access
	1, {0x08}},
	{0x3A, // Interface Pixel Format
	//1, {0x11}},//--->spi data display
	1, {0x55}},//--->16 bpp
	//1, {0x66}},//--->18 bpp
	//{0xB0, // Interface Mode control
	//1, {0x8C}},
	{0xB1, // Frame
	1, {0xc0}},
	//{0xB4, // Display inversion control
	//1, {0x02}},
	//{0xB5, // HSYNC_VSYNC
	//4, {0x02, 0x02, 0xA0, 0x3C}},//VFP,VBP,HFP,HBP
	//4, {0x02, 0x02, 0xC8, 0x64}},//VFP,VBP,HFP,HBP
	//{0xB5, // HSYNC_VSYNC
	//4, {0x08, 0x08, 0x14, 0xff}},
	{0xB6, // RGB/MCU Iterface control
	3, {0x32, 0x22, 0x3B}},
	{0xE9, //Set image function
	1, {0x00}},
	{0xF7, // Adjust control
	4, {0xA9, 0x51, 0x2C, 0x82}},
	//{0x21, // Normal Black __inversion
	//DATA_EMPTY, {}},
	{0x11, // sleep out
	DATA_EMPTY, {}},
	{DELAY,
	50, {}},	
	{0x29, // Dislay on 
	0, {}},
	{DELAY,
	20, {}},
	{END_LCD_CONFIG,
	DATA_EMPTY, {}},
};
#else
#endif

struct spi_t {
        unsigned int cs_pin;
        unsigned int sck_pin;
        unsigned int sdo_pin;
        unsigned int sdi_a_pin;
	unsigned int d_cx_pin;
};

struct ili9488_chip {
        struct spi_t spi;
        struct device *dev;

/*DEBUG*/
        struct class *ili_class;
        struct device *ili_dev;
};

/**************************************************************************/
#define LCD_CS_PIN	chip->spi.cs_pin
#define LCD_SCK_PIN	chip->spi.sck_pin
#define LCD_SDI_A_PIN	chip->spi.sdi_a_pin
#define LCD_SDO_PIN	chip->spi.sdo_pin
#define LCD_D_CX_PIN	chip->spi.d_cx_pin

#define CS_SET()	gpio_set_value(LCD_CS_PIN, GPIO_HIGH)
#define CS_CLR()        gpio_set_value(LCD_CS_PIN, GPIO_LOW)

#define CLK_SET()	gpio_set_value(LCD_SCK_PIN, GPIO_HIGH)
#define CLK_CLR()	gpio_set_value(LCD_SCK_PIN, GPIO_LOW)

#define SDI_A_SET()	gpio_set_value(LCD_SDI_A_PIN, GPIO_HIGH)
#define SDI_A_CLR()	gpio_set_value(LCD_SDI_A_PIN, GPIO_LOW)

#define D_CX_SET()	gpio_set_value(LCD_D_CX_PIN, GPIO_HIGH)
#define D_CX_CLR()	gpio_set_value(LCD_D_CX_PIN, GPIO_LOW)

#define SDO_GET()	gpio_get_value(LCD_SDO_PIN)

static int spi_send_command(struct ili9488_chip *chip, unsigned char cmd)
{
        int ret = 0;
	unsigned int i = 0;
	unsigned int msb_bit = 0x80;
	D_CX_SET();
	CLK_CLR();
	CS_CLR();
	for(i = 0; i <= 7; i++) {
        	udelay(20);
		if (i == 7)
			D_CX_CLR();

                if (cmd & (msb_bit >> i)) {
			SDI_A_SET();
                } else {
			SDI_A_CLR();
                }
		CLK_SET();
                udelay(20);
		CLK_CLR();
	}
	
	D_CX_SET();
	SDI_A_SET();
	CLK_CLR();
	CS_SET();
	
        return ret;
}

static int spi_send_command_data(struct ili9488_chip *chip, unsigned char cmd, unsigned char *ptr_data, unsigned int len)
{
        int ret = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int msb_bit = 0x80;
	unsigned char data = 0;

	if (cmd == DELAY) {
		printk(KERN_ALERT"delay...\n");
		msleep(len);
		return 0;
	}

	D_CX_SET();
	CLK_CLR();
	CS_CLR();
	for(i = 0; i <= 7; i++) {
        	udelay(20);
		if (i == 7)
			D_CX_CLR();

                if (cmd & (msb_bit >> i)) {
			SDI_A_SET();
                } else {
			SDI_A_CLR();
                }
		CLK_SET();
                udelay(20);
		CLK_CLR();
	}
	
	D_CX_SET();

	if (len == DATA_EMPTY) {
		SDI_A_SET();
		CLK_CLR();
		CS_SET();
		msleep(20);
		return 0;
	} else {

	}
	for (j = 0; j < len; j++) {
		data = ptr_data[j];
		for (i = 0; i <= 7; i++) {
			udelay(20);
			if (data & (msb_bit >> i)) {
				SDI_A_SET();
	                } else {
				SDI_A_CLR();
                	}
			CLK_SET();
			udelay(20);
			CLK_CLR();
		}
		udelay(10);
	}
	SDI_A_SET();
	CLK_CLR();
	CS_SET();
        return ret;
}

static int spi_recv_data(struct ili9488_chip *chip, unsigned char cmd)
{

	unsigned int msb_bit = 0x80;
	unsigned char read_data[4] = {0};
	unsigned int temp = 0;
	unsigned int i = 0;
	unsigned len = 0;
	unsigned char omit_first_clk = 0;

	D_CX_SET();
	CLK_CLR();
	CS_CLR();
	for(i = 0; i <= 7; i++) {
        	udelay(20);
		if (i == 7)
			D_CX_CLR();

                if (cmd & (msb_bit >> i)) {
			SDI_A_SET();
                } else {
			SDI_A_CLR();
                }
		CLK_SET();
                udelay(20);
		CLK_CLR();
	}
	
	D_CX_SET();
	SDI_A_SET();
	udelay(20);

	switch (cmd) {
	case 0x09://RDDST
		len = 32;
		omit_first_clk = 1;
		break;
	case 0x4://RDDID
		len = 24;
		omit_first_clk = 1;
		break;
	default://RDID2/RDID3/0Ah/0Bh/0Ch/0Dh/0Eh/0Fh
		len = 31;
		omit_first_clk = 0;
		break;
	};
	
        for ( i = 0; i <= len; i++) {
		temp = temp << 1;
		CLK_CLR();
                udelay(20);
		CLK_SET();
		if (omit_first_clk) {
			omit_first_clk--;
			continue;
		}
		temp |= SDO_GET();
                udelay(20);
        }

	CLK_CLR();
	CS_SET();

	read_data[0] = (temp >> 24) & 0xFF;
	read_data[1] = (temp >> 16) & 0xFF;
	read_data[2] = (temp >> 8) & 0xFF;
	read_data[3] = (temp) & 0xFF;


	return 0;
}

static int spi_display_video(struct ili9488_chip *chip, unsigned char disp_test)
{
        int ret = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int msb_bit = 0x80;
	D_CX_SET();
	CLK_CLR();
	CS_CLR();
	for (j = 0; j <= (320*480); j += 2) {
		for(i = 0; i <= 7; i++) {
        		udelay(20);
                	if (disp_test & (msb_bit >> i)) {
				SDI_A_SET();
	                } else {
				SDI_A_CLR();
                	}
			CLK_SET();
        	        udelay(20);
			CLK_CLR();
		}
	}
	
	D_CX_SET();
	SDI_A_SET();
	CLK_CLR();
	CS_SET();
	
        return ret;
}

/**************************************************************************/

static void
cmd_parsing(const char *buf, unsigned short cnt, unsigned short *data)
{

        char **bp = (char **)&buf;
        char *token, minus, parsing_cnt = 0;
        int val;
        int pos;

        while ((token = strsep(bp, " "))) {
                pos = 0;
                minus = false;
                if ((char)token[pos] == '-') {
                        minus = true;
                        pos++;
                }
                if ((token[pos] == '0') &&
                    (token[pos + 1] == 'x' || token[pos + 1] == 'X')) {
                        if (kstrtoul(&token[pos + 2], 16,
                                     (long unsigned int *)&val))
                                val = 0;
                        if (minus)
                                val *= (-1);
                } else {
                        if (kstrtoul(&token[pos], 10,
                                     (long unsigned int *)&val))
                                val = 0;
                        if (minus)
                                val *= (-1);
                }

                if (parsing_cnt < cnt)
                        *(data + parsing_cnt) = val;
                else
                        break;
                parsing_cnt++;
        }
}



static ssize_t
attr_recv_data_get(struct device *dev, struct device_attribute *attr, char *buf)
{

        return sprintf(buf, "TEST in progress\n");
}

static ssize_t
attr_recv_data_set(struct device *dev, struct device_attribute *attr,
                  const char *buf, size_t size)
{
        struct ili9488_chip *chip = dev_get_drvdata(dev);
        unsigned short cmd = 0;
        unsigned short parse_data[2] = {0};

        memset(parse_data, 0, sizeof(unsigned short) * 2);
        cmd_parsing(buf, 2, parse_data);
        cmd = parse_data[0];
        if (parse_data[1] != 0) {
                printk("Too many arguments\n");
                return -EINVAL;
        }

	/* call spi_recv_data*/
	spi_recv_data(chip, cmd);

	return size;
}

static ssize_t
attr_write_cmd_set(struct device *dev, struct device_attribute *attr,
                   const char *buf, size_t size)
{
        struct ili9488_chip *chip = dev_get_drvdata(dev);
        unsigned short parse_data[2] = {0};

        memset(parse_data, 0, sizeof(unsigned short) * 3);
        cmd_parsing(buf, 2, parse_data);
        if (parse_data[1] != 0) {
                printk("Too many arguments\n");
                return -EINVAL;
        }

	/* call send_cmd*/
	spi_send_command(chip, parse_data[0]);
        return size;
}

static ssize_t
attr_write_cmd_data_set(struct device *dev, struct device_attribute *attr,
                   const char *buf, size_t size)
{
        struct ili9488_chip *chip = dev_get_drvdata(dev);
        unsigned short parse_data[2] = {0};
	unsigned int i = 0;

        memset(parse_data, 0, sizeof(unsigned short) * 3);
        cmd_parsing(buf, 2, parse_data);
        if (parse_data[1] != 0) {
                printk("Too many arguments\n");
                return -EINVAL;
        }

	/* call send_cmd*/
	while (lcd_initialization_setting[i].cmd != END_LCD_CONFIG) {
		spi_send_command_data(chip,
					lcd_initialization_setting[i].cmd,
					lcd_initialization_setting[i].para_list,
					lcd_initialization_setting[i].count);
		i++;
	}

        CLK_SET();
        D_CX_SET();
        CS_CLR();
	
        return size;
}

static ssize_t
attr_write_disp_data_set(struct device *dev, struct device_attribute *attr,
                   const char *buf, size_t size)
{
        struct ili9488_chip *chip = dev_get_drvdata(dev);
        unsigned short parse_data[2] = {0};

        memset(parse_data, 0, sizeof(unsigned short) * 3);
        cmd_parsing(buf, 2, parse_data);
        if (parse_data[1] != 0) {
                printk("Too many arguments\n");
                return -EINVAL;
        }

	spi_display_video(chip, parse_data[0]);

	return size;
}

/**
array of attributes
*/
static struct device_attribute attributes[] = {
	__ATTR(recv_data,
		0664, attr_recv_data_get, attr_recv_data_set),
	__ATTR(write_cmd,
		0222, NULL, attr_write_cmd_set),
	__ATTR(write_config_data,
		0222, NULL, attr_write_cmd_data_set),
	__ATTR(write_display_data,
		0222, NULL, attr_write_disp_data_set),
};

/**
This function is used for creating sysfs attribute
@param de/v linux device structure
@return int status of attribute creation
*/
static int
create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		if (device_create_file(dev, attributes + i)) {
			goto err_sysfs_interface;
		}
	}
	return 0;

err_sysfs_interface:
	printk(KERN_ALERT"\tdevice create file error\n");
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}


/**
This function is used for removing sysfs attribute
@param dev linux device structure
@return void
*/
static void
remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}


static int ili9488_sysfs_init(struct ili9488_chip *chip) {
	if (!chip->ili_class) {
		chip->ili_class = class_create(THIS_MODULE, "ili9488_lcd");
                if (IS_ERR(chip->ili_class)) {
                        chip->ili_class = NULL;
                        return -1;
                }
	}

	if (!chip->ili_dev) {
		chip->ili_dev = device_create(chip->ili_class,
					      NULL,
					      0,
					      chip,
					      "%s",
					      "debug");
        }

	if (IS_ERR(chip->ili_dev)) {
		/*return PTR_ERR(chip->ili_dev);*/
		chip->ili_dev = NULL;
		return -1;
	}

	/*Create general sysfs attribute*/
        if (create_sysfs_interfaces(chip->ili_dev))
                goto err_sysfs_create_gen;

	dev_set_drvdata(chip->ili_dev, chip);

	return 0;
	err_sysfs_create_gen:
        return -1;

}

/**
This function is used for unregistering sysfs attribute for ILI9488
@param ili9488 chip pointer point ili9488_chip data structure
@return void
*/
static void
ili9488_sysfs_cleanup(struct ili9488_chip *chip)
{
        if (chip->ili_dev != NULL) {
                dev_set_drvdata(chip->ili_dev, NULL);
                remove_sysfs_interfaces(chip->ili_dev);
                chip->ili_dev = NULL;
        }

        if (chip->ili_class != NULL) {
                device_destroy(chip->ili_class, 0);
                class_destroy(chip->ili_class);
                chip->ili_class = NULL;
        }
}
/****************************************************/

static int ili9488_initialization(struct ili9488_chip *chip) {
	if (ili9488_sysfs_init(chip))
		goto ili_sysfs_init_err;

	return 0;
ili_sysfs_init_err:
	printk(KERN_ALERT"\tili9488: sysfs err\n");
	ili9488_sysfs_cleanup(chip);
	return -1;
}

static void ili9488_initialization_cleanup(struct ili9488_chip *chip) {
	ili9488_sysfs_cleanup(chip);
	return;
}

/* ILI9488 GPIO initialization
*/
static int ili9488_gpio_init(struct ili9488_chip *chip) {
	struct spi_t *spi = &chip->spi;

	if (gpio_request(spi->cs_pin, "ili9488_cs")) {
                printk("ILI9488: request for cs_pin failed\n");
                return 1;
        }
        if (gpio_request(spi->sck_pin, "ili9488_sck")) {
                gpio_free(spi->cs_pin);
                printk("ILI9488: request for sck_pin failed\n");
                return 1;
        }

        if (gpio_request(spi->sdi_a_pin, "ili9488_sdi")) {
                gpio_free(spi->cs_pin);
                gpio_free(spi->sck_pin);
                printk("ILI9488: request for sdi_a_pin failed\n");
                return 1;
        }

        if (gpio_request(spi->sdo_pin, "ili9488_sdo")) {
                gpio_free(spi->cs_pin);
                gpio_free(spi->sck_pin);
                gpio_free(spi->sdi_a_pin);
                printk("ILI9488: request for sdo_pin failed\n");
                return 1;
        }

	if (gpio_request(spi->d_cx_pin, "ili9488_d_cx")) {
		gpio_free(spi->sdo_pin);
                gpio_free(spi->cs_pin);
                gpio_free(spi->sck_pin);
                gpio_free(spi->sdi_a_pin);
                printk("ILI9488: request for sdo_pin failed\n");
                return 1;
	}

	gpio_direction_output(spi->cs_pin, GPIO_HIGH);
	gpio_direction_output(spi->sck_pin, GPIO_LOW);
        gpio_direction_output(spi->sdi_a_pin, GPIO_HIGH);
        gpio_direction_input(spi->sdo_pin);
        gpio_direction_output(spi->d_cx_pin, GPIO_HIGH);
        return 0;
}

/* ILI9488 GPIO Cleanup
*/
static void gpio_cleanup(struct ili9488_chip *chip) {
	struct spi_t *spi = &chip->spi;
	gpio_free(spi->d_cx_pin);
	gpio_free(spi->sdo_pin);
	gpio_free(spi->sdi_a_pin);
	gpio_free(spi->sck_pin);
	gpio_free(spi->cs_pin);
	return;
}

static void ili9488_write_config(struct ili9488_chip *chip) {
	int i = 0;
/* call send_cmd*/
        while (lcd_initialization_setting[i].cmd != END_LCD_CONFIG) {
                spi_send_command_data(chip,
                                        lcd_initialization_setting[i].cmd,
                                        lcd_initialization_setting[i].para_list,
                                        lcd_initialization_setting[i].count);
                i++;
        }

	return;
}

static int __init ili9488_probe(struct platform_device *pdev)
{
	struct ili9488_chip *chip = NULL;
	int ret = 0;


	chip = (struct ili9488_chip *)kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_dbg(&pdev->dev,"NO MEMORY\n");
		ret = -ENOMEM;
		goto ili_probe_mem_err;
	}

	chip->spi.sck_pin	= ILI9488_LCD_SCK;
	chip->spi.cs_pin	= ILI9488_LCD_CS;
	chip->spi.sdi_a_pin	= ILI9488_LCD_SDI_A;
	chip->spi.sdo_pin	= ILI9488_LCD_SDO;
	chip->spi.d_cx_pin	= ILI9488_LCD_D_CX;

	ili9488_gpio_init(chip);

	dev_set_drvdata(&pdev->dev, chip);

	ili9488_initialization(chip);

	spi_recv_data(chip, 0xda);
	spi_recv_data(chip, 0x0e);

	spi_recv_data(chip, 0x09);
	spi_send_command(chip, 0x29);
	msleep(20);

	spi_recv_data(chip, 0x09);
	spi_send_command(chip, 0x28);
	msleep(20);
	spi_send_command(chip, 0x01);
	msleep(20);
	
	ili9488_write_config(chip);

	spi_recv_data(chip, 0x09);
	spi_recv_data(chip, 0x0e);

        CLK_SET();
	D_CX_SET();
        CS_CLR();

	return ret;
ili_probe_mem_err:
	return ret;
}

static int __exit ili9488_remove(struct platform_device *pdev)
{
	struct ili9488_chip *chip = dev_get_drvdata(&pdev->dev);

	ili9488_initialization_cleanup(chip);
	gpio_cleanup(chip);

	kfree(chip);
	return 0;
}


static struct platform_device_id lcd_id_table[] = {
        { "ili9488", 0 },
        { }
};
MODULE_DEVICE_TABLE(platform, lcd_id_table);


static struct platform_driver ili9488_driver = {
	.probe          = ili9488_probe,
	.remove         = ili9488_remove,
	.driver = {
		.name = "ili9488",
		.owner  = THIS_MODULE,
        },
};

static int __init ili9488_init(void)
{
	platform_driver_register(&ili9488_driver);
	return 0;
}

static void __exit ili9488_exit(void)
{
	platform_driver_unregister(&ili9488_driver);
}

module_init(ili9488_init);
module_exit(ili9488_exit);

MODULE_DESCRIPTION("ili9488 Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("mobiveil");

