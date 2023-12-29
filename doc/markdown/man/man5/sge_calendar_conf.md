---
title: sge_calendar_conf
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_calendar_conf - xxQS_NAMExx calendar configuration file format

# DESCRIPTION

*calendar_conf* reflects the format of the xxQS_NAMExx calendar
configuration. The definition of calendars is used to specify "on duty"
and "off duty" time periods for xxQS_NAMExx queues on a time of day, day
of week or day of year basis. Various calendars can be implemented and
the appropriate calendar definition for a certain class of jobs can be
attached to a queue.

*calendar_conf* entries can be added, modified and displayed with the
*-Acal*, *-acal*, *-Mcal*, *-mcal*, *-scal* and *-scall* options to
*qconf* (1) or with the calendar configuration dialog of the graphical
user interface *qmon* (1).

Note, xxQS_NAMExx allows backslashes (\\) be used to escape newline
(\\newline) characters. The backslash and the newline are replaced with
a space (" ") character before any interpretation.

# FORMAT

## **calendar_name**

The name of the calendar to be used when attaching it to queues or when
administering the calendar definition. See *calendar_name* in
*sge_types* (1) for a precise definition of valid calendar names.

## **year**

The queue status definition on a day of the year basis. This field
generally will specify on which days of a year (and optionally at which
times on those days) a queue, to which the calendar is attached, will
change to a certain state. The syntax of the **year** field is defined
as follows:

    year:=
    	{ NONE
        | year_day_range_list=daytime_range_list[=state]
        | year_day_range_list=[daytime_range_list=]state
        | state}

Where

-   NONE means, no definition is made on the year basis

-   if a definition is made on the year basis, at least one of
    **year_day_range_list**, **daytime_range_list** and **state** always
    have to be present,

-   all day long is assumed if **daytime_range_list** is omitted,

-   switching the queue to "off" (i.e. disabling it) is assumed if
    **state** is omitted,

-   the queue is assumed to be enabled for days neither referenced
    implicitly (by omitting the **year_day_range_list**) nor explicitly

and the syntactical components are defined as follows:

    	year_day_range_list := 	{yearday-yearday|yearday},...
    	daytime_range_list := 	hour[:minute][:second]-
    	 	hour[:minute][:second],...
    	state := 	{on|off|suspended}
    	year_day := 	month_day.month.year
    	month_day := 	{1|2|...|31}
    	month := 	{jan|feb|...|dec|1|2|...|12}
    	year := 	{1970|1971|...|2037}

## **week**

The queue status definition on a day of the week basis. This field
generally will specify on which days of a week (and optionally at which
times on those days) a queue, to which the calendar is attached, will
change to a certain state. The syntax of the **week** field is defined
as follows:

    week:=
    	{ NONE 
        | week_day_range_list[=daytime_range_list][=state]
        | [week_day_range_list=]daytime_range_list[=state]
        | [week_day_range_list=][daytime_range_list=]state} ...

Where

-   NONE means, no definition is made on the week basis

-   if a definition is made on the week basis, at least one of
    **week_day_range_list**, **daytime_range_list** and **state** always
    have to be present,

-   every day in the week is assumed if **week_day_range_list** is
    omitted,

-   syntax and semantics of **daytime_range_list** and **state** are
    identical to the definition given for the year field above,

-   the queue is assumed to be enabled for days neither referenced
    implicitly (by omitting the **week_day_range_list**) nor explicitly

and where **week_day_range_list** is defined as

    	week_day_range_list := 	{weekday-weekday|weekday},...
    	week_day := 	{mon|tue|wed|thu|fri|sat|sun}

with week_day ranges the week_day identifiers must be different.

# SEMANTICS

Successive entries to the **year** and **week** fields (separated by
blanks) are combined in compliance with the following rule:

-   "off"-areas are overridden by overlapping "on"- and
    "suspended"-areas and "suspended"-areas are overridden by
    "on"-areas.

Hence an entry of the form

    	week 	12-18 tue=13-17=on

means that queues referencing the corresponding calendar are disabled
the entire week from 12.00-18.00 with the exception of Tuesday between
13.00-17.00 where the queues are available.

-   Area overriding occurs only within a year/week basis. If a year
    entry exists for a day then only the year calendar is taken into
    account and no area overriding is done with a possibly conflicting
    week area.

-   the second time specification in a daytime_range_list may be before
    the first one and treated as expected. Thus an entry of the form

<!-- -->

    	year 	12.03.2004=12-11=off 

causes the queue(s) be disabled 12.03.2004 from 00:00:00 - 10:59:59 and
12:00:00 - 23:59:59.

# EXAMPLES

(The following examples are contained in the directory
$xxQS_NAME_Sxx_ROOT/util/resources/calendars).

-   Night, weekend and public holiday calendar:

On public holidays "night" queues are explicitly enabled. On working
days queues are disabled between 6.00 and 20.00. Saturday and Sunday are
implicitly handled as enabled times:

    	calendar_name 	night
    	year 	1.1.1999,6.1.1999,28.3.1999,30.3.1999-
    	31.3.1999,18.5.1999-19.5.1999,3.10.1999,25.12.1999,26
    	.12.1999=on
    	week 	mon-fri=6-20

-   Day calendar:

On public holidays "day"-queues are disabled. On working days such
queues are closed during the night between 20.00 and 6.00, i.e. the
queues are also closed on Monday from 0.00 to 6.00 and on Friday from
20.00 to 24.00. On Saturday and Sunday the queues are disabled.

    	calendar_name 	day
    	year 	1.1.1999,6.1.1999,28.3.1999,30.3.1999-
    	31.3.1999,18.5.1999-19.5.1999,3.10.1999,25.12.1999,26
    	.12.1999
    	week 	mon-fri=20-6 sat-sun

-   Night, weekend and public holiday calendar with suspension:

Essentially the same scenario as the first example but queues are
suspended instead of switching them "off".

    	calendar_name 	night_s
    	year 	1.1.1999,6.1.1999,28.3.1999,30.3.1999-
    	31.3.1999,18.5.1999-19.5.1999,3.10.1999,25.12.1999,26
    	.12.1999=on
    	week 	mon-fri=6-20=suspended

-   Day calendar with suspension:

Essentially the same scenario as the second example but queues are
suspended instead of switching them "off".

    	calendar_name 	day_s
    	year 	1.1.1999,6.1.1999,28.3.1999,30.3.1999-
    	31.3.1999,18.5.1999-19.5.1999,3.10.1999,25.12.1999,26
    	.12.1999=suspended
    	week 	mon-fri=20-6=suspended sat-sun=suspended

-   Weekend calendar with suspension, ignoring public holidays:

Settings are only done on the week basis, no settings on the year basis
(keyword "NONE").

    	calendar_name 	weekend_s
    	year 	NONE
    	week 	sat-sun=suspended

# SEE ALSO

*xxqs_name_sxx_intro* (1), *xxqs_name_sxx\_\_types* (1), *qconf* (1),
*xxqs_name_sxx_queue_conf* (5).

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
