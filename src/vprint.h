#ifndef VAR_PRINT_H
#define VAR_PRINT_H

#include <stdint.h>
#include <stdlib.h>

struct vprint {
	const uint32_t *const var_options;
	const char *currPos;
	char *buffer;
	size_t remainingSize;
};

enum vprint_output {
	VPRINT_EOF = -1,
	VPRINT_ERROR = -2,
	VPRINT_MISSING_SIZE = -3,
	VPRINT_UNKNOWN_OPT = -4
};

int vprint_walk(struct vprint *ctx);
int vprint_strcat(struct vprint *ctx, const char *str);
int vprint_itoa(struct vprint *ctx, int value);
void vprint_collect_used(const char *str, uint32_t var_options[8]);

#endif // INI_PARSER_H
