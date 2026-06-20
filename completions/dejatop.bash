# bash completion for dejatop                              -*- shell-script -*-

_dejatop_completions() {
    local cur prev opts commands runners
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    opts="--name --icon --runner --prefix --replace-icon --delete --list --version --help"
    runners="native wine proton"

    case "${prev}" in
        --runner)
            COMPREPLY=( $(compgen -W "${runners}" -- "${cur}") )
            return 0
            ;;
        --icon|--prefix)
            # File/directory completion
            COMPREPLY=( $(compgen -f -- "${cur}") )
            compopt -o filenames 2>/dev/null
            return 0
            ;;
        --name)
            # Free text, no completion
            return 0
            ;;
        --delete|--replace-icon)
            # Complete from existing dejatop entries
            local entries=""
            local apps_dir="${HOME}/.local/share/applications"
            if [[ -d "${apps_dir}" ]]; then
                for f in "${apps_dir}"/dejatop_*.desktop; do
                    [[ -f "$f" ]] || continue
                    local base=$(basename "$f")
                    base="${base#dejatop_}"
                    base="${base%.desktop}"
                    entries="${entries} ${base}"
                done
            fi
            COMPREPLY=( $(compgen -W "${entries}" -- "${cur}") )
            return 0
            ;;
    esac

    # If current word starts with -, complete options
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
        return 0
    fi

    # Default: file completion (for executable path)
    COMPREPLY=( $(compgen -f -- "${cur}") )
    compopt -o filenames 2>/dev/null
    return 0
}

complete -F _dejatop_completions dejatop
