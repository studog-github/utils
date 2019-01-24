/*
 * This algorithm is copied from Wolfgang Denk's mktime() in the u-boot source.
 * It's a bit tricky, fasten your seatbelts.
 *
 *
 * The month is decremented by 2, causing March to become the first month in the
 * year. January and February become the 11th and 12th month. From this:
 *
 * 1971 | 1972                    | 1973
 * -----+-------------------------+-----------------------------------
 *  Dec | Jan Feb Mar Apr ... Dec | Jan Feb Mar Apr ...
 *
 * to this:
 *
 * 1971         | 1972                    | 1973
 * -------------+-------------------------+---------------------------
 *  Dec Jan Feb | Mar Apr ... Dec Jan Feb | Mar Apr ...
 *
 * (One way to look at the math inside the if-block is it's adding zero: added
 * 12 months, removed 1 year. So the overall quantity of time is preserved.)
 *
 * Now that Feb is at the end of the year, there is a useful property to the
 * year: a) Feb 29, 1971 is the last day of the previous year and automatically
 * includes the leap day, and b) Mar 1, 1972 is the first day of the next year
 * and the leap day is automatically included by the
 * '+ year/4 - year/100 + year/400' leap day calculation. Therefore, no explicit
 * leap day correction is required.
 *
 * The shifting of the year needs to be corrected for of course, that'll happen
 * in a moment.
 *
 * (1) epoch = year*365 + year/4 - year/100 + year/400;
 *
 * This calculates the number of days including leap days of complete years in
 * the past, taking advantage of the useful property discussed above.
 *
 * (2) epoch += 367*month/12 + day;
 *
 * This adds the complete days in the current year. There are a couple tricks
 * here.
 *
 * '367*month/12' is handy coincidence of math:
 *
 * 367* 1/12 =  30 ::  30 -   0 = 30 (Feb, imprecisely)
 * 367* 2/12 =  61 ::  61 -  30 = 31 (Mar)
 * 367* 3/12 =  91 ::  91 -  61 = 30 (Apr)
 * 367* 4/12 = 122 :: 122 -  91 = 31 (May)
 * 367* 5/12 = 152 :: 152 - 122 = 30 (Jun)
 * 367* 6/12 = 183 :: 183 - 152 = 31 (Jul)
 * 367* 7/12 = 214 :: 214 - 183 = 31 (Aug)
 * 367* 8/12 = 244 :: 244 - 214 = 30 (Sep)
 * 367* 9/12 = 275 :: 275 - 244 = 31 (Oct)
 * 367*10/12 = 305 :: 305 - 275 = 30 (Nov)
 * 367*11/12 = 336 :: 336 - 305 = 31 (Dec)
 * 367*12/12 = 367 :: 367 - 336 = 31 (Jan)
 *
 * It's easy to see that when the month increments it produces a difference of
 * days from the previous month that mimics the calendar's pattern, offset
 * slightly. This pattern is employed.
 *
 * Consider the firsts of the months, when the month rotation and year
 * adjustment is in effect:
 *
 * Jan 1 :: 367*11/12 + 1 +   0 = 337 :: 337 - 337 =  0       :: 337 - 337 =   0
 * Feb 1 :: 367*12/12 + 1 +   0 = 368 :: 368 - 337 = 31 (Jan) :: 368 - 337 =  31
 * Mar 1 :: 367* 1/12 + 1 + 365 = 396 :: 396 - 368 = 28 (Feb) :: 396 - 337 =  59
 * Apr 1 :: 367* 2/12 + 1 + 365 = 427 :: 427 - 396 = 31 (Mar) :: 427 - 337 =  90
 * May 1 :: 367* 3/12 + 1 + 365 = 457 :: 457 - 427 = 30 (Apr) :: 457 - 337 = 120
 * Jun 1 :: 367* 4/12 + 1 + 365 = 488 :: 488 - 457 = 31 (May) :: 488 - 337 = 151
 * Jul 1 :: 367* 5/12 + 1 + 365 = 518 :: 518 - 488 = 30 (Jun) :: 518 - 337 = 181
 * Aug 1 :: 367* 6/12 + 1 + 365 = 549 :: 549 - 518 = 31 (Jul) :: 549 - 337 = 212
 * Sep 1 :: 367* 7/12 + 1 + 365 = 580 :: 580 - 549 = 31 (Aug) :: 580 - 337 = 243
 * Oct 1 :: 367* 8/12 + 1 + 365 = 610 :: 610 - 580 = 30 (Sep) :: 610 - 337 = 273
 * Nov 1 :: 367* 9/12 + 1 + 365 = 641 :: 641 - 610 = 31 (Oct) :: 641 - 337 = 304
 * Dec 1 :: 367*10/12 + 1 + 365 = 671 :: 671 - 641 = 30 (Nov) :: 671 - 337 = 334
 *
 * The '367' trick in this line of code, plus the current day value, yields the
 * number of complete elapsed days in the year up to the day, as long as the
 * magic value of 337 is subtracted.
 * (The fact that the '367*month/12' value turns out to be a single day short is
 * a critical part of how this formula works; it implicitly does the 'day - 1'
 * correction that would otherwise be required.)
 *
 * Enter the Dragon.
 *
 * (3) epoch -= 719499;
 *
 * This value is '1969 and 337 days':
 *
 * 1969*365 + 1969/4 - 1969/100 + 1969/400 + 337 =
 *   718685 +    492 -       19 +        4 + 337 =
 *   718685 + 477 + 337 =
 *   719499
 *
 * Thus subtracting the magic value of 337. This does double duty because it
 * rectifies the shift that was performed at the beginning.
 *
 * At this point, we have the complete number of days elapsed, from the Unix
 * Epoch of Jan 1, 1970. The rest is a simple conversion to seconds:
 *
 * (4) epoch = epoch*24 + hour;
 * (5) epoch = epoch*60 + minute;
 * (6) epoch = epoch*60 + second;
 *
 */

unsigned long RTC_GetUnixSeconds(void)
{
	unsigned int year = RTCYEAR;
	unsigned int month = RTCMON;
	unsigned int day = RTCDAY;
	unsigned int hour = RTCHOUR;
	unsigned int minute = RTCMIN;
	unsigned int second = RTCSEC;
	unsigned long epoch;

	month -= 2;
	if ((int)month <= 0) {
		month += 12;
		year -= 1;
	}

	// The cast is critical; without it the multiply overflows (int is 2 bytes)
	epoch = (unsigned long)year*365 + year/4 - year/100 + year/400;
	epoch += 367*month/12 + day;
	epoch -= 719499;
	epoch = epoch*24 + hour;
	epoch = epoch*60 + minute;
	epoch = epoch*60 + second;

	return epoch;
}
