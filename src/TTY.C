//---------------------------------------------------------------------------
//
//  Module: tty.c
//
//  Purpose:
//     The sample application demonstrates the usage of the COMM
//     API.  It implements the new COMM API of Windows 3.1.
//
//     NOTE:  no escape sequences are translated, only
//            the necessary control codes (LF, CR, BS, etc.)
//
//  Description of functions:
//     Descriptions are contained in the function headers.
//
//  Development Team:
//     Bryan A. Woodruff
//
//  History:   Date       Author      Comment
//
//              5/ 8/91   BryanW      Wrote it.
//
//             10/18/91   BryanW      Sanitized code & comments.
//
//             10/22/91   BryanW      Minor bug fixes.
//
//              2/10/92   BryanW      Fixed a problem with ReadCommBlock()
//                                    and error detection.  ReadComm() is
//                                    now performed... if the result is < 0
//                                    the error status is read using
//                                    GetCommError() and displayed.  The
//                                    length is adjusted (*= -1).
//
//              2/13/92   BryanW      Updated to provide the option to
//                                    implement/handle CN_RECEIVE
//                                    notifications.
//
//              2/13/92   BryanW      Implemented forced DTR control. DTR
//                                    is asserted on connect and dropped
//                                    on disconnect.
//
//              2/21/92   BryanW      Removed overhead of local memory
//                                    functions - now uses LPTR for
//                                    allocation from heap.
//
//              2/26/92   BryanW      Fixed off by one problem in paint
//                                    calculations.
//
//---------------------------------------------------------------------------
//
//  Written by Microsoft Product Support Services, Windows Developer Support.
//  Copyright (c) 1991 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "tty.h"
#include "nabu.h"

// creates a directory for storing the cycles.
BOOL NEAR makeCycleDirectory( HWND hWnd, char* directory )
{
   struct stat st ;
   char message [ CYCLE_HOST_PATH_LENGTH + 80 ] ;
   char strip [ CYCLE_HOST_PATH_LENGTH ] ;

   // Strip off the trailing slash before making the directory
   strncpy( strip, directory, strlen( directory ) -1 ) ;
   strip[ strlen(directory) - 1 ] = 0 ;

   if ( stat( strip, &st ) == -1 )
   {
      if ( mkdir( strip ) == -1 )
      {
         sprintf( message, "Could not make cycle directory specificed in settings: \n%s %d", strip, strlen( directory) ) ;
         MessageBox( hWnd, message, gszAppName, MB_ICONEXCLAMATION ) ;
         return FALSE;
      }
   }
   return TRUE;
}

//---------------------------------------------------------------------------
//  int PASCAL WinMain( HANDLE hInstance, HANDLE hPrevInstance,
//                      LPSTR lpszCmdLine, int nCmdShow )
//
//  Description:
//     This is the main window loop!
//
//  Parameters:
//     As documented for all WinMain() functions.
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpszCmdLine, int nCmdShow )
{
   HWND  hTTYWnd ;
   MSG   msg ;

   if (!hPrevInstance)
      if (!InitApplication( hInstance ))
         return ( FALSE ) ;

   if (NULL == (hTTYWnd = InitInstance( hInstance, nCmdShow )))
      return ( FALSE ) ;

   while (GetMessage( &msg, NULL, 0, 0 ))
   {
      TranslateMessage( &msg ) ;
      DispatchMessage( &msg ) ;
   }
   return ( (int) msg.wParam ) ;

} // end of WinMain()

//---------------------------------------------------------------------------
//  BOOL NEAR InitApplication( HANDLE hInstance )
//
//  Description:
//     First time initialization stuff.  This registers information
//     such as window classes.
//
//  Parameters:
//     HANDLE hInstance
//        Handle to this instance of the application.
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//             10/22/91   BryanW      Fixed background color problem.
//
//---------------------------------------------------------------------------

BOOL NEAR InitApplication( HANDLE hInstance )
{
   WNDCLASS  wndclass ;

   // register tty window class

   wndclass.style =         NULL ;
   wndclass.lpfnWndProc =   TTYWndProc ;
   wndclass.cbClsExtra =    0 ;
   wndclass.cbWndExtra =    sizeof( WORD ) ;
   wndclass.hInstance =     hInstance ;
   wndclass.hIcon =         LoadIcon( hInstance, MAKEINTRESOURCE( TTYICON ) );
   wndclass.hCursor =       LoadCursor( NULL, IDC_ARROW ) ;
   wndclass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1) ;
   wndclass.lpszMenuName =  MAKEINTRESOURCE( TTYMENU ) ;
   wndclass.lpszClassName = gszTTYClass ;

   return( RegisterClass( &wndclass ) ) ;

} // end of InitApplication()

//---------------------------------------------------------------------------
//  HWND NEAR InitInstance( HANDLE hInstance, int nCmdShow )
//
//  Description:
//     Initializes instance specific information.
//
//  Parameters:
//     HANDLE hInstance
//        Handle to instance
//
//     int nCmdShow
//        How do we show the window?
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

HWND NEAR InitInstance( HANDLE hInstance, int nCmdShow )
{
   HWND  hTTYWnd ;

   // load accelerators
   ghAccel = LoadAccelerators( hInstance, MAKEINTRESOURCE( TTYACCEL ) ) ;

   // create the TTY window
   hTTYWnd = CreateWindow( gszTTYClass, gszAppName,
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           NULL, NULL, hInstance, NULL ) ;

   if (NULL == hTTYWnd)
      return ( NULL ) ;

   ShowWindow( hTTYWnd, nCmdShow ) ;
   UpdateWindow( hTTYWnd ) ;

   return ( hTTYWnd ) ;

} // end of InitInstance()

