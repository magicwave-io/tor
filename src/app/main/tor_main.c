/* Copyright 2001-2004 Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2021, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include <stdbool.h>
#ifdef ENABLE_RESTART_DEBUGGING
#include <stdlib.h>
#endif
#ifdef _WIN32
  #include <windows.h>
  #include <stdio.h>
#endif

/**
 * \file tor_main.c
 * \brief Stub module containing a main() function.
 *
 * We keep the main function in a separate module so that the unit
 * tests, which have their own main()s, can link against main.c.
 **/

int tor_main(int argc, char *argv[]);

/** We keep main() in a separate file so that our unit tests can use
 * functions from main.c.
 */
int
main(int argc, char *argv[])
{
  #ifdef _WIN32
    bool deallocConsole = false;
    const char * TOR_PLUS_CMD = getenv("TOR_PLUS_CMD");
    bool console = false;
    if (TOR_PLUS_CMD != NULL) {
      console = strcmp(TOR_PLUS_CMD, "1") == 0;
    }
    if (console) {
      const int pp_err_code = 941691932;
      if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
        if (AllocConsole() == 0) {
          return pp_err_code;
        }
        deallocConsole = true;
      }
      fpos_t pos = 0;
      fgetpos64(stdin, &pos);
      if (pos == -1) {
        freopen("CONIN$", "r", stdin);
      }
      fgetpos64(stdout, &pos);
      if (pos == -1) {
        freopen("CONOUT$", "w", stdout);
      }
      fgetpos64(stderr, &pos);
      if (pos == -1) {
        freopen("CONERR$", "w", stderr);
      }
    }
  #endif
  int r;
#ifdef ENABLE_RESTART_DEBUGGING
  int restart_count = getenv("TOR_DEBUG_RESTART") ? 1 : 0;
 again:
#endif
  r = tor_main(argc, argv);
  if (r < 0 || r > 255) {
    #ifdef _WIN32
    if (deallocConsole) {
      FreeConsole();
    }
    #endif
    return 1;
  }
#ifdef ENABLE_RESTART_DEBUGGING
  else if (r == 0 && restart_count--)
    goto again;
#endif
  else {
    #ifdef _WIN32
    if (deallocConsole) {
     FreeConsole();
    }
    #endif
    return r;
  }
}

