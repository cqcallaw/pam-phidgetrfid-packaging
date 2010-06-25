/******************************************************************************
 *
 * pam-phidgetrfid - A module for PAM that reads tags from a Phidget
 * RFID tag reader and passes the tag codes down the PAM authentication stack
 * as authentication tokens.
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

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_misc.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <scanner-interface.h>

#define  FALSE   (0)
#define  TRUE    (1)

static pam_handle_t *_pam_handle;
static unsigned int _silent_operation = FALSE;
static int _log_level = LOG_INFO;

/*
 * Log message callback function.
 *
 * Arguments: log_level is the level to log, message is the message to log.
 */
static void log_message(int log_level, char *message)
{
	int status = 0;
	char *response = NULL;

	if (log_level <= _log_level)
	{
		pam_syslog(_pam_handle, log_level, "%s", message);
	}

	if (log_level == LOG_NOTICE && _silent_operation == FALSE)
	{
		const struct pam_message info_message = {
				.msg_style = PAM_TEXT_INFO,
				.msg = message,
		};
		const struct pam_message *message_pointer = &info_message;

		struct pam_response pam_response = {
				.resp = response,
				.resp_retcode = 0,
		};
		struct pam_response *response_pointer = &pam_response;

		const struct pam_conv *pam_conversation = NULL;

		status = pam_get_item(_pam_handle, PAM_CONV, (const void **)&pam_conversation);

		if (status != PAM_SUCCESS || !pam_conversation || !pam_conversation->conv)
		{
			pam_syslog(_pam_handle, LOG_NOTICE, "error getting PAM conversation handle: %s", pam_strerror(_pam_handle, status));
		}
		else
		{
			status = pam_conversation->conv(1, &message_pointer, &response_pointer, pam_conversation->appdata_ptr);
			if (status != PAM_SUCCESS)
				pam_syslog(_pam_handle, LOG_NOTICE, "error encountered sending message to user: %s", pam_strerror(_pam_handle, status));
		}

		if(pam_response.resp != NULL)
			free((void *)pam_response.resp);

		//to satisfy splint: message shouldn't ever change.
		if(info_message.msg != message)
			free((void *)info_message.msg);
	}
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *rhost = NULL;
	char *rfid_code = NULL;
	int pam_function_result = PAM_SUCCESS;
	int scan_result = 0;
	char buffer[9] = "........";

	//set some configuration defaults
	unsigned int timeout = 10;
	unsigned int sleep_increment = 1;

	//sanity checks
	pam_function_result = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
	if(pam_function_result != PAM_SUCCESS)
	{
		pam_syslog(_pam_handle, LOG_ERR, "failed to obtain remote host information during sanity check: %s", pam_strerror(_pam_handle, pam_function_result));
		return pam_function_result;
	}

	if (rhost != NULL && strlen(rhost) > 0)
	{
		//remote login (e.g. over SSH)
		pam_syslog(_pam_handle, LOG_INFO, "remote login--aborting.");
		return PAM_AUTHINFO_UNAVAIL;
	}

	_pam_handle = pamh;

	//process module arguments
	for ( ; argc-- > 0; ++argv)
	{
		if(strcmp(*argv, "debug") == 0)
		{
			_log_level = LOG_DEBUG;
		}
		else if(strcmp(*argv, "silent") == 0 || (flags & PAM_SILENT) == PAM_SILENT)
		{
			_silent_operation = TRUE;
		}
		else if (strcmp(strncpy(buffer, *argv, 8), "timeout=") == 0)
		{
			unsigned int val = (unsigned int)atoi(*argv + 8);
			if (val == 0)
			{
				pam_syslog(_pam_handle, LOG_ERR, "invalid timeout specified: %s", *argv + 8);
			}
			else
			{
				timeout = val;
			}
		}
		else
		{
			pam_syslog(_pam_handle, LOG_ERR, "unrecognized argument: %s", *argv);
		}
	}

	//let's get to it
	if (init_scanner(&log_message, 1) != 0)
	{
		pam_syslog(_pam_handle, LOG_NOTICE, "no scanner detected, exiting.");
		return PAM_AUTHINFO_UNAVAIL;
	}
	scan_result = scan_tag(&rfid_code, timeout, sleep_increment);
	close_scanner();

	//process scan results
	if(scan_result == 1 && rfid_code != NULL)
	{
		pam_syslog(_pam_handle, LOG_NOTICE, "authentication token acquired.");

		pam_function_result = pam_set_item(pamh, PAM_AUTHTOK, (void *)rfid_code);
		if(pam_function_result != PAM_SUCCESS)
		{
			pam_syslog(_pam_handle, LOG_ERR, "error while setting PAM auth token to scanned RFID code: %s", pam_strerror(_pam_handle, pam_function_result));
			return pam_function_result;
		}

		pam_syslog(_pam_handle, LOG_DEBUG, "authentication token passed to PAM.");

		free(rfid_code);

		return PAM_IGNORE;
	}
	else
	{
		pam_syslog(_pam_handle, LOG_NOTICE, "authentication failed.");
		return PAM_AUTHINFO_UNAVAIL;
	}
}

PAM_EXTERN int pam_sm_setcred(/*@unused@*/pam_handle_t *pamh, /*@unused@*/int flags, /*@unused@*/int argc, /*@unused@*/const char **argv)
{
	//Phidget scanner doesn't support re-programming of RFID tags,
	//and we don't have any way of changing the down-stream authentication schemes
	return PAM_SUCCESS;
}
