/**
 ******************************************************************************
 * @file           : feb_string_utils.h
 * @brief          : FEB String Utilities Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_STRING_UTILS_H
#define FEB_STRING_UTILS_H

/**
 * @brief Case-insensitive string comparison
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int FEB_strcasecmp(const char *s1, const char *s2);

#endif /* FEB_STRING_UTILS_H */
