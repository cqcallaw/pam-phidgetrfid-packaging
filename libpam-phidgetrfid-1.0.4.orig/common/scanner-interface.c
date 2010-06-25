/******************************************************************************
 *
 * phidget-interface.c - functions for retrieving tag values from a Phidget
 * RFID scanner using the vendor's API.
 *
 * Copyright (C) 2010 Caleb Callaway
 *
 * License:
 *
 * This file is part of pam-phidgetrfid.
 * pam-phidgetrfid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pam-phidgetrfid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pam-phigetrfid.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <phidget21.h>
#include <pthread.h>
#include <syslog.h>
#include "scanner-interface.h"

static CPhidgetRFIDHandle _rfid_handle;
static unsigned char _current_tag[5];
int _tag_read = FALSE;
static pthread_mutex_t _scan_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _attached_mutex = PTHREAD_MUTEX_INITIALIZER;

/* function pointer */
static void (*_log_function)(int log_level, char *msg);

static size_t _max_message_length = 256;
static char _message[256];

static int _scanner_attached;

/*
 * Tag scan event handler. Takes the value passed to it and replicates it in a local variable.
 */
static int tag_handler(CPhidgetRFIDHandle phidget_handle, /*@unused@*/ void *usrptr, unsigned char *TagVal)
{
	int status = 0;
	int i = 0;

	status = CPhidgetRFID_getLEDOn(phidget_handle, &status);
	if(status != PTRUE)
	{
		status = CPhidgetRFID_setLEDOn(phidget_handle, PTRUE);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error getting LED status: error code '%i'", status);
			_log_function(LOG_ERR, _message);
		}
	}

	_log_function(LOG_DEBUG, "scanning tag.");

	status = pthread_mutex_lock(&_scan_mutex);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting thread lock on tag reader: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}

	for(; i < 5; i++)
	{
		_current_tag[i] = TagVal[i];
	}
	_tag_read = TRUE;

	//leave the scanner LED on for one second
	(void)sleep(1);
	status = pthread_mutex_unlock(&_scan_mutex);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error releasing thread lock on tag reader: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}
	status = CPhidgetRFID_setLEDOn(phidget_handle, PFALSE);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error turning LED on: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	_log_function(LOG_DEBUG, "tag scanned.");

	return 0;
}

/*
 * Scanner attached event. Logs a notification to the log file.
 */
static int attach_handler(CPhidgetHandle RFID, /*@unused@*/void *userptr)
{
	int serial_number = 1;
	int status = 0;
	const char *name = "Default Value";

	status = CPhidget_getDeviceName (RFID, &name);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting Phidget device name: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidget_getSerialNumber(RFID, &serial_number);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting Phidget device serial number: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	(void)snprintf(_message, _max_message_length, "%s %10d attached.", name, serial_number);
	_log_function(LOG_DEBUG, _message);
	_log_function(LOG_DEBUG, "activating antenna.");

	status = CPhidgetRFID_setAntennaOn((CPhidgetRFIDHandle)RFID, PTRUE);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error enabling Phidget RFID antenna: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return status;
	}
	else
	{
		status = pthread_mutex_lock(&_attached_mutex);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error getting thread lock on attachment flag: error code '%i'", status);
			_log_function(LOG_ERR, _message);
			return 1;
		}
		_scanner_attached = TRUE;
		status = pthread_mutex_unlock(&_attached_mutex);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error releasing thread lock on attachment flag: error code '%i'", status);
			_log_function(LOG_ERR, _message);
			return 1;
		}
		return 0;
	}
}

/*
 * Scanner detached event. Logs a notification to the log file.
 */
static int detach_handler(CPhidgetHandle RFID, /*@unused@*/void *userptr)
{
	int serial_number = 1;
	int status = 0;
	const char *name = "Default Name";

	status = pthread_mutex_lock(&_attached_mutex);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting thread lock on attachment flag: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}
	_scanner_attached = FALSE;

	status = pthread_mutex_unlock(&_attached_mutex);

	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error releasing thread lock on attachment flag: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}

	status = CPhidget_getDeviceName (RFID, &name);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting Phidget device name: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidget_getSerialNumber(RFID, &serial_number);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error getting Phidget device serial number: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	(void)snprintf(_message, _max_message_length, "%s %10d detached.", name, serial_number);
	_log_function(LOG_DEBUG, _message);

	return 0;
}

