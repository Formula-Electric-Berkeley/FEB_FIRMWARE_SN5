/**
 ******************************************************************************
 * @file           : feb_string_utils.c
 * @brief          : FEB String Utilities Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_string_utils.h"
#include <ctype.h>

int FEB_strcasecmp(const char *s1, const char *s2)
{
  while (*s1 && *s2)
  {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0)
      return diff;
    s1++;
    s2++;
  }
  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