//---------------------------------------------------------------------------
//  LRESULT FAR PASCAL TTYWndProc( HWND hWnd, UINT uMsg,
//                                 WPARAM wParam, LPARAM lParam )
//
//  Description:
//     This is the TTY Window Proc.  This handles ALL messages
//     to the tty window.
//
//  Parameters:
//     As documented for Window procedures.
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

LRESULT FAR PASCAL TTYWndProc( HWND hWnd, UINT uMsg,
                               WPARAM wParam, LPARAM lParam )
{
   switch (uMsg)
   {
      case WM_CREATE:
         return ( CreateTTYInfo( hWnd ) ) ;

      case WM_COMMAND:
      {
         switch ((WORD) wParam)
         {
            case IDM_CONNECT:
               if (!OpenConnection( hWnd ))
                  MessageBox( hWnd, "Connection failed.", gszAppName,
                              MB_ICONEXCLAMATION ) ;
               break ;

            case IDM_DISCONNECT:
               CloseConnection( hWnd ) ;
               break ;

            case IDM_EXIT:
               PostMessage( hWnd, WM_CLOSE, NULL, 0L ) ;
               break ;
         }
      }
      break ;

      case WM_COMMNOTIFY:
         ProcessCOMMNotification( hWnd, (WORD) wParam, (LONG) lParam ) ;
         break ;

      case WM_PAINT:
         PaintTTY( hWnd ) ;
         break ;

      case WM_SIZE:
         SizeTTY( hWnd, HIWORD( lParam ), LOWORD( lParam ) ) ;
         break ;

      case WM_HSCROLL:
         ScrollTTYHorz( hWnd, (WORD) wParam, LOWORD( lParam ) ) ;
         break ;

      case WM_VSCROLL:
         ScrollTTYVert( hWnd, (WORD) wParam, LOWORD( lParam ) ) ;
         break ;

      case WM_SETFOCUS:
         SetTTYFocus( hWnd ) ;
         break ;

      case WM_KILLFOCUS:
         KillTTYFocus( hWnd ) ;
         break ;

      case WM_DESTROY:
         DestroyTTYInfo( hWnd ) ;
         resetNabuState();
         PostQuitMessage( 0 ) ;
         break ;

      case WM_CLOSE:
         if (IDOK != MessageBox( hWnd, "OK to close window?", gszAppName,
                                 MB_ICONQUESTION | MB_OKCANCEL ))
            break ;

         // fall through

      default:
         return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;
   }
   return 0L ;

} // end of TTYWndProc()

//---------------------------------------------------------------------------
//  LRESULT NEAR CreateTTYInfo( HWND hWnd )
//
//  Description:
//     Creates the tty information structure and sets
//     menu option availability.  Returns -1 if unsuccessful.
//
//  Parameters:
//     HWND  hWnd
//        Handle to main window.
//
//  History:   Date       Author      Comment
//             10/18/91   BryanW      Pulled from tty window proc.
//              1/13/92   BryanW      Fixed bug with invalid handle
//                                    caused by WM_SIZE sent by
//                                    ResetTTYScreen().
//
//---------------------------------------------------------------------------

LRESULT NEAR CreateTTYInfo( HWND hWnd )
{
   HMENU       hMenu ;
   NPTTYINFO   npTTYInfo ;

   if (NULL == (npTTYInfo =
                   (NPTTYINFO) LocalAlloc( LPTR, sizeof( TTYINFO ) )))
      return ( (LRESULT) -1 ) ;

   // initialize TTY info structure

   COMDEV( npTTYInfo )        = 0 ;
   CONNECTED( npTTYInfo )     = FALSE ;
   CURSORSTATE( npTTYInfo )   = CS_HIDE ;
   AUTOWRAP( npTTYInfo )      = TRUE ;
   PORT( npTTYInfo )          = 1 ;
   BAUDRATE( npTTYInfo )      = 123137 ;
   BYTESIZE( npTTYInfo )      = 8 ;
   FLOWCTRL( npTTYInfo )      = 0 ;
   PARITY( npTTYInfo )        = NOPARITY ;
   STOPBITS( npTTYInfo )      = TWOSTOPBITS ;
   XONXOFF( npTTYInfo )       = FALSE ;
   XSIZE( npTTYInfo )         = 0 ;
   YSIZE( npTTYInfo )         = 0 ;
   XSCROLL( npTTYInfo )       = 0 ;
   YSCROLL( npTTYInfo )       = 0 ;
   XOFFSET( npTTYInfo )       = 0 ;
   YOFFSET( npTTYInfo )       = 0 ;
   COLUMN( npTTYInfo )        = 0 ;
   ROW( npTTYInfo )           = 0 ;
   HTTYFONT( npTTYInfo )      = NULL ;
   FGCOLOR( npTTYInfo )       = RGB( 0, 0, 0 ) ;
   USECNRECEIVE( npTTYInfo )  = TRUE ;
   DISPLAYERRORS( npTTYInfo ) = FALSE ;
   CYCLEPATH( npTTYInfo )     = ( char* ) LocalAlloc( LPTR, CYCLE_HOST_PATH_LENGTH) ;
   strcpy( CYCLEPATH( npTTYInfo ), "C:\\cycle\\" ) ;

   // clear screen space

   _fmemset( SCREEN( npTTYInfo ), ' ', MAXROWS * MAXCOLS ) ;

   // setup default font information

   LFTTYFONT( npTTYInfo ).lfHeight =         12 ;
   LFTTYFONT( npTTYInfo ).lfWidth =          0 ;
   LFTTYFONT( npTTYInfo ).lfEscapement =     0 ;
   LFTTYFONT( npTTYInfo ).lfOrientation =    0 ;
   LFTTYFONT( npTTYInfo ).lfWeight =         0 ;
   LFTTYFONT( npTTYInfo ).lfItalic =         0 ;
   LFTTYFONT( npTTYInfo ).lfUnderline =      0 ;
   LFTTYFONT( npTTYInfo ).lfStrikeOut =      0 ;
   LFTTYFONT( npTTYInfo ).lfCharSet =        OEM_CHARSET ;
   LFTTYFONT( npTTYInfo ).lfOutPrecision =   OUT_DEFAULT_PRECIS ;
   LFTTYFONT( npTTYInfo ).lfClipPrecision =  CLIP_DEFAULT_PRECIS ;
   LFTTYFONT( npTTYInfo ).lfQuality =        DEFAULT_QUALITY ;
   LFTTYFONT( npTTYInfo ).lfPitchAndFamily = FIXED_PITCH | FF_MODERN ;
   LFTTYFONT( npTTYInfo ).lfFaceName[0] =    NULL ;

   // set TTYInfo handle before any further message processing.

   SetWindowWord( hWnd, GWW_NPTTYINFO, (WPARAM) npTTYInfo ) ;

   // reset the character information, etc.

   ResetTTYScreen( hWnd, npTTYInfo ) ;

   hMenu = GetMenu( hWnd ) ;
   EnableMenuItem( hMenu, IDM_DISCONNECT,
                   MF_GRAYED | MF_DISABLED | MF_BYCOMMAND ) ;
   EnableMenuItem( hMenu, IDM_CONNECT, MF_ENABLED | MF_BYCOMMAND ) ;

    // Always start out in connected mode if possible
   if ( !OpenConnection( hWnd ) ) {
      MessageBox( hWnd, "Connection failed.", gszAppName,
      MB_ICONEXCLAMATION ) ;
   }

   return ( (LRESULT) TRUE ) ;

} // end of CreateTTYInfo()

