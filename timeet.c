#include "timeet.h"

//+----------------------------------------------------------------------------------------------+

#ifdef __linux__

int handle_error(Display* display, XErrorEvent* error){
  printf("ERROR: X11 error\n");
  xerror = True;
  return 1;
}

//+----------------------------------------------------------------------------------------------+

Window GetTopWindow(Display* display, Window activeWindow){

  Window w = activeWindow;
  Window parent = activeWindow;
  Window root = None;
  Window *children;
  unsigned int nchildren;
  Status s;

  while (parent != root) {
    w = parent;
    s = XQueryTree(display, w, &root, &parent, &children, &nchildren);

    if (s)
      XFree(children);

    if(xerror){
      printf("ERROR: XQueryTree() failed.\n");
      exit(EXIT_FAILURE);
    }

  }

  return w;
}

//+----------------------------------------------------------------------------------------------+

Window GetNamedWindow(Display* display, Window activeWindow){

  Window w;

  w = XmuClientWindow(display, activeWindow);

  return w;
}

//+----------------------------------------------------------------------------------------------+

void CopyWindowName(Display* display, Window activeWindow, char* ByteBuffer){
  XTextProperty prop;
  Status s;
  size_t mbslen;

  s = XGetWMName(display, activeWindow, &prop);

  if(!xerror && s){
    int count = 0, result;
    char **list = NULL;
    result = Xutf8TextPropertyToTextList(display, &prop, &list, &count);

    if(result == Success){
      strcpy(ByteBuffer, list[0]);
    }else{
      printf("ERROR: XmbTextPropertyToTextList\n");
    }
  }
  else 
     printf("ERROR: XGetWMName\n");
}


//+----------------------------------------------------------------------------------------------+

void CopyWindowProcess(Display* display, Window activeWindow, char* ByteBuffer){

  Status s;
  XClassHint* class;

  class = XAllocClassHint();
  if(xerror){
    printf("ERROR: XAllocClassHint\n");
  }

  s = XGetClassHint(display, activeWindow, class);
  if(xerror || s){
    strcpy(ByteBuffer, "[AppName]: ");
    strcat(ByteBuffer, class->res_name);
    strcat(ByteBuffer, " | ");
    strcat(ByteBuffer, "[AppClass]: ");
    strcat(ByteBuffer, class->res_class);
  }

  else{
    printf("ERROR: XGetClassHint\n");
  }

}

#endif

//+----------------------------------------------------------------------------------------------+
//
//UTF-8 stuff for Windows
//
//+----------------------------------------------------------------------------------------------+

typedef uint32_t codepoint_t;


#define BMP_END 0xFFFF

#define UNICODE_MAX 0x10FFFF

#define INVALID_CODEPOINT 0xFFFD

#define GENERIC_SURROGATE_VALUE 0xD800

#define GENERIC_SURROGATE_MASK 0xF800

#define HIGH_SURROGATE_VALUE 0xD800

#define LOW_SURROGATE_VALUE 0xDC00

#define SURROGATE_MASK 0xFC00

#define SURROGATE_CODEPOINT_OFFSET 0x10000

#define SURROGATE_CODEPOINT_MASK 0x03FF

#define SURROGATE_CODEPOINT_BITS 10



#define UTF8_1_MAX 0x7F

#define UTF8_2_MAX 0x7FF

#define UTF8_3_MAX 0xFFFF

#define UTF8_4_MAX 0x10FFFF


#define UTF8_CONTINUATION_VALUE 0x80

#define UTF8_CONTINUATION_MASK 0xC0

#define UTF8_CONTINUATION_CODEPOINT_BITS 6

typedef struct
{
    utf8_t mask;
    utf8_t value;
} utf8_pattern;

static const utf8_pattern utf8_leading_bytes[] =
{
    { 0x80, 0x00 }, // 0xxxxxxx
    { 0xE0, 0xC0 }, // 110xxxxx
    { 0xF0, 0xE0 }, // 1110xxxx
    { 0xF8, 0xF0 }  // 11110xxx
};

#define UTF8_LEADING_BYTES_LEN 4

//+----------------------------------------------------------------------------------------------+

