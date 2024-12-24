//---------------------------------------------------------------------------
//
//  Module: nabu.c
//
//  Purpose:
//     Capabilities to support driving a NABU adaptor
//
//  Development Team:
//     Chris Lenderman
//
//  History:   Date       Author      Comment
//             12/23/24   ChrisL      Wrote it.
//
//---------------------------------------------------------------------------

#include "nabu.h"
#include "ttyexterns.h"

// TODOs
// Implement Time Segment
// Implement support for NABU files
// Add ability to save settings
// Use standard Windows functions for items like malloc, strcpy, etc.

// Base on the way that byte reads are not blocking,
// I've come up with a scheme to track the current processing
// command byte as well as determine which stage we are in for the processing byte
BYTE lastResetProcessingByte = 0x0;
BYTE processingByte;
BOOL processingByteInitialized = FALSE ;
int  processingStage = 0 ;

// The current packet and segment number for a file request
int   packetNumber ;
DWORD segmentNumber ;

// The current loaded packet for a file request, along with its length
unsigned char *loadedPacketPtr = NULL ;
int            loadedPacketLength = 0 ;

// If we have a loaded packet, free it, and reset the packet pointer and length
void freeLoadedPackets()
{
  if ( loadedPacketPtr != NULL )
  {
     free( loadedPacketPtr ) ;
  }
  loadedPacketPtr = NULL ;
  loadedPacketLength = 0 ;
}

