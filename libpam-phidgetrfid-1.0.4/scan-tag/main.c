/******************************************************************************
 *
 * main.c - main file for the scan-tag utility, part of the pam-phidgetrfid
 * package.
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

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <scanner-interface.h>

static unsigned int _silent_operation = FALSE;
static int _log_level = LOG_INFO;

static size_t _max_message_length = 256;
static char _message[256];

/*
 * Log message callback function.
 *
 * Arguments: log_level is the level to log, message is the message to log.
 */
static void log_message(int log_level, char *message)
{
	if (log_level <= _log_level && _silent_operation == FALSE)
	{
		printf("%s\n", message);
	}
}

int main(int argc, char* argv[])
{
	char *rfid_code = NULL;
	int scan_result = 0;
	char buffer[11] = "..........";

	//set some configuration defaults
	unsigned int timeout = 10;
	unsigned int sleep_increment = 1;
	int i = 1;

	//process command line arguments
	for ( ; i < argc; i++)
	{
		if(strcmp(argv[i], "--debug") == 0)
		{
			_log_level = LOG_DEBUG;
		}
		else if(strcmp(argv[i], "--silent") == 0)
		{
			_silent_operation = TRUE;
		}
		else if (strcmp(strncpy(buffer, argv[i], 10), "--timeout=") == 0)
		{
			unsigned int val = (unsigned int)atoi(argv[i] + 10);
			if (val == 0)
			{
				(void)snprintf(_message, _max_message_length, "invalid timeout specified: %s", argv[i] + 10);
				log_message(LOG_ERR, _message);
			}
			else
			{
				timeout = val;
			}
		}
		else
		{
			(void)snprintf(_message, _max_message_length, "unrecognized argument: %s", argv[i]);
			log_message(LOG_ERR, _message);
		}
	}

	//let's get to it
	if (init_scanner(&log_message, 1) != 0)
	{
		printf("no scanner detected, exiting.\n");
		return 0;
	}
	scan_result = scan_tag(&rfid_code, timeout, sleep_increment);
	close_scanner();

	//process scan results
	if(scan_result == 1 && rfid_code != NULL)
	{
		printf("%s\n", rfid_code);
		free(rfid_code);
	}
	else
	{
		printf("Failed to obtain tag code.\n");
	}

	return 0;
}
