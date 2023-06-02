#!/bin/bash

esc() {
  printf "\033[$*m"
}

echo "normal text"
echo $(esc 1)bold text$(esc 0)

awk -v term_cols="${width:-$(tput cols || echo 80)}" 'BEGIN{
    s="/\\";
    for (colnum = 0; colnum<term_cols; colnum++) {
        r = 255-(colnum*255/term_cols);
        g = (colnum*510/term_cols);
        b = (colnum*255/term_cols);
        if (g>255) g = 510-g;
        printf "\033[48;2;%d;%d;%dm", r,g,b;
        printf "\033[38;2;%d;%d;%dm", 255-r,255-g,255-b;
        printf "%s\033[0m", substr(s,colnum%2+1,1);
    }
    printf "\n";
}'

#echo "base"
#for x in {0..8}; do for i in {30..37}; do for a in {40..47}; do echo -ne "\e[$x;$i;$a""m\\\e[$x;$i;$a""m\e[0;37;40m "; done; echo; done; done; echo ""
#
#esc() {
#  printf "\033[$1m"
#}
#
#color() {
#  echo $(esc "0;$(($2+$1))") color $1
#}
#
#echo $(color 0 30) black $(esc 0)
#echo $(color 1 30) red $(esc 0)
#echo $(color 2 30) green $(esc 0)
#echo $(color 3 30) yellow $(esc 0)
#echo $(color 4 30) blue $(esc 0)
#echo $(color 5 30) pink $(esc 0)
#echo $(color 6 30) sky $(esc 0)
#echo $(color 7 30) white $(esc 0)
#
#echo $(color 0 90) bright grey $(esc 0)
#echo $(color 1 90) bright red $(esc 0)
#echo $(color 2 90) bright green $(esc 0)
#echo $(color 3 90) bright yellow $(esc 0)
#echo $(color 4 90) bright blue $(esc 0)
#echo $(color 5 90) bright pink $(esc 0)
#echo $(color 6 90) bright sky $(esc 0)
#echo $(color 7 90) bright white $(esc 0)

printf "glyph: %s code point: %x\n" ∈ \'∈
printf "glyph: %s code point: %x\n" ⚠ \'⚠
printf "glyph: %s code point: %x\n" ⌘ \'⌘
printf "glyph: %s code point: %x\n" ┐ \'┐
