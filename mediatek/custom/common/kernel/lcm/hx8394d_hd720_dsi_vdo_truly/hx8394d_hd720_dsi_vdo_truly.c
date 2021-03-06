#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <mach/mt_gpio.h>
#include <linux/xlog.h>
#include <mach/mt_pm_ldo.h>
#endif
// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  			(720)
#define FRAME_HEIGHT 			(1280)
#define LCM_OTM1283_ID			(0x1283)

#define REGFLAG_DELAY          		(0xFE)
#define REGFLAG_END_OF_TABLE      	(0xFF)	// END OF REGISTERS MARKER

#ifndef FALSE
#define FALSE 0
#endif
#define GPIO_LCD_RST_EN      (GPIO131)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)    		(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 			(lcm_util.udelay(n))
#define MDELAY(n) 			(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)        (lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update))
#define dsi_set_cmdq(pdata, queue_size, force_update)		(lcm_util.dsi_set_cmdq(pdata, queue_size, force_update))
#define wrtie_cmd(cmd)						(lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums)			(lcm_util.dsi_write_regs(addr, pdata, byte_nums))
#define read_reg						(lcm_util.dsi_read_reg())
#define read_reg_v2(cmd, buffer, buffer_size)   		(lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size))

static struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

//  0003:9FC0:
//  croc2008 for 4pda
static struct LCM_setting_table lcm_initialization_setting[] = {
	/*
	   Structure Format :
	   {DCS command, count of parameters, {parameter list}}
	   {REGFLAG_DELAY, milliseconds of time, {}},
	   ...
	   Setting ending by predefined flag
	   {REGFLAG_END_OF_TABLE, 0x00, {}}
	 */
	{0x00,1,{0x00}},
	{0xFF,3,{0x12,0x83,0x01}},	//EXTC=1
	{0x00,1,{0x80}},	        //Orise mode enable
	{0xFF,2,{0x12,0x83}},

	{0x00, 1, {0xC6}},			//debounce
	{0xB0, 1, {0x03}},

	{0x00, 1, {0xB9}},
	{0xB0, 1, {0x51}},

//-------------------- panel setting --------------------//
	{0x00,1,{0x80}},			//TCON Setting
	{0xC0,9,{0x00,0x64,0x00,0x10,0x10,0x00,0x64,0x10,0x10}},
        
	{0x00,1,{0x90}},			//Panel Timing Setting
	{0xC0,6,{0x00,0x5c,0x00,0x01,0x00,0x04}},
        
	{0x00,1,{0xB3}},			//Interval Scan Frame: 0 frame, column inversion
	{0xC0,2,{0x00,0x55}},
         
	{0x00,1,{0x81}},			//frame rate:60Hz
	{0xC1,1,{0x56}},

	{0x00,1,{0x82}},			//chopper
	{0xC4,1,{0x02}},			//source OP down current

	{0x00,1,{0x81}},			//source bias 0.75uA
	{0xC4,1,{0x86}},
         
	{0x00,1,{0x90}},			//clock delay for data latch 
	{0xC4,1,{0x49}},

//-------------------- power setting --------------------//
	{0x00,1,{0xA0}},			//dcdc setting
	{0xC4,14,{0x05,0x10,0x05,0x02,0x05,0x15,0x1A,0x05,0x10,0x05,0x02,0x05,0x15,0x1A}},

	{0x00,1,{0xB0}},			//clamp voltage setting
	{0xC4,2,{0x00,0x00}},
         
	{0x00,1,{0x91}},			//VGH=15V, VGL=-12V, pump ratio:VGH=6x, VGL=-5x
	{0xC5,2,{0xA6,0xC0}},

//-------------------- for Power IC ---------------------------------
	{0x00,1,{0x90}},			//Mode-3
	{0xF5,4,{0x02,0x11,0x02,0x11}},

	{0x00,1,{0x90}},			//2xVPNL, 1.5*=00, 2*=50, 3*=a0
	{0xC5,1,{0x50}},

	{0x00,1,{0x94}},			//Frequency
	{0xC5,1,{0x66}},

	{0x00,1,{0x95}},
	{0xC5,1,{0x66}},
        
	{0x00,1,{0xB0}},			//VDD_18V=1.6V, LVDSVDD=1.55V
	{0xC5,2,{0x04,0x38}},

	{0x00,1,{0xB4}},			//VGLO1/2 Pull low setting
	{0xC5,1,{0xC0}},			//d[7] vglo1 d[6] vglo2 => 0: pull vss, 1: pull vgl
        
	{0x00,1,{0xBB}},			//LVD voltage level setting
	{0xC5,1,{0x80}},
         
//-------------------- power on setting --------------------//
	{0x00,1,{0x80}},			//source blanking frame = black, defacult='30'
	{0xC6,1,{0x24}},
       
