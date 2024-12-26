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
// Add ability to save settings
// Use standard Windows functions for items like malloc, strcpy, etc.
// Anywhere we use a fixed size array, scrutinize it to see if we can
// calculate dynamically instead
// Clean up ugly code
// Don't lock the GUI while downloading files

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

int cycleCrcTable[] =
    { 0, 4129, 8258, 12387, 16516, 20645, 24774, 28903, 33032, 37161, 41290,
      45419, 49548, 53677, 57806, 61935, 4657, 528, 12915, 8786, 21173,
      17044, 29431, 25302, 37689, 33560, 45947, 41818, 54205, 50076,
      62463, 58334, 9314, 13379, 1056, 5121, 25830, 29895, 17572, 21637,
      42346, 46411, 34088, 38153, 58862, 62927, 50604, 54669, 13907, 9842,
      5649, 1584, 30423, 26358, 22165, 18100, 46939, 42874, 38681, 34616,
      63455, 59390, 55197, 51132, 18628, 22757, 26758, 30887, 2112, 6241,
      10242, 14371, 51660, 55789, 59790, 63919, 35144, 39273, 43274,
      47403, 23285, 19156, 31415, 27286, 6769, 2640, 14899, 10770, 56317,
      52188, 64447, 60318, 39801, 35672, 47931, 43802, 27814, 31879,
      19684, 23749, 11298, 15363, 3168, 7233, 60846, 64911, 52716, 56781,
      44330, 48395, 36200, 40265, 32407, 28342, 24277, 20212, 15891,
      11826, 7761, 3696, 65439, 61374, 57309, 53244, 48923, 44858, 40793,
      36728, 37256, 33193, 45514, 41451, 53516, 49453, 61774, 57711, 4224,
      161, 12482, 8419, 20484, 16421, 28742, 24679, 33721, 37784, 41979,
      46042, 49981, 54044, 58239, 62302, 689, 4752, 8947, 13010, 16949,
      21012, 25207, 29270, 46570, 42443, 38312, 34185, 62830, 58703,
      54572, 50445, 13538, 9411, 5280, 1153, 29798, 25671, 21540, 17413,
      42971, 47098, 34713, 38840, 59231, 63358, 50973, 55100, 9939, 14066,
      1681, 5808, 26199, 30326, 17941, 22068, 55628, 51565, 63758, 59695,
      39368, 35305, 47498, 43435, 22596, 18533, 30726, 26663, 6336, 2273,
      14466, 10403, 52093, 56156, 60223, 64286, 35833, 39896, 43963,
      48026, 19061, 23124, 27191, 31254, 2801, 6864, 10931, 14994, 64814,
      60687, 56684, 52557, 48554, 44427, 40424, 36297, 31782, 27655,
      23652, 19525, 15522, 11395, 7392, 3265, 61215, 65342, 53085, 57212,
      44955, 49082, 36825, 40952, 28183, 32310, 20053, 24180, 11923,
      16050, 3793, 7920 };