// Load a file packet based on the current packet and segment number
BOOL NEAR loadFilePacket( HWND hWnd, char* filePath)
{
   char message[ 80 ] ;
   int packetLength = 0 ;
   char segmentName[ 100 ] ;
   FILE *file ;
   long fileSize = 0 ;
   int currentPacket = 0 ;
   unsigned char packetBuffer[ 2 ] ;

   freeLoadedPackets() ;

   if ( segmentNumber == 0x7fffff )
   {
      wsprintf( message, "Time Segment, not yet implemented\r\n" ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      return FALSE ;
   }

   wsprintf( segmentName, "%s%06lx.pak", filePath, segmentNumber ) ;
   wsprintf( message, "Cycle file: %s\r\n", segmentName ) ;
   WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
   file = fopen( segmentName, "rb" ) ;
   if ( file == NULL )
   {
      wsprintf( message, "Could not open file %s%06lx.pak\r\n", filePath, segmentNumber ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      return FALSE ;
   }

   fseek( file, 0, SEEK_END ) ;
   fileSize = ftell(file) ;
   rewind(file) ;

   if ( fileSize > 0 )
   {
      while ( ftell( file ) + 2 < fileSize)
      {
         fread( packetBuffer, 1, 2, file ) ;
         packetLength = ( unsigned int )packetBuffer[ 1 ] ;
         packetLength = packetLength << 8 ;
         packetLength = packetLength + ( unsigned int )packetBuffer[ 0 ] ;

         if (currentPacket == packetNumber)
         {
            loadedPacketPtr  = ( char* )malloc( packetLength ) ;
            if ( loadedPacketPtr == NULL )
            {
               wsprintf( message, "Error allocating memory\n" ) ;
               WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
               fclose( file ) ;
               return FALSE ;
            }
            fread( loadedPacketPtr, 1, packetLength, file ) ;
            loadedPacketLength = packetLength ;
            fclose( file ) ;
            return TRUE ;
         }
         else
         {
            fseek( file, packetLength, SEEK_CUR ) ;
         }
         currentPacket++ ;
      }
      fclose(file) ;
   }

   return FALSE ;
}

// Add escape characters and send a packet
void sendPacket( HWND hWnd )
{
   char *escapedPacket ;
   int escapedCharCount = 0 ;
   int i ;
   int counter = 0 ;

   for ( i = 0; i < loadedPacketLength; i++ )
   {
      if( loadedPacketPtr[ i ] == 0x10 )
      {
         escapedCharCount++ ;
      }
   }

   escapedPacket = ( char* ) malloc( loadedPacketLength + escapedCharCount ) ;

   for ( i = 0; i < loadedPacketLength; i++ )
   {
      if( loadedPacketPtr[ i ] == 0x10) {
          escapedPacket[ counter++ ] = 0x10 ;
      }
      escapedPacket[ counter++ ] = loadedPacketPtr[i] ;
   }

    WriteCommBlock( hWnd, escapedPacket, loadedPacketLength+escapedCharCount ) ;
    free( escapedPacket ) ;
}

// Handle a file request
BOOL NEAR handleFileRequest( HWND hWnd, BYTE b, char* filePath )
{
   char message[ 80 ] ;
   char write[ 4 ] ;
   DWORD tmp ;

   // Stage where we acknowledge the file request
   if ( processingStage == 0 )
   {
      write[ 0 ]= 0x10 ;
      write[ 1 ]= 0x6 ;
      WriteCommBlock(hWnd, write, 2) ;
      processingStage = 1 ;
      return TRUE ;
   }

   // Stage where we bring in the packet number
   if ( processingStage == 1 )
   {
      packetNumber = ( unsigned char )b ;
      processingStage = 2 ;
      return TRUE ;
   }

   // Stage where we bring in first byte of segment number
   if ( processingStage == 2 )
   {
      segmentNumber = ( unsigned char )b ;
      processingStage = 3 ;
      return TRUE ;
   }

   // Stage where we bring in second byte of segment number
   if ( processingStage == 3 )
   {
      tmp = ( unsigned char )b ;
      tmp = tmp << 8 ;
      segmentNumber = segmentNumber + tmp ;
      processingStage = 4 ;
      return TRUE ;
   }

   // Stage where we bring in third byte of segment number
   if ( processingStage == 4 )
   {
      tmp = ( unsigned char )b ;
      tmp = tmp << 16 ;
      segmentNumber = segmentNumber + tmp ;
      wsprintf( message, "Segment %06lx, Packet %06x \r\n", segmentNumber, packetNumber ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;

      WriteCommByte(hWnd, 0xE4) ;

      if ( !loadFilePacket( hWnd, filePath ) )
      {
         wsprintf( message, "Packet not found %06x\r\n", packetNumber );
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         WriteCommByte( hWnd, 0x90 ) ;
         processingStage = 5;
         return TRUE;
      }

      WriteCommByte( hWnd, 0x91 ) ;
      processingStage = 7 ;
      return TRUE ;
   }

   // Stage where we absorb byte 1 for "packet not found"
   if ( processingStage == 5 )
   {
      if ( b != 0x10 )
      {
         return FALSE ;
      }
      processingStage = 6 ;
      return TRUE ;
   }

   // Stage where we absorb byte 2 for "packet not found"
   if ( processingStage == 6 )
   {
      return FALSE ;
   }

   // Stage where we respond after acknowledging that we have a packet
   if ( processingStage == 7 )
   {
      if ( b != 0x10)
      {
         write[ 0 ] = 0x10 ;
         write[ 1 ] = 0x6 ;
         write[ 2 ] = 0xE4 ;
         WriteCommBlock( hWnd, write, 3 ) ;
         return FALSE ;
      }

      processingStage = 8 ;
      return TRUE ;
   }

   // Stage where we abosrb and check a byte
   if ( processingStage == 8 )
   {
      if ( b != 0x6 ) {
          return FALSE ;
      }

      // We will assume success
      sendPacket( hWnd ) ;

      write[ 0 ] = 0x10 ;
      write[ 1 ] = 0xE1 ;
      WriteCommBlock( hWnd, write, 2 ) ;
   }

   return FALSE ;
}

// Reset the NABU state machine
BOOL resetNabuState()
{
   processingByteInitialized = FALSE ;
   processingStage = 0 ;
   freeLoadedPackets() ;
   return TRUE ;
}

// Main NABU processing loop
void NEAR processNABU( HWND hWnd, BYTE b, char* filePath )
{
   int channel ;
   char message[ 40 ] ;
   BYTE write[ 3 ];
   BYTE switchingByte = b ;

   if ( processingByteInitialized )
   {
      switchingByte = processingByte ;
   }
   else
   {
      lastResetProcessingByte = processingByte;
      processingByte = b ;
      processingByteInitialized = TRUE ;
   }

   switch (switchingByte)
   {
      case 0x85: // Channel
         if ( processingStage == 0 )
         {
            write[ 0 ] = 0x10 ;
            write[ 1 ] = 0x6 ;
            WriteCommBlock( hWnd, write, 2 ) ;
            processingStage = 1 ;
            break ;
         }

         if ( processingStage == 1 )
         {
            channel = b ;
            processingStage = 2 ;
            break ;
         }
         if ( processingStage == 2 )
         {
            channel = channel + ( ( ( unsigned int )b ) << 8 ) ;
            wsprintf( message, "Channel: %d\r\n", channel ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
            WriteCommByte( hWnd, 0xE4 ) ;
         }
         resetNabuState() ;
         break ;
      case 0x84: // File Transfer
         if ( processingStage == 0 )
         {
            wsprintf( message, "File Request\r\n" ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message )) ;
         }
         if ( handleFileRequest( hWnd, b, filePath ) )
         {
            break ;
         }
         resetNabuState() ;
         break ;
      case 0x83:
         write[ 0 ] = 0x10 ;
         write[ 1 ] = 0x6 ;
         write[ 2 ] = 0xE4 ;
         WriteCommBlock( hWnd, write, 3 ) ;
         resetNabuState() ;
         break ;
      case 0x82:
         if ( processingStage == 0 )
         {
            wsprintf( message, "Configure Channel\r\n" ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
            write[ 0 ] = 0x10 ;
            write[ 1 ] = 0x6 ;
            WriteCommBlock( hWnd, write, 2 ) ;
            processingStage = 1;
            break;
         }

         if ( processingStage == 1 )
         {
            write[ 0 ] = 0x1F ;
            write[ 1 ] = 0x10 ;
            write[ 2 ] = 0xE1 ;
            WriteCommBlock( hWnd, write, 3 ) ;
            wsprintf( message, "Configure Channel Done\r\n" ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         }

         resetNabuState() ;
         break ;
      case 0x81:
         if ( processingStage == 0 )
         {
            write[ 0 ] = 0x10 ;
            write[ 1 ] = 0x06 ;
            WriteCommBlock( hWnd, write, 2 ) ;
            processingStage = 1 ;
            break ;
         }
         if ( processingStage == 1 )
         {
            processingStage = 2 ;
            break ;
         }
         if (processingStage == 2)
         {
            WriteCommByte( hWnd, 0xE4 ) ;
         }
         resetNabuState() ;
         break ;
      case 0x1E:
         WriteCommByte( hWnd, 0x10 ) ;
         WriteCommByte( hWnd, 0xE1 ) ;
         resetNabuState() ;
         break ;
      case 0x5:
         WriteCommByte(hWnd, 0xE4) ;
         resetNabuState() ;
         break ;
      case 0xF:
         resetNabuState() ;
         break ;
      default:
         wsprintf( message, "Unrecognized command 0x%x\r\n", b ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         resetNabuState();

         // Let's try to execute the last inititialized command we had
         processNABU( hWnd, lastResetProcessingByte, filePath );
         break ;
   }
}
