:: Adapted from https://stackoverflow.com/a/33825986/1352761

@echo off

:: Save the start timestamp
set _stime=%time: =0%


:: yourCommandHere
%*


:: Save the end timestamp
set _etime=%time: =0%

:: Calculate the difference in cSeconds
set /a _hours=100%_stime:~0,2%%%100,_min=100%_stime:~3,2%%%100,_sec=100%_stime:~6,2%%%100,_cs=%_stime:~9,2%
set /a _started=_hours*60*60*100+_min*60*100+_sec*100+_cs

set /a _hours=100%_etime:~0,2%%%100,_min=100%_etime:~3,2%%%100,_sec=100%_etime:~6,2%%%100,_cs=%_etime:~9,2%
set /a _duration=_hours*60*60*100+_min*60*100+_sec*100+_cs-_started

:: Populate variables for rendering (100+ needed for padding)
set /a _hours=_duration/60/60/100,_min=100+_duration/60/100%%60,_sec=100+(_duration/100%%60%%60),_cs=100+_duration%%100

echo(
echo Start: %_stime%
echo   End: %_etime%
echo  Time: %_hours%:%_min:~-2%:%_sec:~-2%.%_cs:~-2%
