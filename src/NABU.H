//---------------------------------------------------------------------------
//
//  Module: nabu.h
//
//  Purpose:
//     This is the header file for NABU Adaptor capabilities.
//
//  Development Team:
//     Chris Lenderman
//
//  History:   Date       Author      Comment
//             12/23/24   ChrisL      Wrote it.
//
//---------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>
#include <io.h>

void NEAR resetNabuState() ;
void NEAR processNABU( HWND, BYTE, char* ) ;

#define PACKET_HEADER_SIZE 16
#define PACKET_DATA_SIZE 993
#define PACKET_CRC_SIZE 2
#define TIME_SEGMENT_SIZE 29

