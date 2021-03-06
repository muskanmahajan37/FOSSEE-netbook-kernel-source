#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/spi/spi.h>

#include "linux_wlan_common.h"

#define USE_SPI_DMA     0	//johnny add

#ifdef NMC_ASIC_A0
#if defined(PLAT_PANDA_ES_OMAP4460)
#define MIN_SPEED 12000000
#define MAX_SPEED 24000000
#elif defined(PLAT_WMS8304)
#define MIN_SPEED 12000000
#define MAX_SPEED 24000000 //4000000
#else
#define MIN_SPEED 24000000
#define MAX_SPEED 48000000
#endif
#else /* NMC_ASIC_A0 */
/* Limit clk to 6MHz on FPGA. */
#define MIN_SPEED 6000000
#define MAX_SPEED 6000000 
#endif /* NMC_ASIC_A0 */

static uint32_t SPEED = MIN_SPEED;

struct spi_device* nmc_spi_dev;
void linux_spi_deinit(void* vp);

static int __init nmc_bus_probe(struct spi_device* spi){
	
	PRINT_D(BUS_DBG,"spiModalias: %s\n",spi->modalias);
	PRINT_D(BUS_DBG,"spiMax-Speed: %d\n",spi->max_speed_hz);
	nmc_spi_dev = spi;
	
	return 0;
}

static int __devexit nmc_bus_remove(struct spi_device* spi){
	
		//linux_spi_deinit(NULL);
	
	return 0;
}


struct spi_driver nmc_bus __refdata = {
		.driver = {
				.name = MODALIAS,
		},
		.probe =  nmc_bus_probe,
		.remove = __devexit_p(nmc_bus_remove),
};


void linux_spi_deinit(void* vp){
	
		spi_unregister_driver(&nmc_bus);	
		
		SPEED = MIN_SPEED;
		PRINT_ER("@@@@@@@@@@@@ restore SPI speed to %d @@@@@@@@@\n", SPEED);
	
}



int linux_spi_init(void* vp){
	int ret = 1;
	static int called = 0;
	
	
	if(called == 0){
		called++;
		if(&nmc_bus == NULL){
			PRINT_ER("nmc_bus address is NULL\n");
		}
		ret = spi_register_driver(&nmc_bus);		
	}

	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);
	
	return ret;
}

#if defined(PLAT_WMS8304)			// rachel
#define TXRX_PHASE_SIZE (4096)
#endif

#if defined (NM73131_0_BOARD)

int linux_spi_write(uint8_t* b, uint32_t len){	

	int ret;

	if(len > 0 && b != NULL) {
		struct spi_message msg;			
		PRINT_D(BUS_DBG,"Request writing %d bytes\n",len);
		struct spi_transfer tr = {
			.tx_buf = b,
			.len = len,							
			.speed_hz = SPEED,
			.delay_usecs = 0,
		};
	
		spi_message_init(&msg);
		spi_message_add_tail(&tr,&msg);
		ret = spi_sync(nmc_spi_dev,&msg);
		if(ret < 0){
			PRINT_ER( "SPI transaction failed\n");
		}

	} else{
		PRINT_ER("can't write data with the following length: %d\n",len);
		PRINT_ER("FAILED due to NULL buffer or ZERO length check the following length: %d\n",len);
		ret = -1;
	}

	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);


	return ret;
}

#elif defined(TXRX_PHASE_SIZE)

