/*
 * Copyright (c) 2020 Gerson Fernando Budke <nandojve@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(tagoio_http_post, CONFIG_TAGOIO_HTTP_POST_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/random/rand32.h>
#include <stdio.h>

#include "wifi.h"
#include "sockets.h"

static struct tagoio_context ctx;

static void response_cb(struct http_response *rsp,
												enum http_final_call final_data,
												void *user_data)
{
	if (final_data == HTTP_DATA_MORE)
	{
		LOG_DBG("Partial data received (%zd bytes)", rsp->data_len);
	}
	else if (final_data == HTTP_DATA_FINAL)
	{
		LOG_DBG("All the data received (%zd bytes)", rsp->data_len);
	}

	LOG_DBG("Response status %s", rsp->http_status);
}

#define LOWER_LIGHT 100
#define UPPER_LIGHT 1000
#define BASE_LIGHT 1.00f

static int collect_light_data(void)
{
	float light;

	/* Gera um valor de luminosidade entre 100 e 1000 lux */
	light = ((sys_rand32_get() % (UPPER_LIGHT - LOWER_LIGHT + 1)) + LOWER_LIGHT);
	light /= BASE_LIGHT;

	(void)snprintf(ctx.payload, sizeof(ctx.payload),
								 "{\"variable\": \"luminosity\","
								 "\"unit\": \"lux\",\"value\": %f}",
								 (double)light);

	/* LOG não imprime float diretamente #18351 */
	LOG_INF("Luminosity: %d lux", (int)light);

	return 0;
}

#define lower 20000
#define upper 100000
#define base 1000.00f

static int collect_data(void)
{
	float temp;

	/* Generate a temperature between 20 and 100 celsius degree */
	temp = ((sys_rand32_get() % (upper - lower + 1)) + lower);
	temp /= base;

	(void)snprintf(ctx.payload, sizeof(ctx.payload),
								 "{\"variable\": \"temperature\","
								 "\"unit\": \"c\",\"value\": %f}",
								 (double)temp);

	/* LOG doesn't print float #18351 */
	LOG_INF("Temp: %d C", (int)temp);

	return 0;
}

static void next_turn(void)
{
	/* Collect temperature data */
	if (collect_data() < 0)
	{
		LOG_INF("Error collecting temperature data.");
		return;
	}

	if (tagoio_connect(&ctx) < 0)
	{
		LOG_INF("No connection available.");
		return;
	}

	if (tagoio_http_push(&ctx, response_cb) < 0)
	{
		LOG_INF("Error pushing temperature data.");
		return;
	}

	k_sleep(K_SECONDS(1)); // Sleep for a second before collecting the next data

	/* Collect luminosity data */
	if (collect_light_data() < 0)
	{
		LOG_INF("Error collecting light data.");
		return;
	}

	if (tagoio_connect(&ctx) < 0)
	{
		LOG_INF("No connection available.");
		return;
	}

	if (tagoio_http_push(&ctx, response_cb) < 0)
	{
		LOG_INF("Error pushing light data.");
		return;
	}
}

void main(void)
{
	LOG_INF("TagoIO IoT - HTTP Client - Sensor Data Demo");

	wifi_connect();

	while (true)
	{
		next_turn();
		k_sleep(K_SECONDS(CONFIG_TAGOIO_HTTP_PUSH_INTERVAL));
	}
}