/*
 * Phidget error event. Logs a notification to the log file.
 */
static int error_handler(/*@unused@*/CPhidgetHandle RFID, /*@unused@*/void *userptr, int error_code, const char *unknown)
{
	(void)snprintf(_message, _max_message_length, "Phidget error encountered: %d - %s", error_code, unknown);
	_log_function(LOG_ERR, _message);
	return 0;
}

/*
 * Calculates the number of digits in the supplied integer
 *
 * Arguments:
 *
 * 'number' is the number to inspect.
 *
 * Returns:
 *
 * An integer representing the number of digits in the supplied integer.
 */
static int get_digit_count(int number)
{
	int digits = 0;
	int step = 1;
	while (step <= number)
	{
		digits++;
		step *= 10;
	}
	return digits > 0 ? digits : 1;
}

/*
 * Initialize the Phidget and register event handlers. Returns 0 if initialization was successful, 1 if not.
 *
 * Arguments: log_function is a function pointer to the function that will handle log messages. wait_time is the time in seconds to wait for a scanner to be detected before the initialization routine times out and returns and error.
 */
int init_scanner(void (*log_function)(int, char*), unsigned int wait_time)
{
	int status = 0;
	unsigned int sleep_time_remaining = wait_time;
	unsigned int sleep_interval = 1;

	_log_function = log_function;
	_scanner_attached = FALSE;

	status = CPhidgetRFID_create(&_rfid_handle);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error creating handle to Phidget: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}

	_log_function(LOG_DEBUG, "registering event handlers.");
	status = CPhidget_set_OnAttach_Handler((CPhidgetHandle)_rfid_handle, attach_handler, NULL);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error registering attach event handler: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidget_set_OnDetach_Handler((CPhidgetHandle)_rfid_handle, detach_handler, NULL);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error registering detach event handler: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidget_set_OnError_Handler((CPhidgetHandle)_rfid_handle, error_handler, NULL);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error registering error event handler: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidgetRFID_set_OnTag_Handler(_rfid_handle, tag_handler, NULL);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error registering tag scanned event handler: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	_log_function(LOG_DEBUG, "opening handle to RFID scanner.");
	status = CPhidget_open((CPhidgetHandle)_rfid_handle, -1);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error opening Phidget handle: error code '%i'", status);
		_log_function(LOG_ERR, _message);
		return 1;
	}

	_log_function(LOG_DEBUG, "waiting for scanner to be attached.");
	while(sleep_time_remaining > 0)
	{
		if(_scanner_attached == TRUE)
			break;
		else
		{
			sleep_time_remaining -= sleep_interval;
			(void)sleep(sleep_interval);
		}
	}

	if(_scanner_attached == FALSE)
	{
		_log_function(LOG_NOTICE, "no RFID scanner detected.");
		return 1;
	}
	else
	{
		_log_function(LOG_DEBUG, "scanner attached.");
		_log_function(LOG_DEBUG, "Phidgit RFID initialization complete.");
		return 0;
	}
}

/*
 * Close the handle to the Phidget and free the associated resources.
 */
void close_scanner()
{
	int status = 0;

	_log_function(LOG_DEBUG, "closing Phidget RFID.");

	status = CPhidget_close((CPhidgetHandle)_rfid_handle);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error closing Phidget handle: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}
	status = CPhidget_delete((CPhidgetHandle)_rfid_handle);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error deleting Phidget handle: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	_scanner_attached = FALSE;
}

/*
 * Scan for a tag for the specified number of seconds.
 * If a tag is received, place it in the specified character array. Otherwise, indicate no tag was received.
 *
 * Arguments:
 *
 * 'destination' is the character array in which the scanned value will be replicated.
 * 'timeout' is the number of seconds to scan.
 *
 * Returns:
 *
 * 1 if a tag was scanned, 0 if a tag was not scanned.
 */
