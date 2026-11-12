# Wildcards, missing executables/files, continue after errors
echo GLOB_START
echo tests/fixtures/*.txt
echo tests/fixtures/a*
echo tests/fixtures/*.xyz
echo GLOB_END
no_such_command_214
cat < tests/fixtures/missing_file_214
echo STILL_RUNNING
