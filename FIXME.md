
# TODOs and FIXMEs

## bugs

- fiasco seems to have infinite loops with the ring-buffer-like use of the STACK entry
    - see commit qsort(antonia):042d57b

## measure.py

- data/qse/\*.svg
    - boxplot is squashed to the left (or is it x squashed as y?)
    - is that actually boxplot?

## general structure

- ELFIO in backtracer/lib
- BUILD_PATHS and similar with ? and documentation
- documentation on setup and patches/branches
    - evtl. fl4mer-base tag an ersten fl4mer-commit in den branches

## refactorings

- block.h should be rewritten in nice and clean cpp
    - probably remove the supposed redundancy
    - or at least the reorderability
    - but add more checks
