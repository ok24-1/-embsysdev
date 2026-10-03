#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_LEN_ERROR   -1
#define TIME_ARRAY_ERROR -2
#define TIME_VALUE_ERROR -3
#define TIME_FORMAT_ERROR -4

int time_parse(const char *time);

#ifdef __cplusplus
}
#endif

#endif /* TIME_PARSER_H */