int linux_spi_write(uint8_t* b, uint32_t len){	
	int ret;
	if(len > 0 && b != NULL) {
		int i = 0;
		int blk = len/TXRX_PHASE_SIZE;
		int remainder = len%TXRX_PHASE_SIZE;

		//printk("[MMM] linux_spi_write in (t: %d), (blk:%d), (rem:%d), (%d)\n", len, blk, remainder, TXRX_PHASE_SIZE);

		char *r_buffer = (char*) kzalloc(TXRX_PHASE_SIZE, GFP_KERNEL);
		if(! r_buffer){
			PRINT_ER("Failed to allocate memory for r_buffer\n");
		}

		if(blk)
		{
			while(i<blk)
			{
				struct spi_message msg;
				struct spi_transfer tr = {
					.tx_buf = b + (i*TXRX_PHASE_SIZE),
					//.rx_buf = NULL,
					.len = TXRX_PHASE_SIZE,
					.speed_hz = SPEED,
					.bits_per_word = 8,
					.delay_usecs = 0,
				};
				/*
				char *r_buffer = (char*) kzalloc(TXRX_PHASE_SIZE, GFP_KERNEL);
				if(! r_buffer){
					PRINT_ER("Failed to allocate memory for r_buffer\n");
				}
				*/
				tr.rx_buf = r_buffer;
				
				memset(&msg, 0, sizeof(msg));
				spi_message_init(&msg);
				msg.spi = nmc_spi_dev;
				msg.is_dma_mapped = USE_SPI_DMA;			// rachel

				spi_message_add_tail(&tr, &msg);
				ret = spi_sync(nmc_spi_dev, &msg);
				if(ret < 0) {
					PRINT_ER( "SPI transaction failed\n");
				}
				//i += MJ_WRITE_SIZE;
				i++;
				
			}
		}
		if(remainder)
		{
			struct spi_message msg;
			struct spi_transfer tr = {
				.tx_buf = b + (blk*TXRX_PHASE_SIZE),
				//.rx_buf = NULL,
				.len = remainder,
				.speed_hz = SPEED,
				.bits_per_word = 8,
				.delay_usecs = 0,
			};
			/*
			char *r_buffer = (char*) kzalloc(remainder, GFP_KERNEL);
			if(! r_buffer){
				PRINT_ER("Failed to allocate memory for r_buffer\n");
			}
			*/
			tr.rx_buf = r_buffer;

			memset(&msg, 0, sizeof(msg));
			spi_message_init(&msg);
			msg.spi = nmc_spi_dev;
			msg.is_dma_mapped = USE_SPI_DMA;				// rachel

			spi_message_add_tail(&tr, &msg);
			ret = spi_sync(nmc_spi_dev, &msg);
			if(ret < 0) {
				PRINT_ER( "SPI transaction failed\n");
			}
		}
		if(r_buffer)
			kfree(r_buffer);
	} else {
		PRINT_ER("can't write data with the following length: %d\n",len);
		PRINT_ER("FAILED due to NULL buffer or ZERO length check the following length: %d\n",len);
		ret = -1;
	}
	
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);

	return ret;

}

#else
int linux_spi_write(uint8_t* b, uint32_t len){	

	int ret;
	struct spi_message msg;

	if(len > 0 && b != NULL){
		struct spi_transfer tr = {
					.tx_buf = b,
					//.rx_buf = r_buffer,
					.len = len,							
					.speed_hz = SPEED,
					.delay_usecs = 0,
		};
		char *r_buffer = (char*) kzalloc(len, GFP_KERNEL);
		if(! r_buffer){
			PRINT_ER("Failed to allocate memory for r_buffer\n");
		}
		tr.rx_buf = r_buffer;
		PRINT_D(BUS_DBG,"Request writing %d bytes\n",len);		
		
		memset(&msg, 0, sizeof(msg));
		spi_message_init(&msg);
//[[johnny add
		msg.spi = nmc_spi_dev;
		msg.is_dma_mapped = USE_SPI_DMA;      // rachel
//]]
		spi_message_add_tail(&tr,&msg);
		
		ret = spi_sync(nmc_spi_dev,&msg);
		if(ret < 0){
			PRINT_ER( "SPI transaction failed\n");
		}
					
		kfree(r_buffer);
	}else{
		PRINT_ER("can't write data with the following length: %d\n",len);
		PRINT_ER("FAILED due to NULL buffer or ZERO length check the following length: %d\n",len);
		ret = -1;
	}
		
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);
	
	
	return ret;
}

#endif

#if defined (NM73131_0_BOARD)

int linux_spi_read(unsigned char*rb, unsigned long rlen){

	int ret;

	if(rlen > 0) {
		struct spi_message msg;
		struct spi_transfer tr = {
			.rx_buf = rb,
			.len = rlen,
			.speed_hz = SPEED,
			.delay_usecs = 0,

		};

		spi_message_init(&msg);
		spi_message_add_tail(&tr,&msg);
		ret = spi_sync(nmc_spi_dev,&msg);
		if(ret < 0){
			PRINT_ER("SPI transaction failed\n");
		}
	}else{
		PRINT_ER("can't read data with the following length: %ld\n",rlen);
		ret = -1;
	}
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);

	return ret;
}

#elif defined(TXRX_PHASE_SIZE)

