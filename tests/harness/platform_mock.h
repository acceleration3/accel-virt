#ifndef AV_PLATFORM_MOCK_H
#define AV_PLATFORM_MOCK_H
#include <stdbool.h>
#include <stdint.h>
#include "../../src/core/platform.h"

void     av_mock_reset(void);
int      av_mock_leaks(void);
uint32_t av_mock_alloc_count(void);

/* Fail the Nth allocation from now (1-based). 0 disables. This is how error
 * paths reach full branch coverage instead of being assumed correct. */
void av_mock_fail_alloc_at(uint32_t nth);

void av_mock_set_irql(av_irql_t irql);

/* Set when a call violated its IRQL annotation, instead of aborting, so the
 * violation itself can be unit tested. */
bool av_mock_irql_violated(void);

#endif
