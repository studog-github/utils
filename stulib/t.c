#include <stdio.h>

#include "stulib.h"

struct _q {
	int one;
	char two;
	unsigned int three;
	char four;
};

int main(int argc, char **argv)
{
	struct _q q;

	(void)argc;
	(void)argv;

	printf("q: %p\n", &q);
	printf("q.one: %p    container: %p\n", &q.one, containerof(&q.one, struct _q, one));
	printf("q.two: %p    container: %p\n", &q.two, containerof(&q.two, struct _q, two));
	printf("q.three: %p  container: %p\n", &q.three, containerof(&q.three, struct _q, three));
	printf("q.four: %p   container: %p\n", &q.four, containerof(&q.four, struct _q, four));

	return 0;
}
