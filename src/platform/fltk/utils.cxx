// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <config.h>
#include <sys/socket.h>
#include "lib/str.h"
#include "utils.h"

#define RX_BUFFER_SIZE 1024

Fl_Color get_color(int argb) {
  // Fl_Color => 0xrrggbbii
  return (argb << 8) & 0xffffff00;
}

Fl_Color get_color(const char *name, Fl_Color def) {
  Fl_Color result = def;
  if (!name || name[0] == '\0') {
    result = def;
  } else if (name[0] == '#') {
    // do hex color lookup
    int rgb = strtol(name + 1, NULL, 16);
    if (!rgb) {
      result = FL_BLACK;
    } else {
      uchar r = rgb >> 16;
      uchar g = (rgb >> 8) & 255;
      uchar b = rgb & 255;
      result = fl_rgb_color(r, g, b);
    }
  } else if (strcasecmp(name, "black") == 0) {
    result = FL_BLACK;
  } else if (strcasecmp(name, "red") == 0) {
    result = FL_RED;
  } else if (strcasecmp(name, "green") == 0) {
    result = fl_rgb_color(0, 0x80, 0);
  } else if (strcasecmp(name, "yellow") == 0) {
    result = FL_YELLOW;
  } else if (strcasecmp(name, "blue") == 0) {
    result = FL_BLUE;
  } else if (strcasecmp(name, "magenta") == 0 ||
             strcasecmp(name, "fuchsia") == 0) {
    result = FL_MAGENTA;
  } else if (strcasecmp(name, "cyan") == 0 ||
             strcasecmp(name, "aqua") == 0) {
    result = FL_CYAN;
  } else if (strcasecmp(name, "white") == 0) {
    result = FL_WHITE;
  } else if (strcasecmp(name, "gray") == 0 ||
             strcasecmp(name, "grey") == 0) {
    result = fl_rgb_color(0x80, 0x80, 0x80);
  } else if (strcasecmp(name, "lime") == 0) {
    result = FL_GREEN;
  } else if (strcasecmp(name, "maroon") == 0) {
    result = fl_rgb_color(0x80, 0, 0);
  } else if (strcasecmp(name, "navy") == 0) {
    result = fl_rgb_color(0, 0, 0x80);
  } else if (strcasecmp(name, "olive") == 0) {
    result = fl_rgb_color(0x80, 0x80, 0);
  } else if (strcasecmp(name, "purple") == 0) {
    result = fl_rgb_color(0x80, 0, 0x80);
  } else if (strcasecmp(name, "silver") == 0) {
    result = fl_rgb_color(0xc0, 0xc0, 0xc0);
  } else if (strcasecmp(name, "teal") == 0) {
    result = fl_rgb_color(0, 0x80, 0x80);
  }
  return result;
}

Fl_Font get_font(const char *name) {
  Fl_Font result = FL_COURIER;
  if (strcasecmp(name, "helvetica") == 0) {
    result = FL_HELVETICA;
  } else if (strcasecmp(name, "times") == 0) {
    result = FL_TIMES;
  }
  return result;
}

void getHomeDir(char *fileName, size_t size, bool appendSlash) {
  const int homeIndex = 1;
  static const char *envVars[] = {
    "APPDATA", "HOME", "TMP", "TEMP", "TMPDIR", ""
  };

  fileName[0] = '\0';

  for (int i = 0; envVars[i][0] != '\0' && fileName[0] == '\0'; i++) {
    const char *home = getenv(envVars[i]);
    if (home && access(home, R_OK) == 0) {
      strlcpy(fileName, home, size);
      if (i == homeIndex) {
        strlcat(fileName, "/.config", size);
        makedir(fileName);
      }
      strlcat(fileName, "/SmallBASIC", size);
      if (appendSlash) {
        strlcat(fileName, "/", size);
      }
      makedir(fileName);
      break;
    }
  }
}

// copy the url into the local cache
bool cacheLink(dev_file_t *df, char *localFile, size_t size) {
  char rxbuff[RX_BUFFER_SIZE];
  FILE *fp;
  const char *url = df->name;
  const char *pathBegin = strchr(url + 7, '/');
  const char *pathEnd = strrchr(url + 7, '/');
  const char *pathNext;
  bool inHeader = true;
  bool httpOK = false;

  getHomeDir(localFile, size, true);
  strcat(localFile, "/cache/");
  makedir(localFile);

  // create host name component
  strncat(localFile, url + 7, pathBegin - url - 7);
  strcat(localFile, "/");
  makedir(localFile);

  if (pathBegin != 0 && pathBegin < pathEnd) {
    // re-create the server path in cache
    int level = 0;
    pathBegin++;
    do {
      pathNext = strchr(pathBegin, '/');
      strncat(localFile, pathBegin, pathNext - pathBegin + 1);
      makedir(localFile);
      pathBegin = pathNext + 1;
    }
    while (pathBegin < pathEnd && ++level < 20);
  }
  if (pathEnd == 0 || pathEnd[1] == 0 || pathEnd[1] == '?') {
    strcat(localFile, "index.html");
  } else {
    strcat(localFile, pathEnd + 1);
  }

  fp = fopen(localFile, "wb");
  if (fp == 0) {
    if (df->handle != -1) {
      shutdown(df->handle, df->handle);
    }
    return false;
  }

  if (df->handle == -1) {
    // pass the cache file modified time to the HTTP server
    struct stat st;
    if (stat(localFile, &st) == 0) {
      df->drv_dw[2] = st.st_mtime;
    }
    if (http_open(df) == 0) {
      return false;
    }
  }

  while (true) {
    int bytes = recv(df->handle, (char *)rxbuff, sizeof(rxbuff), 0);
    if (bytes == 0) {
      break;                    // no more data
    }
    // assumes http header < 1024 bytes
    if (inHeader) {
      int i = 0;
      while (true) {
        int iattr = i;
        while (rxbuff[i] != 0 && rxbuff[i] != '\n') {
          i++;
        }
        if (rxbuff[i] == 0) {
          inHeader = false;
          break;                // no end delimiter
        }
        if (rxbuff[i + 2] == '\n') {
          if (!fwrite(rxbuff + i + 3, bytes - i - 3, 1, fp)) {
            break;
          }
          inHeader = false;
          break;                // found start of content
        }
        // null terminate attribute (in \r)
        rxbuff[i - 1] = 0;
        i++;
        if (strstr(rxbuff + iattr, "200 OK") != 0) {
          httpOK = true;
        }
        if (strncmp(rxbuff + iattr, "Location: ", 10) == 0) {
          // handle redirection
          shutdown(df->handle, df->handle);
          strcpy(df->name, rxbuff + iattr + 10);
          if (http_open(df) == 0) {
            fclose(fp);
            return false;
          }
          break;                // scan next header
        }
      }
    } else if (!fwrite(rxbuff, bytes, 1, fp)) {
      break;
    }
  }

  // cleanup
  fclose(fp);
  shutdown(df->handle, df->handle);
  return httpOK;
}

