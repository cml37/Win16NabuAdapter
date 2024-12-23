//---------------------------------------------------------------------------
//
//  Module: ttyexterns.h
//
//  Purpose:
//     This is the header file to make a few TTY sample programs excerpts
//     available for other use cases to preent circular dependency.
//     inclusions.
//
//  Development Team:
//     Chris Lenderman
//
//  History:   Date       Author      Comment
//             12/23/24   ChrisL      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR WriteTTYBlock( HWND, LPSTR, int ) ;
BOOL NEAR WriteCommBlock( HWND, BYTE*, int) ;
BOOL NEAR WriteCommByte( HWND, BYTE ) ;
