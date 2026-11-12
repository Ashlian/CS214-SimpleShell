# Two- and three-stage pipelines
echo PIPE_START
echo hello | wc -c
echo hello | cat | wc -c
ls | echo
cat | wc -c
echo PIPE_END
