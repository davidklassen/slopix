int main(void) {
	return
	    // Basic arithmetic
	    ((5 + 20 - 4) - 21) +
	    ((5 + 6 * 7) - 47) +
	    ((5 * (9 - 6)) - 15) +
	    (((3 + 5) / 2) - 4) +
	    ((-10 + 20) - 10) +
	    ((- -10) - 10) +

	    // Comparisons (1=true, 0=false)
	    ((0 == 1) - 0) +
	    ((42 == 42) - 1) +
	    ((0 != 1) - 1) +
	    ((0 < 1) - 1) +
	    ((1 < 1) - 0) +
	    ((0 <= 1) - 1) +
	    ((1 <= 1) - 1) +

	    // Logical NOT
	    ((!1) - 0) +
	    ((!0) - 1) +

	    // Bitwise NOT
	    ((~0) - (-1)) +
	    ((~-1) - 0) +

	    // Modulo
	    ((17 % 6) - 5) +

	    // Bitwise AND/OR/XOR
	    ((3 & 1) - 1) +
	    ((7 & 3) - 3) +
	    ((0 | 1) - 1) +
	    ((16 | 3) - 19) +
	    ((0 ^ 0) - 0) +
	    ((15 ^ 15) - 0) +

	    // Shifts
	    ((1 << 3) - 8) +
	    ((5 << 1) - 10) +
	    ((5 >> 1) - 2) +
	    ((-1 >> 1) - (-1)) + // Arithmetic shift

	    // Local variables
	    (({ int a; a=3; a; }) - 3) +
	    (({ int a=3; a; }) - 3) +
	    (({ int a=3; int b=5; a+b; }) - 8) +
	    (({ int a; int b; a=b=3; a+b; }) - 6) +

	    // Different integer sizes
	    (({ char x=1; x; }) - 1) +
	    (({ short x=2; x; }) - 2) +
	    (({ long x=3; x; }) - 3) +

	    // If-else
	    (({ int x; if (1){ x=2;
} else{ x=3;
} x; }) - 2) +
	    (({ int x; if (0){ x=2;
} else{ x=3;
} x; }) - 3) +
	    (({ int x; if (1-1){ x=2;
} else{ x=3;
} x; }) - 3) +
	    (({ int x; if (2-1){ x=2;
} else{ x=3;
} x; }) - 2) +

	    // For loop
	    (({ int i=0; int s=0; for(;i<5;i=i+1){ s=s+i;
} s; }) - 10) +
	    (({ int s=0; for(int i=0;i<5;i=i+1){ s=s+i;
} s; }) - 10) +

	    // While loop
	    (({ int i=0; while(i<5){ i=i+1;
} i; }) - 5) +
	    (({ int i=0; int s=0; while(i<=5) { s=s+i; i=i+1; } s; }) - 15) +

	    // Do-while loop
	    (({ int i=0; do { i=i+1; } while(i<5); i; }) - 5) +

	    // Break
	    (({ int i=0; for(;i<10;i=i+1) { if (i==3){ break;
} } i; }) - 3) +

	    // Logical operators
	    ((1 && 2) - 1) +
	    ((0 && 1) - 0) +
	    ((1 && 0) - 0) +
	    ((0 || 1) - 1) +
	    ((1 || 0) - 1) +
	    ((0 || 0) - 0) +

	    // Ternary operator
	    ((1 ? 2 : 3) - 2) +
	    ((0 ? 2 : 3) - 3) +

	    // Comma operator
	    ((1, 2, 3) - 3) +

	    0;
}
