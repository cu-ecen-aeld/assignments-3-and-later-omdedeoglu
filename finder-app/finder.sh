#!/bin/sh

#echo "First argument: $1"
#echo "Second argument: $2"
#echo "All arguments: $@"
#echo "Number of arguments: $#"

if [ $# -lt 2 ]
then
    echo "Please provide two arguments!"
    echo "Example usage: finder.sh filesdir searchstr"
    exit 1
else
    DIR="$1"
    STR="$2"
fi

if [ -d "$DIR" ]
then
    #echo "Directory exists: $DIR"
    X=$(find "$DIR" -type f | wc -l)
    Y=$(grep -rI "$STR" "$DIR" | wc -l)
    echo "The number of files are $X and the number of matching lines are $Y"
else
    echo "Directory does NOT exist: $DIR"
    exit 1
fi