//---------------------------------------------------------------------------
//  BOOL NEAR DestroyTTYInfo( HWND hWnd )
//
//  Description:
//     Destroys block associated with TTY window handle.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR DestroyTTYInfo( HWND hWnd )
{
   NPTTYINFO npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   // force connection closed (if not already closed)

   if (CONNECTED( npTTYInfo ))
      CloseConnection( hWnd ) ;

   DeleteObject( HTTYFONT( npTTYInfo ) ) ;
   LocalFree( (HLOCAL) CYCLEPATH( npTTYInfo )) ;

   LocalFree( npTTYInfo ) ;
   return ( TRUE ) ;

} // end of DestroyTTYInfo()

//---------------------------------------------------------------------------
//  BOOL NEAR ResetTTYScreen( HWND hWnd, NPTTYINFO npTTYInfo )
//
//  Description:
//     Resets the TTY character information and causes the
//     screen to resize to update the scroll information.
//
//  Parameters:
//     NPTTYINFO  npTTYInfo
//        pointer to TTY info structure
//
//  History:   Date       Author      Comment
//             10/20/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR ResetTTYScreen( HWND hWnd, NPTTYINFO npTTYInfo )
{
   HDC         hDC ;
   TEXTMETRIC  tm ;
   RECT        rcWindow ;

   if (NULL == npTTYInfo)
      return ( FALSE ) ;

   if (NULL != HTTYFONT( npTTYInfo ))
      DeleteObject( HTTYFONT( npTTYInfo ) ) ;

   HTTYFONT( npTTYInfo ) = CreateFontIndirect( &LFTTYFONT( npTTYInfo ) ) ;

   hDC = GetDC( hWnd ) ;
   SelectObject( hDC, HTTYFONT( npTTYInfo ) ) ;
   GetTextMetrics( hDC, &tm ) ;
   ReleaseDC( hWnd, hDC ) ;

   XCHAR( npTTYInfo ) = tm.tmAveCharWidth  ;
   YCHAR( npTTYInfo ) = tm.tmHeight + tm.tmExternalLeading ;

   // a slimy hack to force the scroll position, region to
   // be recalculated based on the new character sizes

   GetWindowRect( hWnd, &rcWindow ) ;
   SendMessage( hWnd, WM_SIZE, SIZENORMAL,
                (LPARAM) MAKELONG( rcWindow.right - rcWindow.left,
                                   rcWindow.bottom - rcWindow.top ) ) ;

   return ( TRUE ) ;

} // end of ResetTTYScreen()

//---------------------------------------------------------------------------
//  BOOL NEAR PaintTTY( HWND hWnd )
//
//  Description:
//     Paints the rectangle determined by the paint struct of
//     the DC.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window (as always)
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//             10/22/91   BryanW      Problem with background color
//                                    and "off by one" fixed.
//
//              2/25/92   BryanW      Off-by-one not quite fixed...
//                                    also resolved min/max problem
//                                    for windows extended beyond
//                                    the "TTY display".
//
//---------------------------------------------------------------------------

