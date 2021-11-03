#include <stdint.h>

#define MAX_COMP_LONG   13
#define MAX_COMP_SHORT  5
#define P               "POOR"
#define P_TO_F          "POOR_TO_FAIR"
#define F               "FAIR"
#define F_TO_G          "FAIR_TO_GOOD"
#define G               "GOOD"
#define G_TO_E          "GOOD_TO_EPIC"
#define E               "EPIC"

typedef struct Rating {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t num_leds;
} Rating;