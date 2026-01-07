#/usr/bin/env bash

get_mode()
{
    local carg
    for carg in ${COMP_WORDS[@]}; do
	case $carg in
	    '--clone')
		echo 'clone'
		return
		;;
	    '--restore')
		echo 'restore'
		return
		;;
	    '--dev-to-dev')
		echo 'dev-to-dev'
		return
		;;
	    '--domain')
		echo 'domain'
		return
		;;
	esac
    done
    echo 'na'

}

get_source()
{
    local carg
    for carg in ${COMP_WORDS[@]}; do
	case $carg in
	    '--source')
		echo 'source'
		return
		;;
	esac
    done
    echo 'na'

}

get_output()
{
    local carg
    for carg in ${COMP_WORDS[@]}; do
	case $carg in
	    '--output')
		echo 'output'
		return
		;;
	    '--overwrite')
		echo 'overwrite'
		return
		;;
	esac
    done
    echo 'na'

}


_partclone_completions()
{
    local cur prev
    local pn mode sourceopt outputopt dev_list availopts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    pn=$1

    mode=$(get_mode)
    if [[ "$pn" == "partclone.dd" ]]; then
        mode='dd'
    elif [[ "$pn" == "partclone.restore" ]]; then
	mode='restore'
    fi

    if [[ "$mode" == "na" ]]; then
	COMPREPLY=( $(compgen -W "--clone --restore --dev-to-dev" -- $cur) )
	return
    fi

    sourceopt=$(get_source)
    if [[ "$sourceopt" == "na" ]]; then
	COMPREPLY=( $(compgen -W "--source" -- $cur) )
	return
    else
        case $prev in
    	    '--source')
    		if [[ "$mode" == "clone" || "$mode" == "dev-to-dev" ]]; then
    		    dev_list=$(lsblk -pnro name)
    		    COMPREPLY=( $(compgen -W "$dev_list" -- $cur) )
    		    return
	        elif [[ "$mode" == "restore"  ]]; then
		    compopt -o bashdefault -o default -o filenames
		    COMPREPLY=( $(compgen -f -- $cur) )
		    return
		else
                    dev_list=$(lsblk -pnro name)
		    compopt -o bashdefault -o default -o filenames
		    COMPREPLY=( $(compgen -f -- $cur) $(compgen -W "$dev_list" -- $cur))
		    return
    		fi
    		;;
        esac
    fi

    outputopt=$(get_output)
    if [[ "$outputopt" == "na" ]]; then
    	if [[ "$mode" == "restore"  || "$mode" == "dev-to-dev"  ]]; then
	    COMPREPLY=( $(compgen -W "--output" -- $cur) )
	else
	    COMPREPLY=( $(compgen -W "--output --overwrite" -- $cur) )
	fi
	return
    else
        case $prev in
    	    '--output'|'--overwrite')
    		if [[ "$mode" == "restore" || "$mode" == "dev-to-dev" ]]; then
    		    dev_list=$(lsblk -pnro name)
    		    COMPREPLY=( $(compgen -W "$dev_list" -- $cur) )
    		    return
	        elif [[ "$mode" == "clone" ]]; then
		    compopt -o bashdefault -o default -o filenames
		    COMPREPLY=( $(compgen -f -- $cur) )
		    return
		else
		    dev_list=$(lsblk -pnro name)
		    compopt -o bashdefault -o default -o filenames
		    COMPREPLY=( $(compgen -f -- $cur) $(compgen -W "$dev_list" -- $cur) )
		    return
    		fi
    		;;
        esac
    fi

    # other options
    compopt -o bashdefault -o default
    case $prev in
	'--checksum-mode')
	    cur=${cur#*=}
	    COMPREPLY=($(compgen -W "0 1" -- "$cur"))
	    return
	    ;;
	'--debug')
	    cur=${cur#*=}
	    COMPREPLY=($(compgen -W "1 2 3" -- "$cur"))
	    return
	    ;;
	'--logfile')
	    compopt -o bashdefault -o default -o filenames
	    COMPREPLY=( $(compgen -f -- $cur) )
	    return
	    ;;
        *)
	    if [[ "$mode" == "dd" ]]; then
	        availopts="--restore_raw_file --logfile --domain --offset_domain= --rescue --checksum-mode= --blocks-per-checksum= --no-reseed --skip_write_error --debug= --no_check --ncurses --ignore_fschk --ignore_crc --force --UI-fresh --no_block_detail --buffer_size --quiet --offset= --btfiles --btfiles_torrent --note --read-direct-io --write-direct-io --help --version"
	    else
		availopts="--restore_raw_file --logfile --compresscmd --domain --offset_domain= --rescue --checksum-mode= --blocks-per-checksum= --no-reseed --skip_write_error --debug= --no_check --ncurses --ignore_fschk --ignore_crc --force --UI-fresh --no_block_detail --buffer_size --quiet --offset= --btfiles --btfiles_torrent --note --read-direct-io --write-direct-io --help --version"
	    fi
	    COMPREPLY=( $(compgen -W "$availopts" -- $cur) )
            [[ ${COMPREPLY-} == *= ]] && compopt -o nospace
	    ;;
    esac
}

