# Elevator Control System

This is a simple elevator control system programed in the c
programming language.

## Modules TODO:
- [x] call pad
- [x] internal controller
- [ ] safety system
- [ ] elevator car
- [ ] controller

## Keep in mind
1. decide on using pthread_cond_signal or
   pthread_cond_broadcast.

## Questions
1. Should I be using consecutive posix helper functionsfor
   setting and checking variables? I'm worried it way allow
   opportunity for other processes to alter state at bad times.
2. I'm using a paturn in the car component that consists of:
    - creating a new thread.
    - Infinite loop:
        - wait on the condition
        - check if the correct conditions are met for the
          current task
        - do some work
        - pthread cancel breakpoint
   Is this a bad method?

