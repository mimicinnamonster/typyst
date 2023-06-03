#!/bin/bash
typyst -t 0.75 -a $(find ~/Videos/bgs/* | shuf | sed 1q) "$@"
