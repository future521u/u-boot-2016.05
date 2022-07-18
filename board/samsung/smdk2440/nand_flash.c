#include "s3c2440_soc.h"
#include "uart.h"

#if 0
#define     __REG(x)					(*(volatile unsigned int *)(x)) 
#define     __REG_BYTE(x)				(*(volatile unsigned char *)(x)) 

/*NAND Flash*/
#define     NFCONF                   __REG(0x4E000000)  //NAND flash configuration             
#define     NFCONT                   __REG(0x4E000004)  //NAND flash control                   
#define     NFCMD                    __REG_BYTE(0x4E000008)  //NAND flash command                   
#define     NFADDR                   __REG_BYTE(0x4E00000C)  //NAND flash address                   
#define     NFDATA                   __REG_BYTE(0x4E000010)  //NAND flash data                      
#define     NFMECC0                  __REG(0x4E000014)  //NAND flash main area ECC0/1          
#define     NFMECC1                  __REG(0x4E000018)  //NAND flash main area ECC2/3          
#define     NFSECC                   __REG(0x4E00001C)  //NAND flash spare area ECC            
#define     NFSTAT                   __REG_BYTE(0x4E000020)  //NAND flash operation status          
#define     NFESTAT0                 __REG(0x4E000024)  //NAND flash ECC status for I/O[7:0]   
#define     NFESTAT1                 __REG(0x4E000028)  //NAND flash ECC status for I/O[15:8]  
#define     NFMECC0_STATUS           __REG(0x4E00002C)  //NAND flash main area ECC0 status     
#define     NFMECC1_STATUS           __REG(0x4E000030)  //NAND flash main area ECC1 status     
#define     NFSECC_STATUS            __REG(0x4E000034)  //NAND flash spare area ECC status     
#define     NFSBLK                   __REG(0x4E000038)  //NAND flash start block address       
#define     NFEBLK                   __REG(0x4E00003C)  //NAND flash end block address       
#endif


#define TXD0READY   (1<<2)

static void nand_init(void)
{
#define TACLS   0
#define TWRPH0  1
#define TWRPH1  0
	/* 设置时序 */
	NFCONF = (TACLS<<12)|(TWRPH0<<8)|(TWRPH1<<4);
	/* 使能NAND Flash控制器, 初始化ECC, 禁止片选 */
	NFCONT = (1<<4)|(1<<1)|(1<<0);	
}

static void nand_select(void)
{
	NFCONT &= ~(1<<1);	
}

static void nand_deselect(void)
{
	NFCONT |= (1<<1);	
}

static void nand_cmd(unsigned char cmd)
{
	volatile int i;
	NFCMD = cmd;
	for (i = 0; i < 10; i++);
}

static void nand_addr(unsigned int addr)
{
	unsigned int col  = addr % 2048;
	unsigned int page = addr / 2048;
	volatile int i;

	NFADDR = col & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR = (col >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	
	NFADDR  = page & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR  = (page >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR  = (page >> 16) & 0xff;
	for (i = 0; i < 10; i++);	
}

static void nand_page(unsigned int page)
{
	volatile int i;
	
	NFADDR  = page & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR  = (page >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR  = (page >> 16) & 0xff;
	for (i = 0; i < 10; i++);	
}

static void nand_col(unsigned int col)
{
	volatile int i;

	NFADDR = col & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR = (col >> 8) & 0xff;
	for (i = 0; i < 10; i++);
}


static void nand_wait_ready(void)
{
	while (!(NFSTAT & 1));
}

static unsigned char nand_data(void)
{
	return NFDATA;
}

static int nand_bad(unsigned int addr)
{
	unsigned int col  = 2048;
	unsigned int page = addr / 2048;
	unsigned char val;

	/* 1. 选中 */
	nand_select();
	
	/* 2. 发出读命令00h */
	nand_cmd(0x00);
	
	/* 3. 发出地址(分5步发出) */
	nand_col(col);
	nand_page(page);
	
	/* 4. 发出读命令30h */
	nand_cmd(0x30);
	
	/* 5. 判断状态 */
	nand_wait_ready();

	/* 6. 读数据 */
	val = nand_data();
	
	/* 7. 取消选中 */		
	nand_deselect();


	if (val != 0xff)
		return 1;  /* bad blcok */
	else
		return 0;
}

static int isBootFromNorFlash(void)
{
    volatile int *p = (volatile int *)0;

    int val;

    val = *p;

    *p = 0x12345678;

    if(*p == 0x12345678)
    {
        /* 写成功,是 nand启动 */
        *p = val;
        return 0;
    }
    else
    {
        /* NOR 不能像内存一样写 */
        return 1;
    }

}

static void nand_read(unsigned int addr, unsigned char *buf, unsigned int len)
{
	int col = addr % 2048;
	int i = 0;
		
	while (i < len)
	{
		/* 1. 选中 */
		nand_select();
		
		
		/* 2. 发出读命令00h */
		nand_cmd(0x00);

		/* 3. 发出地址(分5步发出) */
		nand_addr(addr);

		/* 4. 发出读命令30h */
		nand_cmd(0x30);

		/* 5. 判断状态 */
		nand_wait_ready();

		/* 6. 读数据 */
		for (; (col < 2048) && (i < len); col++)
		{
			buf[i] = nand_data();
			i++;
			addr++;
		}
		
		col = 0;


		/* 7. 取消选中 */		
		nand_deselect();
		
	}
}

int copy_code_to_sdram(unsigned char *src, unsigned char *dest, unsigned int len)
{
    int i = 0;

    /* nor boot */
    if(isBootFromNorFlash())
    {
        while (i < len)
        {
            dest[i] = src[i];
            i++;
        }
    }
    else
    {
        nand_init();
        nand_read((unsigned int)src, dest, len);
    }

    return 0;

}

void clear_bss(void)
{
    extern int __bss_start, __bss_end;

    int *p = &__bss_start;

    for (; p < &__bss_end; p++)
              *p = 0;

}
