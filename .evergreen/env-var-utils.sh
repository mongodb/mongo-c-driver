#!/usr/bin/env bash

# Usage:
#   to_absolute "./relative/path/to/file"
#   to_absolute "./relative/path/to/dir"
#   to_absolute "/absolute/path/to/file"
#   to_absolute "/absolute/path/to/dir"
#
# Uses dirname, basename, and pwd only. Unix-style paths only!
to_absolute() (
  declare path="${1:?"to_absolute requires a path to convert"}"

  declare dir_part base_part
  dir_part="$(dirname "${path}")" || return
  base_part="$(basename "${path}")" || return

  declare res

  if [[ -d "${path}" ]]; then
    cd "${path}" || return
    res="$(pwd)" || return
  else
    res="$(to_absolute "${dir_part}")" || return
    [[ "${res: -1:1}" == "/" ]] || res+="/" || return
    [[ "${base_part}" == "~" ]] || res+="${base_part}" || return
  fi

  printf "%s" "${res:?}"
)

# Usage:
#   to_windows_path "./some/unix/style/path"
#   to_windows_path "/some/unix/style/path"
to_windows_path() {
  cygpath -aw "${1:?"to_windows_path requires a path to convert"}"
}

# Usage:
#   print_var X
print_var() {
  printf "%s: %s\n" "${1:?"print_var requires a variable to print"}" "${!1:-}"
}

# Usage:
#   check_var_opt X
#   check_var_opt X "default value"
check_var_opt() {
  : "${1:?"check_var_opt requires a variable to check"}"
  printf -v "${1}" "%s" "${!1:-"${2:-}"}"
  print_var "${1}"
}

# Usage:
#   check_var_req X
check_var_req() {
  : "${1:?"check_var_req requires a variable to check"}"
  : "${!1:?"required variable ${1} is unset or null!"}"
  print_var "${1}"
}
