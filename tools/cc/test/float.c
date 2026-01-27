#include "test.h"

float add_float(float a, float b) {
	return a + b;
}

double add_double(double a, double b) {
	return a + b;
}

int main(void) {
	// Type conversions: int to float/double (cast table tests)
	ASSERT(35, (float)(char)35);
	ASSERT(35, (float)(short)35);
	ASSERT(35, (float)(int)35);
	ASSERT(35, (float)(long)35);
	ASSERT(35, (float)(unsigned char)35);
	ASSERT(35, (float)(unsigned short)35);
	ASSERT(35, (float)(unsigned int)35);
	ASSERT(35, (float)(unsigned long)35);

	ASSERT(35, (double)(char)35);
	ASSERT(35, (double)(short)35);
	ASSERT(35, (double)(int)35);
	ASSERT(35, (double)(long)35);
	ASSERT(35, (double)(unsigned char)35);
	ASSERT(35, (double)(unsigned short)35);
	ASSERT(35, (double)(unsigned int)35);
	ASSERT(35, (double)(unsigned long)35);

	// Type conversions: float/double to int
	ASSERT(35, (char)(float)35);
	ASSERT(35, (short)(float)35);
	ASSERT(35, (int)(float)35);
	ASSERT(35, (long)(float)35);
	ASSERT(35, (unsigned char)(float)35);
	ASSERT(35, (unsigned short)(float)35);
	ASSERT(35, (unsigned int)(float)35);
	ASSERT(35, (unsigned long)(float)35);

	ASSERT(35, (char)(double)35);
	ASSERT(35, (short)(double)35);
	ASSERT(35, (int)(double)35);
	ASSERT(35, (long)(double)35);
	ASSERT(35, (unsigned char)(double)35);
	ASSERT(35, (unsigned short)(double)35);
	ASSERT(35, (unsigned int)(double)35);
	ASSERT(35, (unsigned long)(double)35);

	// Comparisons with double
	ASSERT(1, 2e3 == 2e3);
	ASSERT(0, 2e3 == 2e5);
	ASSERT(1, 2.0 == 2);
	ASSERT(0, 5.1 < 5);
	ASSERT(0, 5.0 < 5);
	ASSERT(1, 4.9 < 5);
	ASSERT(0, 5.1 <= 5);
	ASSERT(1, 5.0 <= 5);
	ASSERT(1, 4.9 <= 5);

	// Comparisons with float
	ASSERT(1, 2e3f == 2e3);
	ASSERT(0, 2e3f == 2e5);
	ASSERT(1, 2.0f == 2);
	ASSERT(0, 5.1f < 5);
	ASSERT(0, 5.0f < 5);
	ASSERT(1, 4.9f < 5);
	ASSERT(0, 5.1f <= 5);
	ASSERT(1, 5.0f <= 5);
	ASSERT(1, 4.9f <= 5);

	// Arithmetic with double
	ASSERT(6, 2.3 + 3.8);
	ASSERT(-1, 2.3 - 3.8);
	ASSERT(-3, -3.8);
	ASSERT(13, 3.3 * 4);
	ASSERT(2, 5.0 / 2);

	// Arithmetic with float
	ASSERT(6, 2.3f + 3.8f);
	ASSERT(6, 2.3f + 3.8);
	ASSERT(-1, 2.3f - 3.8);
	ASSERT(-3, -3.8f);
	ASSERT(13, 3.3f * 4);
	ASSERT(2, 5.0f / 2);

	// NaN comparisons
	ASSERT(0, 0.0 / 0.0 == 0.0 / 0.0);
	ASSERT(1, 0.0 / 0.0 != 0.0 / 0.0);

	ASSERT(0, 0.0 / 0.0 < 0);
	ASSERT(0, 0.0 / 0.0 <= 0);

	// Boolean context
	ASSERT(0, !3.);
	ASSERT(1, !0.);
	ASSERT(0, !3.f);
	ASSERT(1, !0.f);

	// Ternary with float condition
	ASSERT(5, 0.0 ? 3 : 5);
	ASSERT(3, 1.2 ? 3 : 5);

	// Float function arguments
	ASSERT(5, add_float(2.0f, 3.0f));
	ASSERT(10, add_double(4.0, 6.0));

	// Float local variables
	ASSERT(7, ({ float x = 3.5; float y = 3.5; x + y; }));
	ASSERT(12, ({ double x = 5.5; double y = 6.5; x + y; }));

	printf("OK\n");
	poweroff();
	return 0;
}
