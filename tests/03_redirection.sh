# Create, append, truncate, input and built-in redirection
echo REDIR_START
echo first > tests/fixtures/output.txt
echo second >> tests/fixtures/output.txt
cat < tests/fixtures/output.txt
echo replaced > tests/fixtures/output.txt
cat < tests/fixtures/output.txt
pwd > tests/fixtures/cwd.txt
cat < tests/fixtures/cwd.txt
echo REDIR_END