int linux_spi_read(unsigned char*rb, unsigned long rlen){
	int ret;
	//printk("[MMM] linux_spi_read in, (%d)\n", rlen);

	if(rlen > 0) {
		int i =0;

		int blk = rlen/TXRX_PHASE_SIZE;
		int remainder = rlen%TXRX_PHASE_SIZE;

		char *t_buffer = (char*) kzalloc(TXRX_PHASE_SIZE, GFP_KERNEL);
		if(! t_buffer){
			PRINT_ER("Failed to allocate memory for t_buffer\n");
		}

		//printk("[MMM] read size (%d), seperated (%d)\n", rlen, MJ_READ_SIZE);
		//printk("[MMM] linux_spi_read in (t: %d), (blk:%d), (rem:%d), (%d)\n", rlen, blk, remainder, TXRX_PHASE_SIZE);

		if(blk)
		{
			//printk("  read blk %d cnt\n", blk);
			while(i<blk)
			{
				struct spi_message msg;
				struct spi_transfer tr = {
					//.tx_buf = NULL,
					.rx_buf = rb + (i*TXRX_PHASE_SIZE),
					.len = TXRX_PHASE_SIZE,
					.speed_hz = SPEED,
					.bits_per_word = 8,
					.delay_usecs = 0,
				};
				tr.tx_buf = t_buffer;
				
				memset(&msg, 0, sizeof(msg));
				spi_message_init(&msg);
				msg.spi = nmc_spi_dev;
				msg.is_dma_mapped = USE_SPI_DMA;					// rachel

				spi_message_add_tail(&tr, &msg);
				ret = spi_sync(nmc_spi_dev, &msg);
				if(ret < 0) {
					PRINT_ER( "SPI transaction failed\n");
				}
				i ++;
			}
		}
		if(remainder)
		{
			struct spi_message msg;
			struct spi_transfer tr = {
				//.tx_buf = NULL,
				.rx_buf = rb + (blk*TXRX_PHASE_SIZE),
				.len = remainder,
				.speed_hz = SPEED,
				.bits_per_word = 8,
				.delay_usecs = 0,
			};
			/*
			char *t_buffer = (char*) kzalloc(remainder, GFP_KERNEL);
			if(! t_buffer){
				PRINT_ER("Failed to allocate memory for t_buffer\n");
			}
			*/
			tr.tx_buf = t_buffer;

			//printk("  read remain, %d bytes\n", remainder);
			memset(&msg, 0, sizeof(msg));
			spi_message_init(&msg);
			msg.spi = nmc_spi_dev;
			msg.is_dma_mapped = USE_SPI_DMA;				// rachel

			spi_message_add_tail(&tr, &msg);
			ret = spi_sync(nmc_spi_dev, &msg);
			if(ret < 0) {
				PRINT_ER( "SPI transaction failed\n");
			}
		}

		if(t_buffer)
			kfree(t_buffer);
	}else {
		PRINT_ER("can't read data with the following length: %ld\n",rlen);
		ret = -1;
	}
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);

	return ret;
}

#else
int linux_spi_read(unsigned char*rb, unsigned long rlen){

	int ret;
	
	if(rlen > 0){
		struct spi_message msg;
		struct spi_transfer tr = {
		//		.tx_buf = t_buffer,
				.rx_buf = rb,
				.len = rlen,
				.speed_hz = SPEED,
				.delay_usecs = 0,

		};
		char *t_buffer = (char*) kzalloc(rlen, GFP_KERNEL);
		if(! t_buffer){
			PRINT_ER("Failed to allocate memory for t_buffer\n");
		}
		tr.tx_buf = t_buffer;			

		memset(&msg, 0, sizeof(msg));
		spi_message_init(&msg);
//[[ johnny add
		msg.spi = nmc_spi_dev;
		msg.is_dma_mapped = USE_SPI_DMA;      // rachel
//]]
		spi_message_add_tail(&tr,&msg);

		ret = spi_sync(nmc_spi_dev,&msg);
		if(ret < 0){
			PRINT_ER("SPI transaction failed\n");
		}
		kfree(t_buffer);
	}else{
		PRINT_ER("can't read data with the following length: %ld\n",rlen);
		ret = -1;
	}
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);

	return ret;
}

#endif

int linux_spi_write_read(unsigned char*wb, unsigned char*rb, unsigned int rlen)
{

	int ret;

	if(rlen > 0) {
		struct spi_message msg;
		struct spi_transfer tr = {
			.rx_buf = rb,
			.tx_buf = wb,
			.len = rlen,
			.speed_hz = SPEED,
			.bits_per_word = 8,
			.delay_usecs = 0,

		};

		memset(&msg, 0, sizeof(msg));		// rachel
		spi_message_init(&msg);
		msg.spi = nmc_spi_dev;			// rachel
		msg.is_dma_mapped = USE_SPI_DMA;			// rachel for DMA
		
		spi_message_add_tail(&tr,&msg);
		ret = spi_sync(nmc_spi_dev,&msg);
		if(ret < 0){
			PRINT_ER("SPI transaction failed\n");
		}
	}else{
		PRINT_ER("can't read data with the following length: %d\n",rlen);
		ret = -1;
	}
	/* change return value to match NMI interface */
	(ret<0)? (ret = 0):(ret = 1);

	return ret;
}

int linux_spi_set_max_speed(void)
{
	SPEED = MAX_SPEED;
	
	PRINT_ER("@@@@@@@@@@@@ change SPI speed to %d @@@@@@@@@\n", SPEED);
	return 1;
}
