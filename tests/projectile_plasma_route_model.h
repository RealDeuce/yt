#ifndef TEST_PROJECTILE_PLASMA_ROUTE_MODEL_H
#define TEST_PROJECTILE_PLASMA_ROUTE_MODEL_H

#include "yt_game.h"

enum test_projectile_plasma_impact_route {
	YT_PROJECTILE_PLASMA_NEXT_HOP,
	YT_PROJECTILE_PLASMA_FOOTER,
};

enum test_projectile_plasma_argument_change {
	YT_PROJECTILE_PLASMA_SAME_ORIGIN_ZERO,
	YT_PROJECTILE_PLASMA_BLACK_HOLE_ORIGIN,
	YT_PROJECTILE_PLASMA_BLACK_HOLE_DESTINATION,
};

struct test_projectile_plasma_route_ops {
	bool (*build_route)(void *context, float *origin, float *destination,
	    int16_t *route, size_t route_capacity, float *status,
	    struct yt_error *error);
	bool (*line)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*attention)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	bool (*wait)(void *context, float duration, struct yt_error *error);
	bool (*random)(void *context, float *value, struct yt_error *error);
	bool (*impact)(void *context, int hop, double *energy,
	    enum test_projectile_plasma_impact_route *route,
	    struct yt_error *error);
	bool (*footer)(void *context, const uint8_t *text, size_t length,
	    struct yt_error *error);
	int16_t (*read_route)(void *context, int16_t index);
	void (*write_route)(void *context, int16_t index, int16_t value);
	void (*arguments_changed)(void *context, float origin,
	    float destination,
	    enum test_projectile_plasma_argument_change change);
};

struct test_projectile_plasma_route_state {
	float *origin;
	float *destination;
	double *energy;
	float hop_loss;
	float black_hole[2];
	float sector_record_offset;
	float port_record_offset;
	int16_t *route;
	size_t route_capacity;
	size_t step_limit;
	float route_status;
	float current_hop;
	size_t route_calls;
	size_t hops;
};

bool test_projectile_plasma_route_run(
    struct test_projectile_plasma_route_state *state,
    const struct test_projectile_plasma_route_ops *ops, void *context,
    struct yt_error *error);

#endif
