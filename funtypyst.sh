#!/bin/bash
/home/michal/Projects/typyst/_build/typyst -t 0.80 -a $(find ~/Videos/bgs/* | shuf | sed 1q) "$@"
