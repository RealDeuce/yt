#ifndef CONFIG_TEST_SUPPORT_H
#define CONFIG_TEST_SUPPORT_H

#include "yt_config.h"

void test_config_encode(struct yt_config *config);
bool test_config_store(struct yt_database *database,
    const struct yt_config *config, struct yt_error *error);

#endif