complete -F _partclone_completions partclone.apfs partclone.ext2 partclone.f2fs  \
	partclone.hfs+ partclone.minix partclone.reiser4 partclone.btrfs partclone.ext3  \
	partclone.fat partclone.hfsp  partclone.nilfs2 partclone.restore partclone.ext4  \
	partclone.fat12 partclone.hfsplus partclone.ntfs partclone.vfat partclone.ext4dev \
	partclone.fat16 partclone.xfs partclone.exfat partclone.extfs partclone.fat32 \
	partclone.imager partclone.dd

_partclone_chkimg_completions()
{
    local cur prev
    local sourceopt availopts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    sourceopt=$(get_source)
    if [[ "$sourceopt" == "na" ]]; then
	COMPREPLY=( $(compgen -W "--source" -- $cur) )
	return
    else
        case $prev in
    	    '--source')
		compopt -o bashdefault -o default -o filenames
		COMPREPLY=( $(compgen -f -- $cur) )
		return
    		;;
        esac
    fi

    # other options
    compopt -o bashdefault -o default
    case $prev in
	'--debug')
	    cur=${cur#*=}
	    COMPREPLY=($(compgen -W "1 2 3" -- "$cur"))
	    return
	    ;;
	'--logfile')
	    compopt -o bashdefault -o default -o filenames
	    COMPREPLY=( $(compgen -f -- $cur) )
	    return
	    ;;
        *)
	    availopts="--logfile --debug= --no_check --ncurses --ignore_crc --force --UI-fresh --no_block_detail --buffer_size --note --help --version"
	    COMPREPLY=( $(compgen -W "$availopts" -- $cur) )
            [[ ${COMPREPLY-} == *= ]] && compopt -o nospace
	    ;;
    esac
}
complete -F _partclone_chkimg_completions partclone.chkimg



_partclone_info_completions()
{
    local cur prev
    local sourceopt availopts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    sourceopt=$(get_source)
    if [[ "$sourceopt" == "na" ]]; then
	COMPREPLY=( $(compgen -W "--source" -- $cur) )
	return
    else
        case $prev in
    	    '--source')
		compopt -o bashdefault -o default -o filenames
		COMPREPLY=( $(compgen -f -- $cur) )
		return
    		;;
        esac
    fi

    # other options
    compopt -o bashdefault -o default
    case $prev in
	'--debug')
	    cur=${cur#*=}
	    COMPREPLY=($(compgen -W "1 2 3" -- "$cur"))
	    return
	    ;;
	'--logfile')
	    compopt -o bashdefault -o default -o filenames
	    COMPREPLY=( $(compgen -f -- $cur) )
	    return
	    ;;
        *)
	    availopts="--logfile --debug= --quiet --help --version"
	    COMPREPLY=( $(compgen -W "$availopts" -- $cur) )
            [[ ${COMPREPLY-} == *= ]] && compopt -o nospace
	    ;;
    esac
}

