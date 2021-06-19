#pragma once

#define BUFFER_SIZE_BYTES(buf)      sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf)   (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf)    (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf)  BUFFER_LAST_ELEMENT(buf) = 0

#define DRLTRACE_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

#define DRLTRACE_WARN(msg, ...) do { \
    fprintf(stderr, "WARNING: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
} while (0)

#define DRLTRACE_INFO(level, msg, ...) do { \
    if (op_verbose.get_value() >= level) {\
        fprintf(stderr, "INFO: " msg "\n", ##__VA_ARGS__);    \
        fflush(stderr); \
    }\
} while (0)

#undef BUFPRINT
#define BUFPRINT(buf, bufsz, sofar, len, ...) do { \
    drfront_status_t sc = drfront_bufprint(buf, bufsz, &(sofar), &(len), ##__VA_ARGS__); \
    if (sc != DRFRONT_SUCCESS) \
        DRLTRACE_ERROR("drfront_bufprint failed: %d\n", sc); \
    NULL_TERMINATE_BUFFER(buf); \
} while (0)