````
Usage:
 sideroot [options] -- [<program> [<argument>...]]

Options:
 -r <root-dir>    Change root directory.
 -d <ch-dir>      Set working directory inside chroot (defaults to /).
 -u <uid>         Change user inside chroot.
 -u <uid>         Change group inside chroot.
 -b <src<:<dst>> Bind <src> to <root-dir>/<dst>.
                       Multiple instances allowed.
 -E               Start with empty environment.
 -e <name>=<val>  Set environment variable <name> to <val>.
                       Multiple instances allowed.
 -h               Display this help.
 --chroot         Shortcut for --proc --sys --dev --devpts.
 --proc           Mount proc filesystem to /proc.
 --sys            Mount sysfs filesystem to /sys.
 --dev            Mount devtmpfs filesystem to /dev.
 --devpts         Mount devpts filesystem to /dev/pts. Implies --dev.
 --run            Mount tmpfs filesystem to /run.
 --tmp            Mount tmpfs filesystem to /tmp.
````

