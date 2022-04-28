/*
 * Key-value pair string parser test
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "kernel_utils.h"

#include <hwemu.h>

#define streq(s1, s2) (strcmp((s1), (s2)) == 0)

/*! Returns a random number in range @min...@max */
static int rnd(int min, int max)
{
	return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

static void create_random_pair(struct hwe_pair * pair)
{
	int i;

	pair->req_size = rnd(1, HWE_MAX_REQUEST);

	for (i = 0; i < pair->req_size; i++)
		pair->req[i] = rnd(0, 255);

	pair->resp_size = rnd(1, HWE_MAX_RESPONSE);

	for (i = 0; i < pair->resp_size; i++)
		pair->resp[i] = rnd(0, 255);
}

static int test(int count)
{
	int ok = 1;
	int i;

	printf("Repeating the test %d times(s) ...\n", count);

	for (i = 0; i < count && ok; i++) {
		const char * err;
		struct hwe_pair p1;
		struct hwe_pair p2;
		const char * ps1;

		/* pair1 -> str1 -> pair2 -> str2 -> strcmp(str1, str2) */

		create_random_pair(&p1);

		ps1 = pair_to_str(&p1);

		err = str_to_pair(ps1, strlen(ps1), &p2);

		if (err) {
			printf("*** ERROR: %s\n\n", err);
			printf("%s\n", ps1);
			ok = 0;
		}
		else {
			char * ps2;

			ps2 = strdup(ps1);

			ps1 = pair_to_str(&p2);

			ok = strcmp(ps2, ps1) == 0;

			if (!ok) {
				printf("*** ERROR: pair string mismatch!\n\n");
				printf("%s\n\n", ps2);
				printf("%s\n", ps1);
			}

			free(ps2);
		}
	}

	if (ok)
		puts("Passed.");

	return ok;
}

static int check_pair(const char * pair_str)
{
	struct hwe_pair p;
	const char * err;


	err = str_to_pair(pair_str, strlen(pair_str), &p);

	if (err)
		printf("%s\n", err);

	return !err;
}

static void print_usage(void)
{
	puts(
"Usage:\n"
"\n"
"  pair_parser --test [<count>]\n"
"  pair_parser -t [<count>]\n"
"\n"
"        Creates a random pair, and tests pair parser functions on it.\n"
"        The test is repeated <count> times. If <count> is not specified,\n"
"        a random <count> is assigned.\n"
"\n"
"  pair_parser --check <pair string>\n"
"  pair_parser -c <pair string>\n"
"\n"
"        Checks if <pair string> is valid. If yes, returns a zero exit\n"
"        status. Otherwise, an error message is printed and a non-zero\n"
"        exit status is returned.\n"
	);
}

int main(int argc, char **argv)
{
	int ok = 0;
	int i;

	srand(time(NULL));

	if (argc < 2)
		print_usage();
	else
	if (streq(argv[1], "-t") || streq(argv[1], "--test")) {
		int count;

		if (argc > 3) {
			printf("*** ERROR: wrong number of arguments\n\n");
			print_usage();
		}
		else
		if (argc == 3 && !sscanf(argv[2], "%i", &count)) {
			printf("*** ERROR: invalid repeat count\n\n");
			print_usage();
		}
		else {
			if (argc == 2) {
				puts("Repeat count not specified; assume random number.");

				count = rnd(1, 0xffff);
			}

			ok = test(count);
		}
	}
	else
	if (streq(argv[1], "-c") || streq(argv[1], "--check")) {
		if (argc == 3)
			ok = check_pair(argv[2]);
		else {
			printf("*** ERROR: wrong number of arguments\n\n");
			print_usage();
		}
	}
	else {
		if (!streq(argv[1], "-h") && !streq(argv[1], "--help"))
			printf("*** ERROR: unknown argument `%s'\n\n", argv[1]);

		print_usage();
	}

	return !ok;
}

