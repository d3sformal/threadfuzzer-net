#!/bin/bash

# Usage
usage()
{
    echo "Usage: $0 profiler.dll profiler.conf program [args]"
}

if [ "$#" -lt 3 ]; then
    usage
    exit 1
fi

profiler="$1"
config="$2"
program="$3"

shift 3

export COR_ENABLE_PROFILING=1
export COR_PROFILER={00000000-0000-0000-0000-000000000000}
export COR_PROFILER_PATH="$profiler"

export CORECOR_ENABLE_PROFILING=1
export CORECOR_PROFILER={00000000-0000-0000-0000-000000000000}
export CORECOR_PROFILER_PATH="$profiler"

export COR_PROFILER_CONFIG_FILE=$config
export COR_PROFILER_CONSOLE_PROVIDER=../Debug/ConsoleProvider.exe

"$program" "$@"