int scan_tag(char *destination[], unsigned int timeout, unsigned int sleep_interval)
{
	int status = 0;
	unsigned int sleep_time_remaining = timeout;
	char *notify_message_prefix = "scanning for RFID tag...";
	size_t notify_message_length = strlen(notify_message_prefix) + get_digit_count((int)timeout) + 2;

	char *notify_message = (char *)malloc(notify_message_length * sizeof (char));
	if (notify_message == NULL)
	{
		_log_function(LOG_CRIT, "failed to allocate memory for notify message.");
		return 0;
	}

	_log_function(LOG_DEBUG, "scanning for RFID tag.");

	//wait for tag value to be read until a timeout is reached.
	while(sleep_time_remaining > 0)
	{
		status = pthread_mutex_lock(&_scan_mutex);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error acquiring thread lock on tag reader: error code '%i'", status);
			_log_function(LOG_ERR, _message);
		}

		if(_tag_read == FALSE)
		{
			status = pthread_mutex_unlock(&_scan_mutex);
			if(status != 0)
			{
				(void)snprintf(_message, _max_message_length, "error releasing thread lock on tag reader: error code '%i'", status);
				_log_function(LOG_ERR, _message);
			}

			status = snprintf(notify_message, notify_message_length, "%s%u", notify_message_prefix, sleep_time_remaining);
			if (status < 0 || status > (int)notify_message_length)
			{
				(void)snprintf(_message, _max_message_length, "error generating info message: error code '%i'", status);
				_log_function(LOG_ERR, _message);
			}

			_log_function(LOG_NOTICE, notify_message);

			sleep_time_remaining -= sleep_interval;
			(void)sleep(sleep_interval);
		}
		else
		{
			status = pthread_mutex_unlock(&_scan_mutex);
			if(status != 0)
			{
				(void)snprintf(_message, _max_message_length, "error releasing thread lock on tag reader: error code '%i'", status);
				_log_function(LOG_ERR, _message);
			}
			break;
		}
	}
	free(notify_message);

	_log_function(LOG_DEBUG, "finished scanning for RFID tag.");

	status = pthread_mutex_lock(&_scan_mutex);
	if(status != 0)
	{
		(void)snprintf(_message, _max_message_length, "error acquiring thread lock on tag reader: error code '%i'", status);
		_log_function(LOG_ERR, _message);
	}

	if(_tag_read == FALSE)
	{
		status = pthread_mutex_unlock(&_scan_mutex);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error releasing thread lock on tag reader: error code '%i'", status);
			_log_function(LOG_ERR, _message);
		}

		_log_function(LOG_NOTICE, "scan timed out: no tag acquired.");
		status = 0;
	}
	else
	{
		status = pthread_mutex_unlock(&_scan_mutex);
		if(status != 0)
		{
			(void)snprintf(_message, _max_message_length, "error releasing thread lock on tag reader: error code '%i'", status);
			_log_function(LOG_ERR, _message);
		}

		*destination = (char *)malloc(RFID_TAG_LEN * (sizeof (char) + 1));
		if (*destination == NULL)
		{
			_log_function(LOG_CRIT, "failed to allocate memory for tag value");
			return 0;
		}
		status = snprintf(*destination, RFID_TAG_LEN + 1, "%02x%02x%02x%02x%02x", (unsigned int)_current_tag[0], (unsigned int)_current_tag[1], (unsigned int)_current_tag[2], (unsigned int)_current_tag[3], (unsigned int)_current_tag[4]);
		if (status > 0 && status < RFID_TAG_LEN)
		{
			(void)snprintf(_message, _max_message_length, "error writing tag value to string: error code '%i'", status);
			_log_function(LOG_ERR, _message);
		}

		_log_function(LOG_NOTICE, "tag acquired.");

		status = 1;
	}

	(void)snprintf(_message, _max_message_length, "scan_tag return status: '%i'", status);
	_log_function(LOG_DEBUG, _message);

	return status;
}
