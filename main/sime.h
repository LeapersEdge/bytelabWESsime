#ifndef _SIME_H_
#define _SIME_H_

typedef enum {
    SIME_MOOD_VERY_HAPPY = 0,
    SIME_MOOD_NEUTRAL,
    SIME_MOOD_UNHAPPY,
    SIME_MOOD_VERY_UNHAPPY
} sime_mood_t;

typedef enum {
    SIME_CLEAN_STATUS_CLEAN = 0,
    SIME_CLEAN_STATUS_DIRTY
} sime_clean_status_t;

void sime_init(void);

int sime_get_hp(void);
sime_mood_t sime_get_mood(void);
const char *sime_get_mood_str(void);

sime_clean_status_t sime_get_clean_status(void);
const char *sime_get_clean_status_str(void);

void sime_feed_full(void);
void sime_feed_half(void);
void sime_clean(void);
void sime_poll(void);

#endif