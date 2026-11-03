# Runs the bal-cli executable once, as a shell would, and checks what it did.
#   cmake -DCLI=<bal-cli> -DARGS=<a|b|c> -DCODE=<exit code>
#         [-DOUT=<regex>] [-DERR=<regex>] [-DNO_OUT=1] [-DNO_ERR=1] -P cli_run.cmake
# OUT / ERR must match stdout / stderr; NO_OUT / NO_ERR want them empty.

string(REPLACE "|" ";" ARGS "${ARGS}")
execute_process(
    COMMAND "${CLI}" ${ARGS}
    RESULT_VARIABLE code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
# Windows writes text with CRLF; the patterns use \n.
string(REPLACE "\r\n" "\n" out "${out}")
string(REPLACE "\r\n" "\n" err "${err}")

set(failed "")
if(NOT code STREQUAL CODE)
    string(APPEND failed "exit code ${code}, expected ${CODE}\n")
endif()
if(DEFINED OUT AND NOT out MATCHES "${OUT}")
    string(APPEND failed "stdout does not match: ${OUT}\n")
endif()
if(DEFINED ERR AND NOT err MATCHES "${ERR}")
    string(APPEND failed "stderr does not match: ${ERR}\n")
endif()
if(NO_OUT AND NOT out STREQUAL "")
    string(APPEND failed "stdout should be empty\n")
endif()
if(NO_ERR AND NOT err STREQUAL "")
    string(APPEND failed "stderr should be empty\n")
endif()
if(failed)
    message(FATAL_ERROR "bal-cli ${ARGS}\n${failed}--- stdout:\n${out}--- stderr:\n${err}")
endif()