BOOL NEAR PaintTTY( HWND hWnd )
{
   int          nRow, nCol, nEndRow, nEndCol, nCount, nHorzPos, nVertPos ;
   HDC          hDC ;
   HFONT        hOldFont ;
   NPTTYINFO    npTTYInfo ;
   PAINTSTRUCT  ps ;
   RECT         rect ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   hDC = BeginPaint( hWnd, &ps ) ;
   hOldFont = SelectObject( hDC, HTTYFONT( npTTYInfo ) ) ;
   SetTextColor( hDC, FGCOLOR( npTTYInfo ) ) ;
   SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) ) ;
   rect = ps.rcPaint ;
   nRow =
      min( MAXROWS - 1,
           max( 0, (rect.top + YOFFSET( npTTYInfo )) / YCHAR( npTTYInfo ) ) ) ;
   nEndRow =
      min( MAXROWS - 1,
           ((rect.bottom + YOFFSET( npTTYInfo ) - 1) / YCHAR( npTTYInfo ) ) ) ;
   nCol =
      min( MAXCOLS - 1,
           max( 0, (rect.left + XOFFSET( npTTYInfo )) / XCHAR( npTTYInfo ) ) ) ;
   nEndCol =
      min( MAXCOLS - 1,
           ((rect.right + XOFFSET( npTTYInfo ) - 1) / XCHAR( npTTYInfo ) ) ) ;
   nCount = nEndCol - nCol + 1 ;
   for (; nRow <= nEndRow; nRow++)
   {
      nVertPos = (nRow * YCHAR( npTTYInfo )) - YOFFSET( npTTYInfo ) ;
      nHorzPos = (nCol * XCHAR( npTTYInfo )) - XOFFSET( npTTYInfo ) ;
      rect.top = nVertPos ;
      rect.bottom = nVertPos + YCHAR( npTTYInfo ) ;
      rect.left = nHorzPos ;
      rect.right = nHorzPos + XCHAR( npTTYInfo ) * nCount ;
      SetBkMode( hDC, OPAQUE ) ;
      ExtTextOut( hDC, nHorzPos, nVertPos, ETO_OPAQUE | ETO_CLIPPED, &rect,
                  (LPSTR)( SCREEN( npTTYInfo ) + nRow * MAXCOLS + nCol ),
                  nCount, NULL ) ;
   }
   SelectObject( hDC, hOldFont ) ;
   EndPaint( hWnd, &ps ) ;
   MoveTTYCursor( hWnd ) ;
   return ( TRUE ) ;

} // end of PaintTTY()

//---------------------------------------------------------------------------
//  BOOL NEAR SizeTTY( HWND hWnd, WORD wVertSize, WORD wHorzSize )
//
//  Description:
//     Sizes TTY and sets up scrolling regions.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     WORD wVertSize
//        new vertical size
//
//     WORD wHorzSize
//        new horizontal size
//
//  History:   Date       Author      Comment
//              5/ 8/ 91  BryanW      Wrote it
//
//---------------------------------------------------------------------------

BOOL NEAR SizeTTY( HWND hWnd, WORD wVertSize, WORD wHorzSize )
{
   int        nScrollAmt ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   YSIZE( npTTYInfo ) = (int) wVertSize ;
   YSCROLL( npTTYInfo ) = max( 0, (MAXROWS * YCHAR( npTTYInfo )) -
                               YSIZE( npTTYInfo ) ) ;
   nScrollAmt = min( YSCROLL( npTTYInfo ), YOFFSET( npTTYInfo ) ) -
                     YOFFSET( npTTYInfo ) ;
   ScrollWindow( hWnd, 0, -nScrollAmt, NULL, NULL ) ;

   YOFFSET( npTTYInfo ) = YOFFSET( npTTYInfo ) + nScrollAmt ;
   SetScrollPos( hWnd, SB_VERT, YOFFSET( npTTYInfo ), FALSE ) ;
   SetScrollRange( hWnd, SB_VERT, 0, YSCROLL( npTTYInfo ), TRUE ) ;

   XSIZE( npTTYInfo ) = (int) wHorzSize ;
   XSCROLL( npTTYInfo ) = max( 0, (MAXCOLS * XCHAR( npTTYInfo )) -
                                XSIZE( npTTYInfo ) ) ;
   nScrollAmt = min( XSCROLL( npTTYInfo ), XOFFSET( npTTYInfo )) -
                     XOFFSET( npTTYInfo ) ;
   ScrollWindow( hWnd, 0, -nScrollAmt, NULL, NULL ) ;
   XOFFSET( npTTYInfo ) = XOFFSET( npTTYInfo ) + nScrollAmt ;
   SetScrollPos( hWnd, SB_HORZ, XOFFSET( npTTYInfo ), FALSE ) ;
   SetScrollRange( hWnd, SB_HORZ, 0, XSCROLL( npTTYInfo ), TRUE ) ;

   InvalidateRect( hWnd, NULL, TRUE ) ;

   return ( TRUE ) ;

} // end of SizeTTY()