// Grabs a buffer of data from a file accessed via HTTP
int NEAR grabBuffer( HWND hWnd, SOCKET sd, char* responseBuffer, int bufferLen, BOOL firstGrab )
{
   char message[ 80 ] ;
   int length = 0 ;
   unsigned long pendingByteCount = 0 ;
   int result = 0 ;

   // Read the bytes one at a time, ensuring there is data.
   // We don't want to block the socket, and this ensures that.
   for (;;)
   {
      if ( length > bufferLen )
      {
         break ;
      }

      if ( ioctlsocket(sd, FIONREAD, &pendingByteCount) == SOCKET_ERROR )
      {
         wsprintf( message, "A socket error was encountered on poll\r\n" ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         return -1 ;
      }

      // We wait for the length to grow greater than zero so that we don't
      // terminate right away while a response is still outstanding.
      // (unless this is a subsequent grab of data, in which case, don't wait)
      else if ( pendingByteCount == 0 && ( length > 0 || !firstGrab ))
      {
         break ;
      }

      result = recv( sd, responseBuffer + length, 1, 0 ) ;
      if ( result == SOCKET_ERROR )
      {
         wsprintf( message, "A socket error was encountered\r\n" ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         return -1 ;
      }

      if ( result != 1 )
      {
         wsprintf( message, "Bad socket result\r\n" ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         return -1 ;
      }
      length++ ;
   }
   return length ;
}

// Clean up from socket usage
void socketCleanup( SOCKET sd )
{
   if ( sd != INVALID_SOCKET )
   {
      closesocket( sd ) ;
   }
   WSACleanup() ;
}

// Download a file via HTTP
BOOL NEAR downloadFileViaHttp( HWND hWnd, char* filePath, char *hostAndPath, char* fileNameExtension )
{
   WSADATA wsaData = { 0 } ;
   char fileName[ 100 ] ;
   char fileNameCorrectedExtension [ 5 ];
   char message[ 120 ] ;
   SOCKET sd = INVALID_SOCKET ;
   struct hostent* he = { 0 } ;
   struct sockaddr_in addr_info = { 0 } ;
   char sendBuffer[ 120 ] ;
   int result ;
   char responseBuffer[ 1000 ] ;
   int length = 0 ;
   FILE *file = NULL ;
   int i ;
   BOOL firstGrab ;
   char *dataPtr ;
   char serverHost[ 100 ] ;
   char* serverPath = strrchr( hostAndPath, '/' ) + 1 ;

   strncpy(serverHost, hostAndPath, strrchr(hostAndPath, '/') - hostAndPath);
   serverHost[ strrchr(hostAndPath, '/')- hostAndPath ] = 0;

   if ( result = WSAStartup( 0x202, &wsaData ) != 0 )
   {
      wsprintf( message, "No support for WinSock 2.2, or socket init error. cannot download files\r\n" ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      socketCleanup( sd ) ;
      return FALSE ;
   }

   wsprintf( sendBuffer, "GET /%s/%06lX%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: nabu31\r\nAccept: */*\r\n\r\n", serverPath, segmentNumber, fileNameExtension, serverHost ) ;

   he = gethostbyname( serverHost ) ;
   if ( he == NULL )
   {
      wsprintf( message, "Failed to get host information for '%s': %s\n", serverHost, strerror( errno ) ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      socketCleanup(sd) ;
      return FALSE ;
   }

   sd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
   if ( sd == INVALID_SOCKET )
   {
      wsprintf( message, "Failed to create socket: %s\n", strerror( errno ) ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      socketCleanup( sd ) ;
      return FALSE ;
   }

   addr_info.sin_family = AF_INET ;
   addr_info.sin_port = htons( 80 ) ;
   addr_info.sin_addr = *(( struct in_addr* )he->h_addr ) ;
   memset( &( addr_info.sin_zero ), 0, sizeof( addr_info.sin_zero ) ) ;

   result = connect( sd, ( struct sockaddr* )&addr_info, sizeof( struct sockaddr ) ) ;
   if ( result == -1 )
   {
      wsprintf( message, "Failed to connect: %s\r\n", strerror( errno ) ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      socketCleanup( sd ) ;
      return FALSE ;
   }

   result = send( sd, sendBuffer, strlen( sendBuffer ), 0 ) ;
   if ( result == -1 )
   {
      wsprintf( message, "failed to send request: %s\r\n", strerror( errno ) ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      socketCleanup( sd ) ;
      return FALSE ;
   }

   // Win16 only supports 8.3 file names, truncate the extension if needed
   strncpy( fileNameCorrectedExtension, fileNameExtension, 4 ) ;
   fileNameCorrectedExtension[ 4 ] = 0 ;
   wsprintf( fileName, "%s%06lX%s", filePath, segmentNumber, fileNameCorrectedExtension ) ;

   firstGrab = TRUE ;
   while ( TRUE )
   {
      length = grabBuffer( hWnd, sd, responseBuffer, sizeof( responseBuffer ), firstGrab ) ;
      if ( length < 0 )
      {
         wsprintf( message, "Failed to retrieve data from socket request\r\n" ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         socketCleanup( sd ) ;
         if ( file != NULL )
         {
            fclose( file ) ;
            unlink( fileName ) ;
         }
         return FALSE ;
      }

      if ( length == 0 )
      {
          break ;
      }

      if ( firstGrab )
      {
         dataPtr = strstr( responseBuffer, "HTTP/1.1 200 OK\r\n" ) ;
         // File not found, return
         if (dataPtr == NULL)
         {
            socketCleanup( sd ) ;
            return FALSE ;
         }
         // Unexpected data, return
         dataPtr = strstr( responseBuffer, "\r\n\r\n" ) + 4 ;
         if (dataPtr == NULL)
         {
            wsprintf( message, "Unexpected data from HTTP return request\r\n" ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
            socketCleanup( sd ) ;
            return FALSE;
         }
         firstGrab = FALSE ;
         length = length - ( dataPtr - responseBuffer ) ;

         // File not found if no content
         if ( length == 0)
         {
            wsprintf( message, "Zero byte result from HTTP request\r\n" ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
            socketCleanup( sd ) ;
            return FALSE ;
         }
         file = fopen( fileName, "wb" ) ;
         wsprintf( message, "Downloading from %s\r\n", hostAndPath ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
      }
      else
      {
         dataPtr = responseBuffer ;
      }

      for ( i=0; i<length; i++ )
      {
         fprintf( file,"%c",dataPtr[i] ) ;
      }
   }

   fclose( file ) ;
   socketCleanup( sd ) ;
   return TRUE ;
}

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

// Calculates the CRC of a given cycle
void NEAR calculateCycleCRC( BYTE *data, int dataLength )
{
   int seed = 0xFFFF ;
   int i ;

   for ( i = 0; i < dataLength; i++ )
   {
      int index = ((((seed >> 8)) ^ data[i] & 0xFF)) & 0xFF ;
      seed <<= 8 ;
      seed ^= cycleCrcTable[ index ] ;
   }

   // ok, now get the high and low order CRC bytes
   seed ^= 0xFFFF ;
   data[ dataLength ] = (BYTE) ((seed >> 8) & 0xFF) ;
   data[ dataLength + 1 ] = (BYTE) (seed & 0xFF) ;
}

// Creates the time segment
void NEAR createTimeSegment()
{
   time_t now ;
   struct tm *currTime ;
   time(&now) ;
   currTime = localtime(&now) ;

   loadedPacketPtr  = ( char* )malloc( TIME_SEGMENT_SIZE ) ;

   loadedPacketPtr[ 0 ] = 0x7f ;
   loadedPacketPtr[ 1 ] = 0xff ;
   loadedPacketPtr[ 2 ] = 0xff ;
   loadedPacketPtr[ 3 ] = 0x0 ;
   loadedPacketPtr[ 4 ] = 0x0 ;
   loadedPacketPtr[ 5 ] = 0x7f ;
   loadedPacketPtr[ 6 ] = 0xff ;
   loadedPacketPtr[ 7 ] = 0xff ;
   loadedPacketPtr[ 8 ] = 0xff ;
   loadedPacketPtr[ 9 ] = 0x7f ;
   loadedPacketPtr[ 10 ] = 0x80 ;
   loadedPacketPtr[ 11 ] = 0x30 ;
   loadedPacketPtr[ 12 ] = 0x0 ;
   loadedPacketPtr[ 13 ] = 0x0 ;
   loadedPacketPtr[ 14 ] = 0x0 ;
   loadedPacketPtr[ 15 ] = 0x0 ;
   loadedPacketPtr[ 16 ] = 0x2 ;
   loadedPacketPtr[ 17 ] = 0x2 ;
   loadedPacketPtr[ 18 ] = currTime->tm_wday + 1 ;
   loadedPacketPtr[ 19 ] = 0x54 ;
   loadedPacketPtr[ 20 ] = currTime->tm_mon + 1 ;
   loadedPacketPtr[ 21 ] = currTime->tm_mday ;
   loadedPacketPtr[ 22 ] = currTime->tm_hour % 12 ;
   loadedPacketPtr[ 23 ] = currTime->tm_min ;
   loadedPacketPtr[ 24 ] = currTime->tm_sec ;
   loadedPacketPtr[ 25 ] = 0x0 ;
   loadedPacketPtr[ 26 ] = 0x0 ;

   // Calculate CRC will fill in indexes 27 and 28 with the CRC
   calculateCycleCRC( loadedPacketPtr, 27 ) ;
   loadedPacketLength = TIME_SEGMENT_SIZE ;
}

// Populates the packet header and CRC
void NEAR populatePacketHeaderAndCrc( int offset, BOOL lastSegment, BYTE *buffer, int bytesRead )
{
   BYTE type = 0x20 ;

   // Cobble together the header
   buffer [ 0 ] = ((int) (segmentNumber >> 16) & 0xFF) ;
   buffer [ 1 ] = ((int) (segmentNumber >> 8) & 0xFF) ;
   buffer [ 2 ] = ((int) (segmentNumber & 0xFF)) ;
   buffer [ 3 ] = packetNumber ;

   // Owner
   buffer [ 4 ] = 0x1 ;

   // Tier
   buffer [ 5 ] = 0x7F ;
   buffer [ 6 ] = 0xFF ;
   buffer [ 7 ] = 0xFF ;
   buffer [ 8 ] = 0xFF ;

   // Mystery bytes
   buffer [ 9 ] = 0x7F ;
   buffer [ 10 ] = 0x80 ;

   // Packet Type
   if ( lastSegment )
   {
      // Set the 4th bit to mark end of segment
      type = (BYTE) ( type | 0x10 ) ;
   }
   else if ( packetNumber == 0 )
   {
      type = 0xa1 ;
   }

   buffer [ 11 ] = type ;
   buffer [ 12 ] = packetNumber ;
   buffer [ 13 ] = 0x0 ;
   buffer [ 14 ] = ((int) (offset >> 8) & 0xFF) ;
   buffer [ 15 ] = ((int) (offset & 0xFF)) ;

   // Payload already prepopulated, so just calculate the CRC
   calculateCycleCRC( buffer, PACKET_HEADER_SIZE + bytesRead ) ;
}

// Create a file packet based on the current packet and segment number
BOOL NEAR createFilePacket( HWND hWnd, char* filePath, char* hostAndPath, BOOL tryDownload )
{
   char message[ 80 ] ;
   char segmentName[ 100 ] ;
   FILE *file ;
   long fileSize = 0 ;
   int currentPacket = 0 ;
   int offset = 0 ;
   int bytesRead = 0 ;

   wsprintf( segmentName, "%s%06lX.nab", filePath, segmentNumber ) ;
   file = fopen( segmentName, "rb" ) ;
   if ( file == NULL )
   {
      if ( tryDownload )
      {
         downloadFileViaHttp( hWnd, filePath, hostAndPath, ".nabu" ) ;
         file = fopen( segmentName, "rb" ) ;
      }

      if ( file == NULL )
      {
         return FALSE ;
      }
   }

   fseek( file, 0, SEEK_END ) ;
   fileSize = ftell( file ) ;
   rewind( file ) ;

   if ( fileSize > 0 )
   {
      while ( ftell( file ) < fileSize )
      {
         if ( currentPacket == packetNumber )
         {
            offset = ftell( file );
            loadedPacketPtr = ( char* )malloc( PACKET_HEADER_SIZE + PACKET_DATA_SIZE + PACKET_CRC_SIZE ) ;
            if ( loadedPacketPtr == NULL )
            {
               wsprintf( message, "Error allocating memory\n" ) ;
               WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
               fclose( file ) ;
               return FALSE ;
            }
            // Skip past the header and fill in the data
            bytesRead = fread( &loadedPacketPtr[ PACKET_HEADER_SIZE ], 1, PACKET_DATA_SIZE, file ) ;
            // Populate the header and CRC
            populatePacketHeaderAndCrc( offset, ftell( file ) == fileSize, loadedPacketPtr, bytesRead ) ;
            loadedPacketLength = PACKET_HEADER_SIZE + bytesRead + PACKET_CRC_SIZE ;
            fclose( file ) ;
            return TRUE ;
         }
         else
         {
            fseek( file, PACKET_DATA_SIZE, SEEK_CUR ) ;
            currentPacket++ ;
         }
      }
      fclose( file ) ;
   }
   return FALSE;
}

// Load a file packet based on the current packet and segment number
BOOL NEAR loadFilePacket( HWND hWnd, char* filePath, char* hostAndPath, BOOL tryDownload )
{
   char message[ 80 ] ;
   int packetLength = 0 ;
   char segmentName[ 100 ] ;
   FILE *file ;
   long fileSize = 0 ;
   int currentPacket = 0 ;
   unsigned char packetBuffer[ 2 ] ;

   wsprintf( segmentName, "%s%06lX.pak", filePath, segmentNumber ) ;
   file = fopen( segmentName, "rb" ) ;
   if ( file == NULL )
   {
      if (tryDownload )
      {
         downloadFileViaHttp(hWnd, filePath, hostAndPath, ".pak") ;
         file = fopen( segmentName, "rb" ) ;
      }

      if ( file == NULL )
      {
         return FALSE ;
      }
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
BOOL NEAR handleFileRequest( HWND hWnd, BYTE b, char* filePath, char* hostAndPath )
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
      wsprintf( message, "Segment %06lX, Packet %06X \r\n", segmentNumber, packetNumber ) ;
      WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;

      WriteCommByte(hWnd, 0xE4) ;

      freeLoadedPackets() ;

      if ( segmentNumber == 0x7fffff )
      {
         createTimeSegment();
      }
      // We will try local file access, then download.
          // Ugly code. Wow, this program is brand new and already needs a refactor.
      else if ( !loadFilePacket( hWnd, filePath, hostAndPath, FALSE ) )
      {
         if ( !createFilePacket( hWnd, filePath, hostAndPath, FALSE ) )
         {
            if ( !loadFilePacket( hWnd, filePath, hostAndPath, TRUE ) )
            {
               if ( !createFilePacket( hWnd, filePath, hostAndPath, TRUE ) )
               {
                   wsprintf( message, "Could not load segment %06X and packet %06X\r\n", segmentNumber, packetNumber );
                   WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
                   WriteCommByte( hWnd, 0x90 ) ;
                   processingStage = 5;
                   return TRUE;
               }
            }
         }
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
void NEAR processNABU( HWND hWnd, BYTE b, char* filePath, char* hostAndPath )
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
            wsprintf( message, "File Request: " ) ;
            WriteTTYBlock( hWnd, (LPSTR) message, strlen( message )) ;
         }
         if ( handleFileRequest( hWnd, b, filePath, hostAndPath ) )
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
         wsprintf( message, "Unrecognized command 0x%X\r\n", b ) ;
         WriteTTYBlock( hWnd, (LPSTR) message, strlen( message ) ) ;
         resetNabuState();

         // Let's try to execute the last inititialized command we had
         processNABU( hWnd, lastResetProcessingByte, filePath, hostAndPath );
         break ;
   }
}
