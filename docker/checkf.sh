fn_exists() { declare -F "$1" > /dev/null; }
unset f
fn_exists f && echo yes || echo no
f() { return; }
fn_exists f && echo yes || echo no
