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

BOOL resetNabuState() ;
void NEAR processNABU( HWND, BYTE, char* ) ;