//---------------------------------------------------------------------------
//  BOOL NEAR ScrollTTYVert( HWND hWnd, WORD wScrollCmd, WORD wScrollPos )
//
//  Description:
//     Scrolls TTY window vertically.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     WORD wScrollCmd
//        type of scrolling we're doing
//
//     WORD wScrollPos
//        scroll position
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR ScrollTTYVert( HWND hWnd, WORD wScrollCmd, WORD wScrollPos )
{
   int        nScrollAmt ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   switch (wScrollCmd)
   {
      case SB_TOP:
         nScrollAmt = -YOFFSET( npTTYInfo ) ;
         break ;

      case SB_BOTTOM:
         nScrollAmt = YSCROLL( npTTYInfo ) - YOFFSET( npTTYInfo ) ;
         break ;

      case SB_PAGEUP:
         nScrollAmt = -YSIZE( npTTYInfo ) ;
         break ;

      case SB_PAGEDOWN:
         nScrollAmt = YSIZE( npTTYInfo ) ;
         break ;

      case SB_LINEUP:
         nScrollAmt = -YCHAR( npTTYInfo ) ;
         break ;

      case SB_LINEDOWN:
         nScrollAmt = YCHAR( npTTYInfo ) ;
         break ;

      case SB_THUMBPOSITION:
         nScrollAmt = wScrollPos - YOFFSET( npTTYInfo ) ;
         break ;

      default:
         return ( FALSE ) ;
   }
   if ((YOFFSET( npTTYInfo ) + nScrollAmt) > YSCROLL( npTTYInfo ))
      nScrollAmt = YSCROLL( npTTYInfo ) - YOFFSET( npTTYInfo ) ;
   if ((YOFFSET( npTTYInfo ) + nScrollAmt) < 0)
      nScrollAmt = -YOFFSET( npTTYInfo ) ;
   ScrollWindow( hWnd, 0, -nScrollAmt, NULL, NULL ) ;
   YOFFSET( npTTYInfo ) = YOFFSET( npTTYInfo ) + nScrollAmt ;
   SetScrollPos( hWnd, SB_VERT, YOFFSET( npTTYInfo ), TRUE ) ;

   return ( TRUE ) ;

} // end of ScrollTTYVert()

//---------------------------------------------------------------------------
//  BOOL NEAR ScrollTTYHorz( HWND hWnd, WORD wScrollCmd, WORD wScrollPos )
//
//  Description:
//     Scrolls TTY window horizontally.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     WORD wScrollCmd
//        type of scrolling we're doing
//
//     WORD wScrollPos
//        scroll position
//
//  History:   Date       Author      Comment
//              5/ 8/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR ScrollTTYHorz( HWND hWnd, WORD wScrollCmd, WORD wScrollPos )
{
   int        nScrollAmt ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   switch (wScrollCmd)
   {
      case SB_TOP:
         nScrollAmt = -XOFFSET( npTTYInfo ) ;
         break ;

      case SB_BOTTOM:
         nScrollAmt = XSCROLL( npTTYInfo ) - XOFFSET( npTTYInfo ) ;
         break ;

      case SB_PAGEUP:
         nScrollAmt = -XSIZE( npTTYInfo ) ;
         break ;

      case SB_PAGEDOWN:
         nScrollAmt = XSIZE( npTTYInfo ) ;
         break ;

      case SB_LINEUP:
         nScrollAmt = -XCHAR( npTTYInfo ) ;
         break ;

      case SB_LINEDOWN:
         nScrollAmt = XCHAR( npTTYInfo ) ;
         break ;

      case SB_THUMBPOSITION:
         nScrollAmt = wScrollPos - XOFFSET( npTTYInfo ) ;
         break ;

      default:
         return ( FALSE ) ;
   }
   if ((XOFFSET( npTTYInfo ) + nScrollAmt) > XSCROLL( npTTYInfo ))
      nScrollAmt = XSCROLL( npTTYInfo ) - XOFFSET( npTTYInfo ) ;
   if ((XOFFSET( npTTYInfo ) + nScrollAmt) < 0)
      nScrollAmt = -XOFFSET( npTTYInfo ) ;
   ScrollWindow( hWnd, -nScrollAmt, 0, NULL, NULL ) ;
   XOFFSET( npTTYInfo ) = XOFFSET( npTTYInfo ) + nScrollAmt ;
   SetScrollPos( hWnd, SB_HORZ, XOFFSET( npTTYInfo ), TRUE ) ;

   return ( TRUE ) ;

} // end of ScrollTTYHorz()

//---------------------------------------------------------------------------
//  BOOL NEAR SetTTYFocus( HWND hWnd )
//
//  Description:
//     Sets the focus to the TTY window also creates caret.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR SetTTYFocus( HWND hWnd )
{
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   if (CONNECTED( npTTYInfo ) && (CURSORSTATE( npTTYInfo ) != CS_SHOW))
   {
      CreateCaret( hWnd, NULL, XCHAR( npTTYInfo ), YCHAR( npTTYInfo ) ) ;
      ShowCaret( hWnd ) ;
      CURSORSTATE( npTTYInfo ) = CS_SHOW ;
   }
   MoveTTYCursor( hWnd ) ;
   return ( TRUE ) ;

} // end of SetTTYFocus()

//---------------------------------------------------------------------------
//  BOOL NEAR KillTTYFocus( HWND hWnd )
//
//  Description:
//     Kills TTY focus and destroys the caret.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR KillTTYFocus( HWND hWnd )
{
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   if (CONNECTED( npTTYInfo ) && (CURSORSTATE( npTTYInfo ) != CS_HIDE))
   {
      HideCaret( hWnd ) ;
      DestroyCaret() ;
      CURSORSTATE( npTTYInfo ) = CS_HIDE ;
   }
   return ( TRUE ) ;

} // end of KillTTYFocus()

//---------------------------------------------------------------------------
//  BOOL NEAR MoveTTYCursor( HWND hWnd )
//
//  Description:
//     Moves caret to current position.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR MoveTTYCursor( HWND hWnd )
{
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   if (CONNECTED( npTTYInfo ) && (CURSORSTATE( npTTYInfo ) & CS_SHOW))
      SetCaretPos( (COLUMN( npTTYInfo ) * XCHAR( npTTYInfo )) -
                   XOFFSET( npTTYInfo ),
                   (ROW( npTTYInfo ) * YCHAR( npTTYInfo )) -
                   YOFFSET( npTTYInfo ) ) ;

   return ( TRUE ) ;

} // end of MoveTTYCursor()

