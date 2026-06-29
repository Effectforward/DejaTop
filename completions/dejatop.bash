# bash completion for dejatop                              -*- shell-script -*-

_dejatop_completions() {
    local cur prev opts runners
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    opts="--name --icon --runner --prefix --category --env --force --show --rename --update --replace-icon --delete --list --version --help"
    runners="native wine proton"

    case "${prev}" in
        --runner)
            COMPREPLY=( $(compgen -W "${runners}" -- "${cur}") )
            return 0
            ;;
        --icon|--prefix)
            # IFS=$'\n' prevents splitting on spaces in filenames/paths
            local IFS=$'\n'
            COMPREPLY=( $(compgen -f -- "${cur}") )
            compopt -o filenames 2>/dev/null
            return 0
            ;;
        --name|--category|--env)
            return 0
            ;;
        --show|--delete|--update)
            _dejatop_entry_names "${cur}"
            return 0
            ;;
        --rename|--replace-icon)
            # First argument: existing entry name
            _dejatop_entry_names "${cur}"
            return 0
            ;;
    esac

    # Position-aware second argument for two-arg commands
    if [[ "${COMP_CWORD}" -ge 3 ]]; then
        local cmd="${COMP_WORDS[1]}"
        case "${cmd}" in
            --replace-icon)
                # Second arg: icon file path
                local IFS=$'\n'
                COMPREPLY=( $(compgen -f -- "${cur}") )
                compopt -o filenames 2>/dev/null
                return 0
                ;;
            --rename)
                # Second arg: new display name — free text, no completion
                return 0
                ;;
        esac
    fi

    # Flag completion
    if [[ "${cur}" == -* ]]; then
        COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
        return 0
    fi

    # Default: file/directory completion for the executable path argument.
    # IFS=$'\n' is the fix for paths with spaces in directory names.
    local IFS=$'\n'
    COMPREPLY=( $(compgen -f -- "${cur}") )
    compopt -o filenames 2>/dev/null
    return 0
}

# Helper: populate COMPREPLY with existing entry safe-names
_dejatop_entry_names() {
    local cur="$1"
    local apps_dir="${HOME}/.local/share/applications"
    local -a names=()
    if [[ -d "${apps_dir}" ]]; then
        local f b
        for f in "${apps_dir}"/dejatop_*.desktop; do
            [[ -f "${f}" ]] || continue
            b=$(basename "${f}" .desktop)
            b="${b#dejatop_}"
            names+=("${b}")
        done
    fi
    local IFS=$'\n'
    COMPREPLY=( $(compgen -W "${names[*]}" -- "${cur}") )
}

complete -F _dejatop_completions dejatop
