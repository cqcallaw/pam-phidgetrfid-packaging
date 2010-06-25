/*
 * scanner-interface.h
 *
 *  Created on: Jun 7, 2010
 *      Author: caleb
 */

#ifndef SCANNERINTERFACE_H_
#define SCANNERINTERFACE_H_

	#define RFID_TAG_LEN 10
	#define  FALSE   (0)
	#define  TRUE    (1)

	/* scanner interface functions */
	int scan_tag(char *destination[], unsigned int timeout, unsigned int sleep_increment);
	int init_scanner(void (*log_function)(int, char*), unsigned int wait_time);
	void close_scanner();

#endif /* SCANNERINTERFACE_H_ */