//---------------------------------------------------------------------------
//  BOOL NEAR ProcessCOMMNotification( HWND hWnd, WORD wParam, LONG lParam )
//
//  Description:
//     Processes the WM_COMMNOTIFY message from the COMM.DRV.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     WORD wParam
//        specifes the device (nCid)
//
//     LONG lParam
//        LOWORD contains event trigger
//        HIWORD is NULL
//
//  History:   Date       Author      Comment
//              5/10/91   BryanW      Wrote it.
//             10/18/91   BryanW      Updated to verify the event.
//
//---------------------------------------------------------------------------

BOOL NEAR ProcessCOMMNotification( HWND hWnd, WORD wParam, LONG lParam )
{
   char       szError[ 20 ] ;
   int        nError, nLength ;
   BYTE       abIn ;
   COMSTAT    ComStat ;
   NPTTYINFO  npTTYInfo ;
   MSG        msg ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   if (!USECNRECEIVE( npTTYInfo ))
   {
      // verify that it is a COMM event specified by our mask

      if (CN_EVENT & LOWORD( lParam ) != CN_EVENT)
         return ( FALSE ) ;

      // reset the event word so we are notified
      // when the next event occurs

      GetCommEventMask( COMDEV( npTTYInfo ), EV_RXCHAR ) ;

      // We loop here since it is highly likely that the buffer
      // can been filled while we are reading this block.  This
      // is especially true when operating at high baud rates
      // (e.g. >= 9600 baud).

      do
      {
         if ( nLength = ReadCommByte( hWnd, &abIn ))
         {
            processNABU(hWnd, abIn, CYCLEPATH( npTTYInfo ) ) ;

            // force a paint

            UpdateWindow( hWnd ) ;
         }
      }
      while (!PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE) ||
             (nLength > 0)) ;

   }
   else
   {
      // verify that it is a receive event

      if (CN_RECEIVE & LOWORD( lParam ) != CN_RECEIVE)
         return ( FALSE ) ;

      do
      {
         if ( nLength = ReadCommByte( hWnd, &abIn ))
         {
            processNABU(hWnd, abIn, CYCLEPATH( npTTYInfo ) ) ;

            // force a paint

            UpdateWindow( hWnd ) ;
         }
         if (nError = GetCommError( COMDEV( npTTYInfo ), &ComStat ))
         {
            if (DISPLAYERRORS( npTTYInfo ))
            {
               wsprintf( szError, "<CE-%04x>", nError ) ;
               WriteTTYBlock( hWnd, szError, lstrlen( szError ) ) ;
            }
         }
      }
      while ((!PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE )) ||
            (ComStat.cbInQue >= MAXBLOCK)) ;
   }
   return ( TRUE ) ;

} // end of ProcessCOMMNotification()


//---------------------------------------------------------------------------
//  BOOL NEAR OpenConnection( HWND hWnd )
//
//  Description:
//     Opens communication port specified in the TTYINFO struct.
//     It also sets the CommState and notifies the window via
//     the fConnected flag in the TTYINFO struct.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR OpenConnection( HWND hWnd )
{
   char       szPort[ 10 ] ;
   char       szTemp[ 10 ] = "COM";
   BOOL       fRetVal ;
   HCURSOR    hOldCursor, hWaitCursor ;
   HMENU      hMenu ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   if ( !makeCycleDirectory ( hWnd, CYCLEPATH( npTTYInfo ) ) ) {
      return ( FALSE ) ;
   }

   // show the hourglass cursor
   hWaitCursor = LoadCursor( NULL, IDC_WAIT ) ;
   hOldCursor = SetCursor( hWaitCursor ) ;

   wsprintf( szPort, "%s%d", (LPSTR) szTemp, PORT( npTTYInfo ) ) ;

   // open COMM device

   if ((COMDEV( npTTYInfo ) = OpenComm( szPort, RXQUEUE, TXQUEUE )) < 0)
      return ( FALSE ) ;

   fRetVal = SetupConnection( hWnd ) ;

   if (fRetVal)
   {
      CONNECTED( npTTYInfo ) = TRUE ;

      // set up notifications from COMM.DRV

      if (!USECNRECEIVE( npTTYInfo ))
      {
         // In this case we really are only using the notifications
         // for the received characters - it could be expanded to
         // cover the changes in CD or other status lines.

         SetCommEventMask( COMDEV( npTTYInfo ), EV_RXCHAR ) ;

         // Enable notifications for events only.

         // NB:  This method does not use the specific
         // in/out queue triggers.

         EnableCommNotification( COMDEV( npTTYInfo ), hWnd, -1, -1 ) ;
      }
      else
      {
         // Enable notification for CN_RECEIVE events.

         EnableCommNotification( COMDEV( npTTYInfo ), hWnd, MAXBLOCK, -1 ) ;
      }

      // assert DTR

      EscapeCommFunction( COMDEV( npTTYInfo ), SETDTR ) ;


      SetTTYFocus( hWnd ) ;

      hMenu = GetMenu( hWnd ) ;
      EnableMenuItem( hMenu, IDM_DISCONNECT,
                      MF_ENABLED | MF_BYCOMMAND ) ;
      EnableMenuItem( hMenu, IDM_CONNECT,
                      MF_GRAYED | MF_DISABLED | MF_BYCOMMAND ) ;
   }
   else
   {
      CONNECTED( npTTYInfo ) = FALSE ;
      CloseComm( COMDEV( npTTYInfo ) ) ;
   }

   // restore cursor
   SetCursor( hOldCursor ) ;

   return ( fRetVal ) ;

} // end of OpenConnection()

