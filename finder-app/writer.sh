#!/bin/sh

#echo "First argument: $1"
#echo "Second argument: $2"
#echo "All arguments: $@"
#echo "Number of arguments: $#"

if [ $# -lt 2 ]
then
    echo "Please provide two arguments!"
    echo "Example usage: writer.sh writefile writestr"
    exit 1
else
    DIR="$(dirname "$1")"
    STR="$2"
fi

if [ -d "$DIR" ]
then
    #echo "Directory exists: $DIR"
    touch "$1"
    echo "$STR" > "$1"

else
    #echo "Directory does NOT exist: $DIR"
    mkdir -p "$DIR" && touch "$1"
    echo "$STR" > "$1"
fi