	{0x00,1,{0x94}},			//VCL on  	
	{0xF5,1,{0x02}},

	{0x00,1,{0xBA}},			//VSP on   	
	{0xF5,1,{0x03}},

	{0x00,1,{0xB2}},
	{0xF5,2,{0x11,0x15}},

	{0x00,1,{0xB4}},
	{0xF5,2,{0x14,0x15}},
	
	{0x00,1,{0xB6}},
	{0xF5,2,{0x12,0x15}},
	
	{0x00,1,{0xB8}},
	{0xF5,2,{0x14,0x15}},		

//  0003:AF80:
//-------------------- panel timing state control --------------------//
	{0x00,1,{0x80}},             //panel timing state control
	{0xCB,11,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0x90}},             //panel timing state control
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xA0}},             //panel timing state control
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xB0}},             //panel timing state control
	{0xCB,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xC0}},             //panel timing state control
	{0xCB,15,{0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x00,0x05,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xD0}},             //panel timing state control
	{0xCB,15,{0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05}},

	{0x00,1,{0xE0}},             //panel timing state control
	{0xCB,14,{0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00}},

	{0x00,1,{0xF0}},             //panel timing state control
	{0xCB,11,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},

//  0003:B400:
//-------------------- panel pad mapping control --------------------//
	{0x00,1,{0x80}},             //panel pad mapping control
	{0xCC,15,{0x02,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x29,0x2A,0x00,0x06,0x00,0x00,0x00,0x00}},

	{0x00,1,{0x90}},             //panel pad mapping control
	{0xCC,15,{0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x01,0x09,0x0B,0x0D,0x0F,0x11,0x13,0x29}},

	{0x00,1,{0xA0}},             //panel pad mapping control
	{0xCC,14,{0x2A,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x00}},

	{0x00,1,{0xB0}},             //panel pad mapping control
	{0xCC,15,{0x05,0x13,0x11,0x0F,0x0D,0x0B,0x09,0x29,0x2A,0x00,0x01,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xC0}},             //panel pad mapping control
	{0xCC,15,{0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x06,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x29}},

	{0x00,1,{0xD0}},             //panel pad mapping control
	{0xCC,14,{0x2A,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00}},

//  0003:B760:
//-------------------- panel timing setting --------------------//
	{0x00,1,{0x80}},             //panel VST setting
	{0xCE,12,{0x89,0x05,0x20,0x88,0x05,0x20,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0x90}},             //panel VEND setting
	{0xCE,14,{0x54,0xFC,0x10,0x54,0xFD,0x10,0x55,0x00,0x10,0x55,0x01,0x10,0x00,0x00}},

	{0x00,1,{0xA0}},             //panel CLKA1/2 setting
	{0xCE,14,{0x58,0x07,0x04,0xFC,0x00,0x10,0x00,0x58,0x06,0x04,0xFD,0x00,0x10,0x00}},

	{0x00,1,{0xB0}},             //panel CLKA3/4 setting
	{0xCE,14,{0x58,0x05,0x04,0xFE,0x00,0x10,0x00,0x58,0x04,0x04,0xFF,0x00,0x10,0x00}},

	{0x00,1,{0xC0}},             //panel CLKb1/2 setting
	{0xCE,14,{0x58,0x03,0x05,0x00,0x00,0x10,0x00,0x58,0x02,0x05,0x01,0x00,0x10,0x00}},

	{0x00,1,{0xD0}},             //panel CLKb3/4 setting
	{0xCE,14,{0x58,0x01,0x05,0x02,0x00,0x10,0x00,0x58,0x00,0x05,0x03,0x00,0x10,0x00}},

	{0x00,1,{0x80}},             //panel CLKc1/2 setting
	{0xCF,14,{0x50,0x00,0x05,0x04,0x00,0x10,0x00,0x50,0x01,0x05,0x05,0x00,0x10,0x00}},

	{0x00,1,{0x90}},             //panel CLKc3/4 setting
	{0xCF,14,{0x50,0x02,0x05,0x06,0x00,0x10,0x00,0x50,0x03,0x05,0x07,0x00,0x10,0x00}},

	{0x00,1,{0xA0}},             //panel CLKd1/2 setting
	{0xCF,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xB0}},             //panel CLKd3/4 setting
	{0xCF,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},

	{0x00,1,{0xC0}},             //panel ECLK setting
	{0xCF,11,{0x3C,0x78,0x20,0x20,0x00,0x00,0x01,0x01,0x20,0x00,0x00}}, //gate pre. ena.

