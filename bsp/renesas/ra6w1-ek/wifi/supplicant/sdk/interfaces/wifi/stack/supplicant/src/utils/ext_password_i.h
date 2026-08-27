/*
 * External password backend - internal definitions
 * Copyright (c) 2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EXT_PASSWORD_I_H
#define EXT_PASSWORD_I_H

#include "ext_password.h"

struct ext_password_backend {
	const char *name;
	void * (*init)(const char *params);
	void (*deinit)(void *ctx);
	struct wpabuf * (*get)(void *ctx, const char *name);
};

struct wpabuf * ext_password_alloc(size_t len);
struct ext_password_data * ext_password_init(const char *backend,
					     const char *params);
void ext_password_deinit(struct ext_password_data *data);
struct wpabuf * ext_password_get(struct ext_password_data *data,
				 const char *name);
void ext_password_free(struct wpabuf *pw);

/* Available ext_password backends */

#ifdef CONFIG_EXT_PASSWORD_TEST
extern const struct ext_password_backend ext_password_test;
#endif /* CONFIG_EXT_PASSWORD_TEST */

#ifdef CONFIG_EXT_PASSWORD_FILE
extern const struct ext_password_backend ext_password_file;
#endif /* CONFIG_EXT_PASSWORD_FILE */

#endif /* EXT_PASSWORD_I_H */