//---------------------------------------------------------------------------
//  BOOL NEAR SetupConnection( HWND hWnd )
//
//  Description:
//     This routines sets up the DCB based on settings in the
//     TTY info structure and performs a SetCommState().
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR SetupConnection( HWND hWnd )
{
   BOOL       fRetVal ;
   BYTE       bSet ;
   DCB        dcb ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   GetCommState( COMDEV( npTTYInfo ), &dcb ) ;

   dcb.BaudRate = BAUDRATE( npTTYInfo ) ;
   dcb.ByteSize = BYTESIZE( npTTYInfo ) ;
   dcb.Parity = PARITY( npTTYInfo ) ;
   dcb.StopBits = STOPBITS( npTTYInfo ) ;

   // setup hardware flow control

   bSet = (BYTE) ((FLOWCTRL( npTTYInfo ) & FC_DTRDSR) != 0) ;
   dcb.fOutxDsrFlow = dcb.fDtrflow = bSet ;
   dcb.DsrTimeout = (bSet) ? 30 : 0 ;

   bSet = (BYTE) ((FLOWCTRL( npTTYInfo ) & FC_RTSCTS) != 0) ;
   dcb.fOutxCtsFlow = dcb.fRtsflow = bSet ;
   dcb.CtsTimeout = (bSet) ? 30 : 0 ;

   // setup software flow control

   bSet = (BYTE) ((FLOWCTRL( npTTYInfo ) & FC_XONXOFF) != 0) ;

   dcb.fInX = dcb.fOutX = bSet ;
   dcb.XonChar = ASCII_XON ;
   dcb.XoffChar = ASCII_XOFF ;
   dcb.XonLim = 100 ;
   dcb.XoffLim = 100 ;

   // other various settings

   dcb.fBinary = TRUE ;
   dcb.fParity = TRUE ;
   dcb.fRtsDisable = FALSE ;
   dcb.fDtrDisable = FALSE ;

   fRetVal = !(SetCommState( &dcb ) < 0) ;

   return ( fRetVal ) ;

} // end of SetupConnection()

//---------------------------------------------------------------------------
//  BOOL NEAR CloseConnection( HWND hWnd )
//
//  Description:
//     Closes the connection to the port.  Resets the connect flag
//     in the TTYINFO struct.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR CloseConnection( HWND hWnd )
{
   HMENU      hMenu ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   // Disable event notification.  Using a NULL hWnd tells
   // the COMM.DRV to disable future notifications.

   EnableCommNotification( COMDEV( npTTYInfo ), NULL, -1, -1 ) ;

   // kill the focus

   KillTTYFocus( hWnd ) ;

   // drop DTR

   EscapeCommFunction( COMDEV( npTTYInfo ), CLRDTR ) ;

   // close comm connection

   CloseComm( COMDEV( npTTYInfo ) ) ;
   CONNECTED( npTTYInfo ) = FALSE ;

   // change the selectable items in the menu

   hMenu = GetMenu( hWnd ) ;
   EnableMenuItem( hMenu, IDM_DISCONNECT,
                   MF_GRAYED | MF_DISABLED | MF_BYCOMMAND ) ;
   EnableMenuItem( hMenu, IDM_CONNECT,
                   MF_ENABLED | MF_BYCOMMAND ) ;

   resetNabuState();
   return ( TRUE ) ;

} // end of CloseConnection()

//---------------------------------------------------------------------------
//  int NEAR ReadCommBlock( HWND hWnd, LPSTR lpszBlock, int nMaxLength )
//
//  Description:
//     Reads a block from the COM port and stuffs it into
//     the provided block.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     LPSTR lpszBlock
//        block used for storage
//
//     int nMaxLength
//        max length of block to read
//
//  History:   Date       Author      Comment
//              5/10/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

int NEAR ReadCommBlock( HWND hWnd, LPSTR lpszBlock, int nMaxLength )
{
   char       szError[ 20 ] ;
   int        nLength, nError ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   nLength = ReadComm( COMDEV( npTTYInfo ), lpszBlock, nMaxLength ) ;

   if (nLength < 0)
   {
      nLength *= -1 ;
      while (nError = GetCommError( COMDEV( npTTYInfo ), NULL ))
      {
         if (DISPLAYERRORS( npTTYInfo ))
         {
            wsprintf( szError, "<CE-%04x>", nError ) ;
            WriteTTYBlock( hWnd, szError, lstrlen( szError ) ) ;
         }
      }
   }

   return ( nLength ) ;

} // end of ReadCommBlock()

//---------------------------------------------------------------------------
//  BOOL NEAR ReadCommByte( HWND hWnd, BYTE* bByte )
//
//  Description:
//     Reads a byte from the COM port and stuffs it into
//     the provided byte.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     BYTE*
//        byte used for storage
//
//  History:   Date       Author      Comment
//             12/23/24   ChrisL      Wrote it.
//
//---------------------------------------------------------------------------

int NEAR ReadCommByte ( HWND hWnd, BYTE* bByte )
{
   char       szError[ 20 ] ;
   int        nLength, nError ;
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   nLength = ReadComm( COMDEV( npTTYInfo ), bByte, 1 ) ;

   if (nLength < 0)
   {
      nLength *= -1 ;
      while (nError = GetCommError( COMDEV( npTTYInfo ), NULL ))
      {
         if (DISPLAYERRORS( npTTYInfo ))
         {
            wsprintf( szError, "<CE-%04x>", nError ) ;
            WriteTTYBlock( hWnd, szError, lstrlen( szError ) ) ;
         }
      }
   }

   return ( nLength ) ;

} // end of ReadCommByte()