//  0003:BD90:
	{0x00,1,{0xB5}},
	{0xC5,6,{0x08,0x35,0xFF,0x08,0x35,0xFF}},

	{0x00,1,{0x00}},			//GVDD=5.008V, NGVDD=-5.008V 
	{0xD8,2,{0xAE,0xAE}},

	{0x00,1,{0x00}},			//VCOMDC=-1.584	//use factory otp target 
	{0xD9,1,{0x55}},			// zx++ BOE MTP vcom

//-------------------- Gamma --------------------// 	
	{0x00,1,{0x00}}, 
	{0xE1,16,{0x02,0x11,0x19,0x0E,0x08,0x12,0x0C,0x0B,0x03,0x07,0x0C,0x09,0x10,0x12,0x0D,0x08}},	

	{0x00,1,{0x00}}, 
	{0xE2,16,{0x02,0x11,0x18,0x0D,0x07,0x11,0x0C,0x0B,0x02,0x06,0x0B,0x08,0x10,0x11,0x0C,0x08}}, 

//  0003:C060:
	{0x00,1,{0x00}},             //Orise mode disable
	{0xFF,3,{0xFF,0xFF,0xFF}},

	{REGFLAG_DELAY,50,{}}, // 32

	{0x36,1,{0x00}},		//TE ON                                                                                                 
	{0x3A,1,{0x77}},

// Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.
                                                                                                                        
	{0x11,1,{0x00}},		// Sleep Out  
	{REGFLAG_DELAY,120,{}}, // 78                                                                                                  
                                   				                                                                                
	{0x29,1,{0x00}}, 		// Display on
//	{REGFLAG_DELAY,20,{}}, // 14  

//	{0x00,1,{0x00}},	 		

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
//  0003:C2A0:

static struct LCM_setting_table lcm_sleep_out_setting[] = {
	// Sleep Out
	{0x11, 0, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	// Sleep Mode On
	{0x10, 0, {0x00}},
	{REGFLAG_DELAY, 100, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {

		unsigned cmd;
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V2(cmd, table[i].count,
					table[i].para_list, force_update);
		}
	}
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS * util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS * params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	// enable tearing-free
	params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;	//BURST_VDO_MODE;

	// DSI
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_ONE_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = (LCM_COLOR_ORDER)"720*1280";
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.intermediat_buffer_num = 0;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage

	params->dsi.packet_size = 2;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = 0;

		params->dsi.vertical_sync_active				= 0;// 3    2
		params->dsi.vertical_backporch					= 2;// 20   1
		params->dsi.vertical_frontporch					= 2160; // 1  12
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 4;// 50  2
		params->dsi.horizontal_backporch				= 12;
		params->dsi.horizontal_frontporch				= 15;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	// Bit rate calculation
	//1 Every lane speed
	params->dsi.pll_div1 = 0;	// div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
	params->dsi.pll_div2 = 1;	// div2=0,1,2,3;div1_real=1,2,4,4
	params->dsi.fbk_div = 14;	//12;    // fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)

}

static void lcm_init(void)
{
	unsigned int data_array[16];

#ifdef BUILD_LK
	upmu_set_rg_vgp6_vosel(6);
	upmu_set_rg_vgp6_en(1);
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
#endif
	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(20);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(120);

	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00280500;	// Display Off
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00100500;	// Sleep In
	dsi_set_cmdq(&data_array, 1, 1);
	MDELAY(200);
#ifdef BUILD_LK
	upmu_set_rg_vgp6_en(0);
#else
	hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");
#endif
}

static void lcm_resume(void)
{
#ifndef BUILD_LK
	lcm_init();
#endif
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id0, id1, id2, id3, id4;
	unsigned char buffer[5];
	unsigned int array[5];
#ifdef BUILD_LK
	upmu_set_rg_vgp6_vosel(6);
	upmu_set_rg_vgp6_en(1);
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_2800, "LCM");
#endif

	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
	MDELAY(10);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
	MDELAY(100);

	// Set Maximum return byte = 1
	array[0] = 0x00053700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1, buffer, 5);
	id0 = buffer[0];
	id1 = buffer[1];
	id2 = buffer[2];
	id3 = buffer[3];
	id4 = buffer[4];

#if defined(BUILD_LK)
	printf("%s, Module ID = {%x, %x, %x, %x, %x} \n", __func__, id0,
	       id1, id2, id3, id4);
#else
	printk("%s, Module ID = {%x, %x, %x, %x,%x} \n", __func__, id0,
	       id1, id2, id3, id4);
#endif

	return (LCM_OTM1283_ID == ((id2 << 8) | id3)) ? 1 : 0;
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER hx8394d_hd720_dsi_vdo_truly_lcm_drv = {
	.name = "hx8394d_hd720_dsi_vdo_truly",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
};

