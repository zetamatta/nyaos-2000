#ifndef NTCONSOLE_H
#define NTCONSOLE_H

class NnString;
class Console {
public:
    static void backspace(int n=1);
    static void clear();
    static unsigned int getShiftKey();
    enum { SHIFT = 0x8000 };
#ifdef NYACUS
    static void restore_default_console_mode();
    static void getLocate(int &x,int &y);
    static void color(int c);
    static int color();
    static int getWidth();
#endif
    /* enum{ SHIFT = 0x3 }; */
    static void locate(int x,int y);
    static int getkey();
    static void writeClipBoard( const char *s , int len );
    static void readClipBoard( NnString &buffer );
    static void readTextVram( int x , int y , char *buffer , int size );
    static void setConsoleTitle( const char *s );
};

#endif
