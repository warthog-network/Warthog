#!/bin/sh
cat $@ | sed 's/#include/\/\/#include/g' | xxd -i
# cat $@ | sed 's/#include/\/\/#include/g'
