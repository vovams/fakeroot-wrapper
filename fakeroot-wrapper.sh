#!/bin/bash

declare -r SCRIPT_NAME="$(basename "$0")"

function main()
{
    local updateEnvFile=1
    local envFileIsNumeric=0
    local targetDir
    local envFilePath
    
    while [ "$#" -gt 0 ]; do
        case "$1" in
            "-n")
                updateEnvFile=0
                ;;
            
            "-i")
                envFileIsNumeric=1
                ;;
            
            "--")
                shift
                break
                ;;
            
            *)
                if [ -z "$targetDir" ]; then
                    targetDir="$1"
                    if [ "$targetDir" == "." ]; then
                        envFilePath=".fakerootenv"
                    else
                        if [ "$targetDir" == ".." ]; then
                            envFilePath="../.fakerootenv"
                        else
                            envFilePath="$targetDir.fakerootenv"
                        fi
                    fi
                else
                    envFilePath="$1"
                fi
                ;;
        esac
        shift
    done
    
    if [ -z "$targetDir" -o -z "$envFilePath" ]; then
        echo "Usage: $SCRIPT_NAME [ -i | -n ] target_dir [ fakeroot_env_filename ] [ -- fakeroot_args ... ]"
        return 0
    fi
    
    local tmpEnvFilePath="$envFilePath.tmp"
    
    if [ "$envFileIsNumeric" == "1" ]; then
        cp "$envFilePath" "$tmpEnvFilePath" || return 1
    else
        filenamesToNumbers "$targetDir" "$envFilePath" "$tmpEnvFilePath" || return 1
    fi
    
    trap "rm -f \"$tmpEnvFilePath\"" EXIT
    
    fakeroot -i "$tmpEnvFilePath" -s "$tmpEnvFilePath" "$@"
    local ret="$?"
    
    if [ "$updateEnvFile" == "1" ]; then
        numbersToFilenames "$targetDir" "$tmpEnvFilePath" "$envFilePath" || return 1
    fi
    
    return "$ret"
}

function filenamesToNumbers()
{
    local baseDir="$1"
    local filenamesFile="$2"
    local numbersFile="$3"
    
    if [ ! -e "$filenamesFile" ]; then
        touch "$numbersFile"
        return 0
    fi
    
    local numbersData=""
    local line
    while read line; do
        local data="$(sed -r 's/^(.+);.+$/\1/' <<< "$line")"
        local filename="$(sed -r 's/^.+;(.+)$/\1/' <<< "$line")"
        
        numbersData+="$(stat "$baseDir/$filename" --format "dev=%D,ino=%i,")"
        numbersData+="$data"
        numbersData+=$'\n'
    done < "$filenamesFile"
    
    echo -n "$numbersData" > $numbersFile
}

function numbersToFilenames()
{
    local baseDir="$1"
    local numbersFile="$2"
    local filenamesFile="$3"
    
    if [ ! -e "$numbersFile" ]; then
        touch "$filenamesFile"
        return 0
    fi
    
    local -A entries
    local line
    while read line; do
        local dev="$(sed -r 's/^.*(^|,)dev=([0-9a-f]+)(,|$).*$/\2/' <<< "$line")"
        local ino="$(sed -r 's/^.*(^|,)ino=([0-9]+)(,|$).*$/\2/' <<< "$line")"
        local data="$(sed -r 's/(^|,)(dev)=([0-9a-f]+)(,|$)/,/g;
                              s/(^|,)(ino)=([0-9]+)(,|$)/,/g;
                              s/,{2,}/,/g;
                              s/(^,)|(,$)//g;' <<< "$line")"
        
        entries["${dev}_${ino}"]="$data"
    done < "$numbersFile"
    
    [ "$?" != "0" ] && return 1
    
    local files
    IFS=$'\n' files=($(find "$baseDir" -exec stat --printf="%D %i " '{}' ';' -printf '%P\n'))
    
    [ "$?" != "0" ] && return 1
    
    local newData=""
    local stat
    for stat in "${files[@]}"; do
        local dev="$(awk '{ print $1 }' <<< "$stat")"
        local ino="$(awk '{ print $2 }' <<< "$stat")"
        local filename="$(sed -r 's/^([0-9]+\s){2}//' <<< "$stat")"
        [ -z "$filename" ] && filename="."
        
        if [ -n "${entries[${dev}_${ino}]}" ]; then
            newData+="${entries[${dev}_${ino}]};$filename"
            newData+=$'\n'
            unset entries["${dev}_${ino}"]
        fi
    done
    
    echo -n "$newData" > "$filenamesFile" || return 1

    local key
    for key in "${!entries[@]}"; do
        echo "$SCRIPT_NAME: not saved file $(tr "_" ":" <<< "$key") => ${entries[$key]}" >&2
    done
}

main "$@"
