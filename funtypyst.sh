#!/bin/bash
typyst -t 0.80 -a $(find ~/Videos/bgs/* | shuf | sed 1q) "$@"
