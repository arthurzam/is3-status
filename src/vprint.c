#include "vprint.h"

#include <stdio.h>
#include <string.h>

int vprint_walk(struct vprint *ctx) {
	const char *next = strchr(ctx->currPos, '%');
	if (next == NULL)
		return vprint_strcat(ctx, ctx->currPos);
	const size_t len = (size_t)(next - ctx->currPos);
	if (ctx->remainingSize <= len)
		return VPRINT_MISSING_SIZE;
	memcpy(ctx->buffer, ctx->currPos, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
	ctx->currPos += len + 2;
	const char n = *(next + 1);
	if (0 != (ctx->var_options[n >> 5] & (1 << (n & 0x1F))))
		return n;
	else if (n == '%') {
		ctx->buffer[0] = '%';
		ctx->buffer[1] = '\0';
		ctx->buffer += 1;
		ctx->remainingSize -= 1;
		return n;
	} else
		return VPRINT_UNKNOWN_OPT;
}

int vprint_strcat(struct vprint *ctx, const char *str) {
	size_t len = strlen(str);
	if (ctx->remainingSize <= len)
		return VPRINT_MISSING_SIZE;
	memcpy(ctx->buffer, str, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
	return VPRINT_EOF;
}

int vprint_itoa(struct vprint *ctx, int value) {
	int len = snprintf(ctx->buffer, ctx->remainingSize, "%d", value);
	if (len < 0 || (unsigned)len >= ctx->remainingSize)
		return VPRINT_MISSING_SIZE;
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= (unsigned)len;
	return VPRINT_EOF;
}

void vprint_collect_used(const char *str, uint32_t var_options[8]) {
	while ((str = strchr(str, '%'))) {
		if (*(++str) == '\0')
			return;
		var_options[(*str) >> 5] |= (1 << ((*str) & 0x1F));
		++str;
	}
}
