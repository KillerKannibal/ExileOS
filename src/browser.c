#include "browser.h"
#include "string.h"

char browser_url[BROWSER_MAX_URL];
char browser_content[BROWSER_MAX_CONTENT];

void browser_init() {
    strcpy(browser_url, "http://exile.os");
    strcpy(browser_content, "Welcome to the ExileOS Web Browser!\n\nThis application is now hooked into the Window Manager.\n\nStatus: Network Layer 2 (Ethernet) is active.\nPending: TCP/IP stack implementation to perform real HTTP requests.\n\nFuture updates will allow you to fetch raw data from your USB-tethered connection!");
}