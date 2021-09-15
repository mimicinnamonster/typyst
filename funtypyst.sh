#!/bin/bash
typyst -a $(find ~/Videos/bgs/* | shuf | sed 1q) "$@"
