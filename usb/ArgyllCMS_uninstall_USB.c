
/*
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   2023/11/16
 *
 * Copyright 2023 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include "copyright.h"
#include "aconfig.h"

#include "windows.h"
#include "setupapi.h"

#ifdef SUOI_FORCEDELETE		/* If SDK doesn't have declaration... */

static void loadSDK() {
}

#else	/* If SDK doesn't have declaration... */

# define SUOI_FORCEDELETE   0x00000001

BOOL (WINAPI *SetupUninstallOEMInfA)(PCSTR FileName, DWORD Flags, PVOID Reserved) = NULL;

static void loadSDK() {
	HMODULE dllh = NULL;

	if ((dllh = LoadLibraryA("Setupapi.dll")) == NULL)
		return;

	*(FARPROC*)&SetupUninstallOEMInfA = GetProcAddress(dllh, "SetupUninstallOEMInfA");
}

#endif

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(void) {
	fprintf(stderr,"Uninstall ArgyllCMS libusb-win32 USB drivers%s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: ArgyllCMS_uninstall_USB [-v level]\n");
	fprintf(stderr," -v level                 Verbose level 0-2 (default 1)\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;			/* argument we're looking at */
	int verb = 1;		/* Default = none */
	int rv = 0;
	
	if (argc < 1)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* Verbosity */
			else if (argv[fa][1] == 'v') {
				fa = nfa;
				if (na == NULL) usage();
				verb = atoi(na);
			}

			else 
				usage();
		}
		else
			break;
	}

	if (verb >= 1)
		printf("Un-installing ArgyllCMS libusb0 instruments and drivers:\n");

	/* Un-install Devices */
	{
		// ClassGuid {817cffe0-328b-11df-9b9f-0002a5d5c51b}
		GUID guid = { 0x817cffe0, 0x328b, 0x11df, { 0x9b, 0x9f, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
		HDEVINFO dinfo;
		SP_DEVINFO_DATA infodata = { 0 } ;
		int index = 0;

		infodata.cbSize = sizeof(SP_DEVINFO_DATA);

		if ((dinfo = SetupDiGetClassDevsA(&guid, "USB", NULL, 0)) == NULL) {
			error("Didn't find any ArgyllCM libusb0 devices (Error 0x%x)",GetLastError());
		}

		if (verb >= 2) printf("Un-installing Devices\n");

		index = 0;
		while (SetupDiEnumDeviceInfo(
                             dinfo,
                             index,
                             &infodata)) {
			int dvix = 0;

			if (verb >= 2)
			printf("Un-installing device %d, GUID = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
				index+1,
				infodata.ClassGuid.Data1,
				infodata.ClassGuid.Data2,
				infodata.ClassGuid.Data3,
				infodata.ClassGuid.Data4[0],
				infodata.ClassGuid.Data4[1],
				infodata.ClassGuid.Data4[2],
				infodata.ClassGuid.Data4[3],
				infodata.ClassGuid.Data4[4],
				infodata.ClassGuid.Data4[5],
				infodata.ClassGuid.Data4[6],
				infodata.ClassGuid.Data4[7]);

			if (!SetupDiCallClassInstaller(DIF_REMOVE, dinfo, &infodata)) {
				if (GetLastError() == 0xe0000235)
					error("Un-installing Device '%d' failed because a 32 bit un-installer won't "
				              "work on a 64 bit system.",index+1);
				error("Un-installing Device %d failed (Error 0x%x)",index+1, GetLastError());
			}
    		index++;
        }
		SetupDiDestroyDeviceInfoList(dinfo);
	}

	/* Un-install Driver */
	{
		// ClassGuid {817cffe0-328b-11df-9b9f-0002a5d5c51b}
		GUID guid = { 0x817cffe0, 0x328b, 0x11df, { 0x9b, 0x9f, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } };
		HDEVINFO dinfo;
		SP_DRVINFO_DATA_A driverinfo = { 0 };
		SP_DRVINFO_DETAIL_DATA_A driverdetail = { 0 };
		CHAR InfName[MAX_PATH], *p;
		DWORD dsize = 0;
		int dvix = 0;

		driverinfo.cbSize = sizeof(SP_DRVINFO_DATA_A);
		driverdetail.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_A);

		if ((dinfo = SetupDiCreateDeviceInfoList(&guid, NULL)) == INVALID_HANDLE_VALUE) {
			error("Creating Device Info failed (Error 0x%x)",GetLastError());
		}

		if (!SetupDiBuildDriverInfoList(dinfo, NULL, SPDIT_CLASSDRIVER)) {
			error("Failed to locate ArgyllCM libusb0 Driver class (Error 0x%x)",GetLastError());
		}
    
		/* Find first device that uses this driver */
		if (!SetupDiEnumDriverInfoA(
                             dinfo,
							 NULL,
							 SPDIT_CLASSDRIVER,
                             0,
                             &driverinfo)) {
			error("Didn't find any Devices that use the ArgyllCMS driver (Error 0x%x)\n",GetLastError());
		}

		if (!SetupDiGetDriverInfoDetailA(dinfo, NULL, &driverinfo, &driverdetail,
		    driverdetail.cbSize, &dsize) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			error("Retrieving detailed Driver information failed (Error 0x%x)\n",GetLastError());
		}

		if (strlen(driverdetail.InfFileName) > 5
		 && (p = strrchr(driverdetail.InfFileName,'\\')) != NULL) {
			strcpy(InfName, p+1);

			loadSDK();

			if (SetupUninstallOEMInfA == NULL)
				error("Couldn't find SetupUninstallOEMInfA API");

			if (verb >= 1) printf("Un-installing Driver '%s' InfFile '%s'\n",
			                            driverinfo.ProviderName,InfName);

			if (!SetupUninstallOEMInfA(InfName, SUOI_FORCEDELETE, NULL)) {
				if (GetLastError() == 0xe0000235)
					error("Un-install driver '%s' failed because a 32 bit un-installer won't "
				              "work on a 64 bit system.",driverinfo.ProviderName);
				error("Un-install driver '%s' failed with (Error 0x%x)",
				              driverinfo.ProviderName,GetLastError());
			}
		} else {
			error("Driver '%s' InfPath '%s' is invalid\n",
		  	       driverinfo.ProviderName,driverdetail.InfFileName);
		}
		SetupDiDestroyDeviceInfoList(dinfo);
	}

	if (verb >= 1)
		printf("Un-install success!\n");

	return 0;
}


/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"  ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

