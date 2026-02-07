#ifndef STDARG_H
#define STDARG_H

#ifdef __GNUC__

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, ty) __builtin_va_arg(ap, ty)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#else

typedef struct {
	void *__stack;
	void *__gr_top;
	void *__vr_top;
	int __gr_offs;
	int __vr_offs;
} __va_list;

typedef __va_list va_list[1];

#define va_start(ap, last)                         \
	do {                                       \
		*(ap) = *(__va_list *)__va_area__; \
	} while (0)

#define va_end(ap)

static void *__va_arg_gp(__va_list *ap, int sz) {
	if (ap->__gr_offs >= 0) {
		void *p = ap->__stack;
		ap->__stack = (char *)p + ((sz + 7) & ~7);
		return p;
	}
	void *p = (char *)ap->__gr_top + ap->__gr_offs;
	ap->__gr_offs += 8;
	return p;
}

static void *__va_arg_fp(__va_list *ap, int sz) {
	if (ap->__vr_offs >= 0) {
		void *p = ap->__stack;
		ap->__stack = (char *)p + ((sz + 7) & ~7);
		return p;
	}
	void *p = (char *)ap->__vr_top + ap->__vr_offs;
	ap->__vr_offs += 16;
	return p;
}

#define va_arg(ap, ty)                                               \
	({                                                           \
		int klass = __builtin_reg_class(ty);                 \
		*(ty *)(klass == 0   ? __va_arg_gp(ap, sizeof(ty))   \
			: klass == 1 ? __va_arg_fp(ap, sizeof(ty))   \
				     : __va_arg_gp(ap, sizeof(ty))); \
	})

#define va_copy(dest, src) ((dest)[0] = (src)[0])

#endif /* __GNUC__ */

#endif
