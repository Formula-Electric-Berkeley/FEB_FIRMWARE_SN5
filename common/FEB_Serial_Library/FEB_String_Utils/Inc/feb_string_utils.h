/**
 ******************************************************************************
 * @file           : feb_string_utils.h
 * @brief          : FEB String Utilities Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_STRING_UTILS_H
#define FEB_STRING_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Case-insensitive string comparison
   * @param s1 First string
   * @param s2 Second string
   * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
   */
  int FEB_strcasecmp(const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif

#endif /* FEB_STRING_UTILS_H */
