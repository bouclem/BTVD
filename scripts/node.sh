#!/bin/bash
echo "================================"
echo "  BitVoid Node"
echo "================================"
echo

if ! command -v bitvoid-core &> /dev/null; then
    if [ -f ./bitvoid-core ]; then
        ./bitvoid-core node "$@"
    else
        echo "Please download bitvoid-core and put it in this folder."
        exit 1
    fi
else
    bitvoid-core node "$@"
fi