static codepoint_t decode_utf16(utf16_t const* utf16, size_t len, size_t* index){
    utf16_t high = utf16[*index];

    if ((high & GENERIC_SURROGATE_MASK) != GENERIC_SURROGATE_VALUE)
        return high; 

    if ((high & SURROGATE_MASK) != HIGH_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    if (*index == len - 1)
        return INVALID_CODEPOINT;
    
    utf16_t low = utf16[*index + 1];

    if ((low & SURROGATE_MASK) != LOW_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    (*index)++;

    codepoint_t result = high & SURROGATE_CODEPOINT_MASK;
    result <<= SURROGATE_CODEPOINT_BITS;
    result |= low & SURROGATE_CODEPOINT_MASK;
    result += SURROGATE_CODEPOINT_OFFSET;
    
    return result;
}

//+----------------------------------------------------------------------------------------------+

static int calculate_utf8_len(codepoint_t codepoint){
    if (codepoint <= UTF8_1_MAX)
        return 1;

    if (codepoint <= UTF8_2_MAX)
        return 2;

    if (codepoint <= UTF8_3_MAX)
        return 3;

    return 4;
}

//+----------------------------------------------------------------------------------------------+

static size_t encode_utf8(codepoint_t codepoint, utf8_t* utf8, size_t len, size_t index){
    int size = calculate_utf8_len(codepoint);

    if (index + size > len)
        return 0;

    for (int cont_index = size - 1; cont_index > 0; cont_index--)
    {
        utf8_t cont = codepoint & ~UTF8_CONTINUATION_MASK;
        cont |= UTF8_CONTINUATION_VALUE;

        utf8[index + cont_index] = cont;
        codepoint >>= UTF8_CONTINUATION_CODEPOINT_BITS;
    }

    utf8_pattern pattern = utf8_leading_bytes[size - 1];

    utf8_t lead = codepoint & ~(pattern.mask);
    lead |= pattern.value;

    utf8[index] = lead;

    return size;
}

//+----------------------------------------------------------------------------------------------+

size_t utf16_to_utf8(utf16_t const* utf16, size_t utf16_len, utf8_t* utf8, size_t utf8_len){
    size_t utf8_index = 0;

    for (size_t utf16_index = 0; utf16_index < utf16_len; utf16_index++)
    {
        codepoint_t codepoint = decode_utf16(utf16, utf16_len, &utf16_index);

        if (utf8 == NULL)
            utf8_index += calculate_utf8_len(codepoint);
        else
            utf8_index += encode_utf8(codepoint, utf8, utf8_len, utf8_index);
    }

    return utf8_index;
}

//+----------------------------------------------------------------------------------------------+

static codepoint_t decode_utf8(utf8_t const* utf8, size_t len, size_t* index){
    utf8_t leading = utf8[*index];


    int encoding_len = 0;

    utf8_pattern leading_pattern;

    bool matches = false;
    
    do
    {
        encoding_len++;
        leading_pattern = utf8_leading_bytes[encoding_len - 1];

        matches = (leading & leading_pattern.mask) == leading_pattern.value;

    } while (!matches && encoding_len < UTF8_LEADING_BYTES_LEN);

    if (!matches)
        return INVALID_CODEPOINT;

    codepoint_t codepoint = leading & ~leading_pattern.mask;

    for (int i = 0; i < encoding_len - 1; i++)
    {
        if (*index + 1 >= len)
            return INVALID_CODEPOINT;

        utf8_t continuation = utf8[*index + 1];

        if ((continuation & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_VALUE)
            return INVALID_CODEPOINT;

        codepoint <<= UTF8_CONTINUATION_CODEPOINT_BITS;
        codepoint |= continuation & ~UTF8_CONTINUATION_MASK;

        (*index)++;
    }

    int proper_len = calculate_utf8_len(codepoint);

    if (proper_len != encoding_len)
        return INVALID_CODEPOINT;

    if (codepoint < BMP_END && (codepoint & GENERIC_SURROGATE_MASK) == GENERIC_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    if (codepoint > UNICODE_MAX)
        return INVALID_CODEPOINT;

    return codepoint;
}

//+----------------------------------------------------------------------------------------------+

static int calculate_utf16_len(codepoint_t codepoint){
    if (codepoint <= BMP_END)
        return 1;

    return 2;
}

//+----------------------------------------------------------------------------------------------+

static size_t encode_utf16(codepoint_t codepoint, utf16_t* utf16, size_t len, size_t index){
    // Not enough space on the string
    if (index >= len)
        return 0;

    if (codepoint <= BMP_END)
    {
        utf16[index] = codepoint;
        return 1;
    }

    if (index + 1 >= len)
        return 0;

    codepoint -= SURROGATE_CODEPOINT_OFFSET;

    utf16_t low = LOW_SURROGATE_VALUE;
    low |= codepoint & SURROGATE_CODEPOINT_MASK;

    codepoint >>= SURROGATE_CODEPOINT_BITS;

    utf16_t high = HIGH_SURROGATE_VALUE;
    high |= codepoint & SURROGATE_CODEPOINT_MASK;

    utf16[index] = high;
    utf16[index + 1] = low;

    return 2;
}

//+----------------------------------------------------------------------------------------------+

size_t utf8_to_utf16(utf8_t const* utf8, size_t utf8_len, utf16_t* utf16, size_t utf16_len){

    size_t utf16_index = 0;

    for (size_t utf8_index = 0; utf8_index < utf8_len; utf8_index++)
    {
        codepoint_t codepoint = decode_utf8(utf8, utf8_len, &utf8_index);

        if (utf16 == NULL)
            utf16_index += calculate_utf16_len(codepoint);
        else
            utf16_index += encode_utf16(codepoint, utf16, utf16_len, utf16_index);
    }

    return utf16_index;
}

//+----------------------------------------------------------------------------------------------+

int get_window_name(char *ByteBuffer){

#if defined _WIN32 || defined _WIN64

  utf16_t title[MAX_BUFFER_SIZE] = { 0 };
  
  HWND hWnd = GetForegroundWindow();
  if(hWnd == NULL){
  	printf("ERROR: Couldn't get foreground window\n");
  	return -1;
  }

  GetWindowTextW(hWnd, title, MAX_BUFFER_SIZE);

  utf16_to_utf8(title, MAX_BUFFER_SIZE, (utf8_t*)ByteBuffer, MAX_BUFFER_SIZE);

#elif __linux__

  xerror = False;

  char *display_name = NULL;
  Window activeWindow;

  display = XOpenDisplay(display_name);

  if (display == NULL) {
        fprintf (stderr, "unable to open display '%s'\n", XDisplayName (display_name));
    }

  XSetErrorHandler(handle_error);

  int revert_to;

  XGetInputFocus(display, &activeWindow, &revert_to);

  if(xerror){
    printf("ERROR: XGetInputFocus() failed.\n");
    exit(EXIT_FAILURE);
  }
  else if(activeWindow == None){
    printf("There is no window in focus.\n");
    exit(EXIT_FAILURE);
  }

  activeWindow = GetTopWindow(display, activeWindow);
  activeWindow = GetNamedWindow(display, activeWindow);

  CopyWindowName(display, activeWindow, ByteBuffer);

  XCloseDisplay(display);

#elif __APPLE_CC__

 CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
 CFIndex numWindows = CFArrayGetCount(windowList);

 for(int i = 0; i < (int)numWindows; i++){

    CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);

    CFStringRef appName = (CFStringRef)CFDictionaryGetValue(info, kCGWindowName);

    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer), kCFNumberIntType, &layer);

    if(appName != 0){

        CFStringGetCString(appName, ByteBuffer, MAX_BUFFER_SIZE, kCFStringEncodingUTF8);
        break;
    }
 }

 CFRelease(windowList);

#endif


  return 0;

}

//+----------------------------------------------------------------------------------------------+

int get_process_name(char* ByteBuffer){

#if defined _WIN32 || defined _WIN64
  
  PROCESSENTRY32 ProcessEntry;
  HANDLE sHandle;
  DWORD ProcessID, ThreadID;
  
  HWND hWnd = GetForegroundWindow();
  ThreadID = GetWindowThreadProcessId(hWnd, &ProcessID);

 //get active processes list
  sHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if(sHandle == INVALID_HANDLE_VALUE){
  	 printf("ERROR: Couldn't get process list.\n");
  	 return -1;
  }

  ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

  if(!Process32First(sHandle, &ProcessEntry)){
  	 CloseHandle(sHandle);
  	 printf("ERROR: Couldn't start get process info.\n");
  	 return -2;
  }

  do{

  	DWORD currentPID = ProcessEntry.th32ProcessID;

  	if(currentPID == ProcessID){
  		strcpy(ByteBuffer, ProcessEntry.szExeFile);
  		break;
  	}

  }while(Process32Next(sHandle, &ProcessEntry));

 #elif __linux__

  xerror = False;

  char *display_name = NULL;
  Window activeWindow;

  display = XOpenDisplay(display_name);

  if (display == NULL) {
        fprintf (stderr, "unable to open display '%s'\n", XDisplayName (display_name));
    }

  XSetErrorHandler(handle_error);

  int revert_to;

  XGetInputFocus(display, &activeWindow, &revert_to);

  if(xerror){
    printf("ERROR: XGetInputFocus() failed.\n");
    exit(EXIT_FAILURE);
  }
  else if(activeWindow == None){
    printf("There is no window in focus.\n");
    exit(EXIT_FAILURE);
  }

  activeWindow = GetTopWindow(display, activeWindow);
  activeWindow = GetNamedWindow(display, activeWindow);

  CopyWindowProcess(display, activeWindow, ByteBuffer);

  XCloseDisplay(display);

#elif __APPLE_CC__

 CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowLIstOptionOnScreenOnly, kCGNullWindowID);
 CFIndex numWindows = CFArrayGetCount(windowList);

 for(int i = 0; i < (int)numWindows; i++){

    CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);

    CFStringRef appName = (CFStringRef)CFDictionaryGetValue(info, kCGWindowOwnerName);

    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer), kCFNumberIntType, &layer);

    if(appName != 0){

        CFStringGetCString(appName, ByteBuffer, MAX_BUFFER_SIZE, kCFStringEncodingUTF8);
        break;
    }
 }

 CFRelease(windowList);

#endif

    return 0;
}