# pwait

## Description
`pwait` waits for process supplied on the command line to finish. It's
different from `wait` because it can wait for *any* process to finish not just
children of the current shell. When all the processes supplied can no longer be
found `pwait` exits with `0`.
The above replaces idioms such as `tail --pid=<PID> --follow /dev/null && do_something`
which now becomes `pwait <PID> && do_something`.

## Usage
The program can be invoked as follows `pwait [options..] <PID..> <NAME..>`
Process can be given by PID or by name, `pwait 1735 gcc`. Name resolution is
not *exact*, meaning all processes that `<NAME>` is a substring of will be waited.

### Options
Several options can be given that control the behavior of `pwait`.
`-t, --timeout <NUMBER>` tells `pwait` to exit with `1` after `<NUMBER>` of
seconds have passed and not all the processes have finished.
`pwait --timeout 30 7204` waits for the process with PID `7204` for a
maximum of `30` seconds, if the process hasn't finished by then it exits
with `1`. `<NUMBER>` can also be a decimal number, such as `0.4`. By default,
`pwait` waits indefinitely for the processes to finish.

`-n, --interval <NUMBER>` tells `pwait` to sleep for `<NUMBER>` of seconds
after each check for the existence of the processes.
`pwait --interval 3 7204` periodically sleeps for `3` seconds after checking
for the existence of the process with PID `7204`. Default is `1`.

## Installation
The program is written to conform to the `C++ 98` standard.
It can be compiled the following way:

```
git clone https://github.com/begone-prop/pwait.git
cd pwait
g++ -Wall -Wextra -O2 -std=c++98 -pedantic ./pwait.cpp -o ./pwait
```

**Note:** There is no guarantee that the process you are waiting for is the one
you actually want because it's only defined to `wait` for processes that are
your children. It's possible that in the time interval between checks for the
existence of the process the original one terminated and a new, different
process was created and was assigned the same PID. Using smaller values for the
`--interval` options make the odds of this happening smaller.
