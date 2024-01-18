//gcc -shared -o librs485.so -fPIC rs485.c
//cp librs485.so /usr/lib/librs485.so
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <time.h>


int set_interface_attribs(int fd, int speed)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }
    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    struct serial_struct serial;
    ioctl(fd, TIOCGSERIAL, &serial);
    serial.flags |= ASYNC_LOW_LATENCY;
    serial.xmit_fifo_size = 1; // what is "xmit" ??
    ioctl(fd, TIOCSSERIAL, &serial);

    tty.c_lflag &=  ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB; /* no parity bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */
    tty.c_iflag &= ~(IGNBRK | BRKINT | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag&=~(ONLCR|OCRNL);

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 8;

	tcflush( fd, TCIFLUSH );

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }


    return 0;
}

int serial_d=0;

int s_open(char *dev) {
    char *portname = dev;
    //printf("open serial: %s \n", dev );
    serial_d = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_d < 0) return -1;

    set_interface_attribs(serial_d, B19200);
    fcntl(serial_d, F_SETFL, FNDELAY);
    tcflush(serial_d, TCIFLUSH);
    tcdrain(serial_d);
    return(0);
}

int s_close() {
    if (serial_d) close(serial_d);
    return(0);
}

int s_request(char *sbuff, int sbuffl, char *dbuff, int *dbuffl, int delay) {
	int  wlen;
	char *tt;
	tt = (char *)dbuff;

        usleep(2000); // 5ms wait
		wlen = write( serial_d,  sbuff, sbuffl);
		if (wlen <= 0 ) return(-1);
		if (wlen != sbuffl) printf("write error: %d != %d \n",wlen, sbuffl);

		//tcdrain(serial_d); // waits until all output written to the object referred to by fd has been transmitted.
		usleep( 521*(sbuffl+8) + delay); // 1000000*sbuffl*10/19200

		int p=0, i=0, rdlen;
        uint8_t datalen=0;
        uint8_t cmd=0;

		while(i<16) {
			rdlen = read(serial_d, tt+p, 8);
			if (rdlen > 0) {
                //if (p>0 && i>0) printf("next: [%d] %d->%d  len=%d cmd=%d\r\n",i, p, rdlen, datalen, cmd);
				p+=rdlen;
				if (p>=4) {
					memcpy(&datalen, tt+3, 1);
					memcpy(&cmd, tt+5, 1);
					if (datalen>16) {
                        printf("Data corrupted(>16 bytes): %d \r\n",datalen);
                        return -1;
					}
					if (p>=(datalen+7)) break;
					if (datalen>0) usleep( 521*((7+datalen)-p)+782);
				}
			} else {
                usleep(782);
			}

			i++;
		}

		if (p>=23) {
            uint8_t *bb;
            bb = malloc(8);
            usleep( 521*16);
            rdlen = read(serial_d, bb, 8);
            if (rdlen>0) {
                printf("next header ahead: %d \r\n", rdlen );
            }
            free(bb);
		}

        if(i>2 && p>0) printf("retries to send: %d\n",i);
		//if (i>2) printf("retries: %d\n",i);

        if (p==0) usleep(5000);

		*dbuffl = p;
		return(0);
}

