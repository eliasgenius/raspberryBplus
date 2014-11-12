/*
 * main.c
 *
 *
 *  Created on: 14.08.2013
 *      Author: alexs
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/wait.h>
#include <signal.h>
#include "rfid.h"
#include "bcm2835.h"
#include "config.h"
#define PIN RPI_GPIO_P1_11

uint8_t HW_init(uint32_t spi_speed, uint8_t gpio);
void usage(char *);
char *KeyA="Test01";
char *KeyB="Test10";
uint8_t debug=0;


int main(int argc, char *argv[]) {

	uid_t uid;
	uint8_t SN[10];
	uint16_t CType=0;
	uint8_t SN_len=0;
	char status;
	int tmp,i;
	
	char str[255];
	char *p;
	char sn_str[23];
	pid_t child;
	int max_page=0;
	uint8_t page_step=0;

	FILE * fmem_str;
	char save_mem=0;
	char fmem_path[255];
	uint8_t use_gpio=0;
	uint8_t gpio=255;
	uint32_t spi_speed=100000000L;
	
	char tarjeta1[4];
	char tarjeta2[4];
	
	
	tarjeta1[1]='d';
	tarjeta1[2]='d';
	
	tarjeta2[1]='0';
	tarjeta2[2]='5';
	
	if (argc>1) {
		if (strcmp(argv[1],"-d")==0) {debug=1;
		printf("Debug mode.\n");
		}else{
			usage(argv[0]);
			exit(1);
		}
	}

	if (open_config_file(config_file)!=0) {
		if (debug) {fprintf(stderr,"Can't open config file!");}
		else{syslog(LOG_DAEMON|LOG_ERR,"Can't open config file!");}
		return 1;
	}
	if (find_config_param("GPIO=",str,sizeof(str)-1,1)==1) {
		gpio=(uint8_t)strtol(str,NULL,10);
		if (gpio<28) {
			use_gpio=1;
		} else {
			gpio=255;
			use_gpio=0;
		}
	}
	if (find_config_param("SPI_SPEED=",str,sizeof(str)-1,1)==1) {
		spi_speed=(uint32_t)strtoul(str,NULL,10);
		if (spi_speed>125000L) spi_speed=125000L;
		if (spi_speed<4) spi_speed=4;
	}

	if (HW_init(spi_speed,gpio)) return 1; // Если не удалось инициализировать RC522 выходим.
	if (read_conf_uid(&uid)!=0) return 1;
	setuid(uid);
	InitRc522();

	if (find_config_param("NEW_TAG_PATH=",fmem_path,sizeof(fmem_path)-1,0)) {
		save_mem=1;
		if (fmem_path[strlen(fmem_path)-1]!='/') {
			sprintf(&fmem_path[strlen(fmem_path)],"/");
			if (strlen(fmem_path)>=240) {
				if (debug) {fprintf(stderr,"Too long path for tag dump files!");}
				else{syslog(LOG_DAEMON|LOG_ERR,"Too long path for tag dump files!");}
				return 1;
			}
		}
	}

	for (;;) {
		
			
		status= find_tag(&CType);
		if (status==TAG_NOTAG) {
			usleep(200000);
			continue;
		}else if ((status!=TAG_OK)&&(status!=TAG_COLLISION)) {continue;}

		if (select_tag_sn(SN,&SN_len)!=TAG_OK) {continue;}

		p=sn_str;
		*(p++)='[';
		for (tmp=0;tmp<SN_len;tmp++) {
			sprintf(p,"%02x",SN[tmp]);
			p+=2;
		}
		//for debugging
		if (debug) {
		*p=0;
		fprintf(stderr,"Type: %04x, XX Serial: %s\n",CType,&sn_str[1]);
		//printf(" numeros %c,%c \n",sn_str[1],sn_str[2]);
		//printf(" * numeros %c,%c \n",tarjeta1[1],tarjeta1[2]);
		if (sn_str[1]==tarjeta1[1] && sn_str[2]==tarjeta1[2]){
			printf(" * tarjeta 1 \n");
			// Turn it on
				bcm2835_gpio_write(PIN, HIGH);
				
				// wait a bit
				bcm2835_delay(800);
					
				// turn it off
				bcm2835_gpio_write(PIN, LOW);
					
				// wait a bit
				//bcm2835_delay(500);
				}

			int contador;
			for(contador=0;contador<6; contador++){
				
				
			}
		}
		*(p++)=']';
		*(p++)=0;
		//printf(sn_str[1]);
		if (use_gpio) bcm2835_gpio_write(gpio, HIGH);
		//ищем SN в конфиге
		if (find_config_param(sn_str,str,sizeof(str),1)>0) {
			child=fork();
			if (child==0) {
				fclose(stdin);
				freopen("","w",stdout);
				freopen("","w",stderr);
				execl("/bin/sh","sh","-c",str,NULL);
			} else if (child>0) {
				i=6000;
				do {
					usleep(10000);
					tmp=wait3(NULL,WNOHANG,NULL);
					i--;
				} while (i>0 && tmp!=child);

				if (tmp!=child) {
					kill(child,SIGKILL);
					wait3(NULL,0,NULL);
					if (debug) {fprintf(stderr,"Killed\n");}
				}else {
					if (debug) {fprintf(stderr,"Exit\n");}
				}
			}else{
				if (debug) {fprintf(stderr,"Can't run child process! (%s %s)\n",sn_str,str);}
				else{syslog(LOG_DAEMON|LOG_ERR,"Can't run child process! (%s %s)\n",sn_str,str);}
			}

		}else{

			if (debug) {fprintf(stderr,"New tag: type=%04x SNlen=%d SN=%s\n",CType,SN_len,sn_str);}
				else{syslog(LOG_DAEMON|LOG_INFO,"New tag: type=%04x SNlen=%d SN=%s\n",CType,SN_len,sn_str);}

			if (save_mem) {
				switch (CType) {
				case 0x4400:
					max_page=0x0f;
					page_step=4;
					break;
				case 0x0400:
					PcdHalt();
					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
					continue;
					max_page=0x3f;
					page_step=1;
					break;
				default:
					break;
				}
				p=str;
				sprintf(p,"%s",fmem_path);
				p+=strlen(p);
				for (tmp=0;tmp<SN_len;tmp++) {
					sprintf(p,"%02x",SN[tmp]);
					p+=2;
				}
				sprintf(p,".txt");
				if ((fmem_str=fopen(str,"r"))!=NULL) {
					fclose(fmem_str);
					PcdHalt();
					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
					continue;
				}
				if ((fmem_str=fopen(str,"w"))==NULL) {
					syslog(LOG_DAEMON|LOG_ERR,"Cant open file for write: %s",str);
					PcdHalt();
					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
					continue;
				}
				for (i=0;i<max_page;i+=page_step) {
					read_tag_str(i,str);
					fprintf(fmem_str,"%02x: %s\n",i,str);
				}
				fclose(fmem_str);
			}
			
			
			
		}
		//printf(" * numeros %c,%c \n",sn_str[1],sn_str[2]);
		//printf(" * numeros %c,%c \n",targeta1[1],targeta1[2]);
		//-------------------------
		FILE *ptr_file;
		char buffer[7];
		ptr_file = fopen("wh.txt", "a+");
		if (!ptr_file)
			printf("jajaja\n");
		while (fgets(buffer,7,ptr_file) != NULL){
			printf("%s",buffer);
			int contador;
			for(contador=0;contador<6; contador++)
			{
			// Turn it on
			bcm2835_gpio_write(PIN, HIGH);
			
			// wait a bit
			bcm2835_delay(500);
			
			// turn it off
			bcm2835_gpio_write(PIN, LOW);
			
			// wait a bit
			bcm2835_delay(500);
			}
			}
		fclose(ptr_file);
		//---------------------------
		PcdHalt();
		if (use_gpio) bcm2835_gpio_write(gpio, LOW);
	}

	bcm2835_spi_end();
	bcm2835_close();
	close_config_file();
	


}


uint8_t HW_init(uint32_t spi_speed, uint8_t gpio) {
	uint16_t sp;

	sp=(uint16_t)(250000L/spi_speed);
	if (!bcm2835_init()) {
		syslog(LOG_DAEMON|LOG_ERR,"Can't init bcm2835!\n");
		return 1;
	}
	if (gpio<28) {
		bcm2835_gpio_fsel(gpio, BCM2835_GPIO_FSEL_OUTP);
		bcm2835_gpio_write(gpio, LOW);
	}

	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
	bcm2835_spi_setClockDivider(sp); // The default
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default
	
	bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_OUTP);				  // para el blink
	
	return 0;
}

void usage(char * str) {
	printf("Usage:\n%s [options]\n\nOptions:\n -d\t Debug mode\n\n",str);
}