//---------------------------------------------------------------------------
//  BOOL NEAR WriteCommByte( HWND hWnd, BYTE bByte )
//
//  Description:
//     Writes a byte to the COM port specified in the associated
//     TTY info structure.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     BYTE bByte
//        byte to write to port
//
//  History:   Date       Author      Comment
//              5/10/91   BryanW      Wrote it.
//
//---------------------------------------------------------------------------

BOOL NEAR WriteCommByte( HWND hWnd, BYTE bByte )
{
   NPTTYINFO  npTTYInfo ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   WriteComm( COMDEV( npTTYInfo ), (LPSTR) &bByte, 1 ) ;
   return ( TRUE ) ;

} // end of WriteCommByte()

//---------------------------------------------------------------------------
//  BOOL NEAR WriteCommBlock( HWND hWnd, BYTE* bByte, int nByteLen )
//
//  Description:
//     Writes a series of bytes to the COM port specified in the associated
//     TTY info structure.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     BYTE* bByte
//        byte array to write to port
//
//     int nByteLen
//        byte array length to write to port
//
//
//  History:   Date       Author      Comment
//             12/22/24   CLenderman Wrote it.
//
//---------------------------------------------------------------------------
BOOL NEAR WriteCommBlock( HWND hWnd, BYTE* bByte, int nByteLen )
{
   NPTTYINFO  npTTYInfo ;
   int remaining = nByteLen;
   BYTE* bytePtr = bByte;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO ))) {
      return ( FALSE ) ;
   }

   while (TRUE) {
      if (remaining < TXQUEUE ) {
         WriteComm( COMDEV( npTTYInfo ), (LPSTR) bytePtr, remaining) ;
         break;
      }
      else {
         WriteComm( COMDEV( npTTYInfo ), (LPSTR) bytePtr, TXQUEUE) ;
         remaining = remaining - TXQUEUE;
         bytePtr += TXQUEUE;
      }
   }

   return ( TRUE ) ;

} // end of WriteCommBlock()

//---------------------------------------------------------------------------
//  BOOL NEAR WriteTTYBlock( HWND hWnd, LPSTR lpBlock, int nLength )
//
//  Description:
//     Writes block to TTY screen.  Nothing fancy - just
//     straight TTY.
//
//  Parameters:
//     HWND hWnd
//        handle to TTY window
//
//     LPSTR lpBlock
//        far pointer to block of data
//
//     int nLength
//        length of block
//
//  History:   Date       Author      Comment
//              5/ 9/91   BryanW      Wrote it.
//              5/20/91   BryanW      Modified... not character based,
//                                    block based now.  It was processing
//                                    per char.
//
//---------------------------------------------------------------------------

BOOL NEAR WriteTTYBlock( HWND hWnd, LPSTR lpBlock, int nLength )
{
   int        i ;
   NPTTYINFO  npTTYInfo ;
   RECT       rect ;

   if (NULL == (npTTYInfo = (NPTTYINFO) GetWindowWord( hWnd, GWW_NPTTYINFO )))
      return ( FALSE ) ;

   for (i = 0 ; i < nLength; i++)
   {
      switch (lpBlock[ i ])
      {
         case ASCII_BEL:
            // Bell
            MessageBeep( 0 ) ;
            break ;

         case ASCII_BS:
            // Backspace
            if (COLUMN( npTTYInfo ) > 0)
               COLUMN( npTTYInfo ) -- ;
            MoveTTYCursor( hWnd ) ;
            break ;

         case ASCII_CR:
            // Carriage return
            COLUMN( npTTYInfo ) = 0 ;
            MoveTTYCursor( hWnd ) ;
            if (!NEWLINE( npTTYInfo ))
               break;

            // fall through

         case ASCII_LF:
            // Line feed
            if (ROW( npTTYInfo )++ == MAXROWS - 1)
            {
               _fmemmove( (LPSTR) (SCREEN( npTTYInfo )),
                          (LPSTR) (SCREEN( npTTYInfo ) + MAXCOLS),
                          (MAXROWS - 1) * MAXCOLS ) ;
               _fmemset( (LPSTR) (SCREEN( npTTYInfo ) + (MAXROWS - 1) * MAXCOLS),
                         ' ', MAXCOLS ) ;
               InvalidateRect( hWnd, NULL, FALSE ) ;
               ROW( npTTYInfo )-- ;
            }
            MoveTTYCursor( hWnd ) ;
            break ;

         default:
            *(SCREEN( npTTYInfo ) + ROW( npTTYInfo ) * MAXCOLS +
                COLUMN( npTTYInfo )) = lpBlock[ i ] ;
            rect.left = (COLUMN( npTTYInfo ) * XCHAR( npTTYInfo )) -
                        XOFFSET( npTTYInfo ) ;
            rect.right = rect.left + XCHAR( npTTYInfo ) ;
            rect.top = (ROW( npTTYInfo ) * YCHAR( npTTYInfo )) -
                       YOFFSET( npTTYInfo ) ;
            rect.bottom = rect.top + YCHAR( npTTYInfo ) ;
            InvalidateRect( hWnd, &rect, FALSE ) ;

            // Line wrap
            if (COLUMN( npTTYInfo ) < MAXCOLS - 1)
               COLUMN( npTTYInfo )++ ;
            else if (AUTOWRAP( npTTYInfo ))
               WriteTTYBlock( hWnd, "\r\n", 2 ) ;
            break;
      }
   }
   return ( TRUE ) ;

} // end of WriteTTYBlock()

//---------------------------------------------------------------------------
//  End of File: tty.c
//---------------------------------------------------------------------------
