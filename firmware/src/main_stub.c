/* Guard: exactly one app/stageN.c must define main() for the selected STAGE. */
#if STAGE < 0 || STAGE > 5
#error "STAGE must be 0..5"
#endif
