#ifndef BROWSER_H
#define BROWSER_H

#include <stdint.h>

#define BROWSER_MAX_URL 128
#define BROWSER_MAX_CONTENT 4096

extern char browser_url[BROWSER_MAX_URL];
extern char browser_content[BROWSER_MAX_CONTENT];

void browser_init();

#endif