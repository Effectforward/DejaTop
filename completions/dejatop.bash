# bash completion for dejatop                              -*- shell-script -*-

_dejatop_completions() {
    local cur prev opts commands runners
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    opts="--name --icon --runner --prefix --category --env --force --replace-icon --delete --show --list --version --help"
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
        --name|--category|--env)
            # Free text, no completion
            return 0
            ;;
        --delete|--replace-icon|--show)
            # Complete from existing dejatop entries (by Name= field when readable,
            # falling back to sanitized filename)
            local entries=""
            local apps_dir="${HOME}/.local/share/applications"
            if [[ -d "${apps_dir}" ]]; then
                for f in "${apps_dir}"/dejatop_*.desktop; do
                    [[ -f "$f" ]] || continue
                    # Try to read Name= from the file for a human-readable completion
                    local display_name
                    display_name=$(grep -m1 "^Name=" "$f" 2>/dev/null | cut -d= -f2-)
                    if [[ -n "${display_name}" ]]; then
                        entries="${entries} ${display_name}"
                    else
                        local base=$(basename "$f")
                        base="${base#dejatop_}"
                        base="${base%.desktop}"
                        entries="${entries} ${base}"
                    fi
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
