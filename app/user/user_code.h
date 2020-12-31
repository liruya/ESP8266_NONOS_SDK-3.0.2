#ifndef _USER_CODE_H_
#define _USER_CODE_H_
	
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif
	
#define	UERR_SUCCESS				(0)
#define	UMSG_SUCCESS				"SUCCESS"

#define	UERR_FOTA_BASE				(200)
#define	UERR_FOTA_UPGRADING			(UERR_FOTA_BASE+1)
#define	UERR_FOTA_PARAMS			(UERR_FOTA_BASE+2)
#define	UERR_FOTA_URL				(UERR_FOTA_BASE+3)
#define	UERR_FOTA_VERSION			(UERR_FOTA_BASE+4)

#define	UMSG_FOTA_UPGRADING			"Upgrading"
#define	UMSG_FOTA_PARAMS			"Invalid params"
#define	UMSG_FOTA_URL				"Invalid url or too long"
#define	UMSG_FOTA_VERSION			"Invalid version"


#ifdef __cplusplus
extern "C" {
#endif


#endif