// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "harness_wind.hpp"
#include "harness_task.hpp"
#include "harness_flight.hpp"
#include "Contest/Solvers/ContestDijkstra.hpp"
#include "test_debug.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

int n_samples = 0;
int interactive = 0;
int output_skip = 5;

AutopilotParameters autopilot_parms;

int terrain_height = 1;
AllocatedPath replay_file = Path("test/data/0asljd01.igc");
AllocatedPath waypoint_file = Path("test/data/waypoints_geo.wpt");
AllocatedPath task_file;
double range_threshold = 15000;

void PrintDistanceCounts() {
  if (n_samples) {
    printf("# Instrumentation\n");
    printf("#    (total cycles %d)\n#\n",n_samples);
  }
  n_samples = 0;
}

/** 
 * Wait-for-key prompt
 * 
 * @param time time of simulation
 * 
 * @return character received by keyboard
 */
char
WaitPrompt() {
  if (interactive) {
    puts("# [enter to continue]");
    return getchar();
  }
  return 0;
}

bool
ParseArgs(int argc, char** argv)
{
  // initialise random number generator once per test program
  srand(0);

  while (1)    {
    static struct option long_options[] =
      {
	/* These options set a flag. */
	{"verbose", optional_argument,       0, 'v'},
	{"interactive", optional_argument,   0, 'i'},
	{"startalt", required_argument,   0, 'a'},
	{"bearingnoise", required_argument,   0, 'n'},
	{"outputskip", required_argument,       0, 's'},
	{"targetnoise", required_argument,       0, 't'},
	{"turnspeed", required_argument,       0, 'r'},
	{"igc", required_argument,       0, 'f'},
	{"task", required_argument,       0, 'x'},
	{"waypoints", required_argument,       0, 'w'},
	{"rangethreshold", required_argument,       0, 'd'},
	{0, 0, 0, 0}
      };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    int c = getopt_long (argc, argv, "s:v:i:n:t:r:a:f:x:w:d:",
                         long_options, &option_index);
    /* Detect the end of the options. */
    if (c == -1)
      break;
    
    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
	break;
      printf ("option %s", long_options[option_index].name);
      if (optarg)
	printf (" with arg %s", optarg);
      printf ("\n");
      break;
    case 'f':
      replay_file = Path(optarg);
      break;
    case 'w':
      waypoint_file = Path(optarg);
      break;
    case 'x':
      task_file = Path(optarg);
      break;
    case 'd':
      range_threshold = atof(optarg);
      break;
    case 'a':
      autopilot_parms.start_alt = atof(optarg);
      break;
    case 's':
      output_skip = atoi(optarg);
      break;
    case 'v':
      if (optarg) {
        verbose = atoi(optarg);
      } else {
        verbose = 1;
      }
      break;
    case 'n':
      autopilot_parms.bearing_noise = atof(optarg);
      break;
    case 't':
      autopilot_parms.target_noise = atof(optarg);
      break;
    case 'r':
      autopilot_parms.turn_speed = atof(optarg);
      break;
    case 'i':
      if (optarg) {
        interactive = atoi(optarg);
      } else {
        interactive = 1;
      }
      break;
    case '?':
      /* getopt_long already printed an error message. */

      for (unsigned i=0; i+1< sizeof(long_options)/sizeof(option); i++) {
        switch (long_options[i].has_arg) {
        case 0:
          printf(" --%s %c\n", long_options[i].name, 
                 long_options[i].val);
          break;
        case 1:
          printf(" --%s -%c value\n", long_options[i].name, 
                 long_options[i].val);
          break;
        case 2:
          printf(" --%s -%c [value]\n", long_options[i].name, 
                 long_options[i].val);
        }
      }
      abort();
      return false;
      break;      
    default:
      return false;
    }
  }

  if (interactive && !verbose) {
    verbose=1;
  }

  return true;
}

const char* GetTestName(const char* in, int task_num, int wind_num)
{
  static char buffer[100];
  sprintf(buffer,"%s (task %s, wind %s)", in, task_name(task_num), wind_name(wind_num));
  return buffer;
}
