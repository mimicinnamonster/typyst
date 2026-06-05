export PS1=""

esc() {
  printf "\033[$*m"
}

#echo '🙇'
#echo '⚠'
#echo 'ヾ'
#echo '( •_•)>⌐■-■     ヾ(⌐■_■)ノ♪'
#echo '(｡•́︿•̀｡) Pwease fixy-fix ASAPy-wapsy! (๑•́₋•̩̥̀๑)'
#echo 'ヽ(&ﾟ▽ﾟ)ノ'
#echo "a"

#echo $(esc 1)bold text$(esc 0)
#echo not bold text
#echo '/\/\/\/\/\/\/\/\/\'

echo $(esc 1)hello$(esc 0)
echo hello
