#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <fmtmsg.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ENV_LIMIT 100
#define ENV_VAR_MAX 1000
#define BIND_LIMIT 100
#define DIR_SEP ":"
#define FMT_OPT(spec, desc) " " spec "\n " desc "\n\n"

void die(const char *msg) {
  if (errno)
    perror(msg);
  else
    fprintf(stderr, "%s\n", msg);
  exit(1);
}

typedef struct {
  char src[PATH_MAX];
  char dst[PATH_MAX];
} bind_mount;

void help() {
  printf("Usage:\n sideroot [options] -- [<program> [<argument>...]]\n\n");
  printf(
      "Options:\n" 
   		FMT_OPT("-r <root-dir>", "Set root directory.")
		FMT_OPT("-d <ch-dir>", "Set working directory for chroot.") 

		FMT_OPT("-D",
		"Bind current directory to working directory in chroot")

		FMT_OPT("-u <uid>", "Change user inside chroot") 

		FMT_OPT("-g <gid>", "Change group inside chroot")

		FMT_OPT("-R", "Change user and group to 0") 

		FMT_OPT("-b <src<" DIR_SEP "<dst>>",
		"Bind <src> to <root-dir>/<dst>.")

		FMT_OPT("-p", "Execute command as init") 
		FMT_OPT("-U", "Change uid and gid to 1000")

		FMT_OPT("-R", "Change uid and gid to 0") 
		FMT_OPT("-E", "Do not clean environment.")

		FMT_OPT("-e <name>=<val>",
		"Set environment variable <name> to <val>.")

		FMT_OPT("-h", "Display this help.") 
		FMT_OPT("--noproc", "Do not mount proc.")
	);
}

int main(int argc, char *argv[]) {
  char wd_dir[PATH_MAX];
  getcwd(wd_dir, sizeof(wd_dir));

  char *root_dir = NULL;
  char *ch_dir = "/";
  int uid = getuid();
  int gid = getgid();
  int proc = 1;
  int sys = 1;
  int dev = 1;
  int devpts = 1;
  int run = 1;
  int tmp = 1;
  int clear_env = 1;
  bind_mount binds[BIND_LIMIT];
  int bind_count = 0;
  int bind_wd = 0;
  char env[ENV_LIMIT][ENV_VAR_MAX];
  int env_count = 0;

  if (argc == 1) {
    help();
    return 1;
  }

  opterr = 0;
  int arg;
  while ((arg = getopt(argc, argv, "hpURr:d:Du:g::b:e:E")) != -1) {
    switch (arg) {
    case 'h':
      help();
      return 0;
    case 'R':
      uid = gid = 0;
      break;
    case 'U':
      uid = gid = 1000;
      break;
    case 'r':
      root_dir = optarg;
      break;
    case 'D':
      bind_wd = 1;
      if (!strcmp(ch_dir, "/")) ch_dir = "/work";
      break;
    case 'd':
      ch_dir = optarg;
      break;
    case 'u':
      uid = atoi(optarg);
      break;
    case 'g':
      gid = atoi(optarg);
      break;
    case 'b':
      char *src = strtok(optarg, DIR_SEP);
      char *dst = strtok(NULL, DIR_SEP);
      strcpy(binds[bind_count].src, src);
      strcpy(binds[bind_count].dst, dst ? dst : src);
      bind_count++;
      break;

    case 'E':
      clear_env = 0;
      break;

    case 'e':
      if (strlen(optarg) < ENV_VAR_MAX) {
        strcpy(env[env_count], optarg);
        env_count++;
      }
      break;
    case '?':
    case ':':
      fprintf(stderr, "Invalid option -%c\n", optopt);
      exit(1);
    }
  }

  int wd_index = -1;
  if (bind_wd) {
    strcpy(binds[bind_count].src, wd_dir);
    strcpy(binds[bind_count].dst, ch_dir);
    wd_index = bind_count;
    bind_count++;
  }

  strcpy(binds[bind_count].src, "/var/run/chrome");
  strcpy(binds[bind_count].dst, "/var/chrome");
  bind_count++;

  if (!root_dir)
    root_dir = wd_dir;

  if (unshare(CLONE_NEWNS | CLONE_NEWPID | CLONE_FS | CLONE_THREAD))
    die("unshare");
  struct stat sb;

  for (int index = 0; index < bind_count; index++) {
    char *target;
    asprintf(&target, "%s/%s", root_dir,
             binds[index].dst[0] == '/' ? binds[index].dst + 1
                                        : binds[index].dst);

    if ((index == wd_index ||
         strcmp(binds[index].src, "/var/run/chrome") == 0) &&
        stat(target, &sb) && mkdir(target, S_IRWXU | S_IRWXG))
      die(target);

    if (mount(binds[index].src, target, "bind", MS_BIND | MS_PRIVATE, NULL))
      die(target);
    free(target);
  }

  int pid = fork();
  if (pid == -1)
    die("fork");
  else if (pid) {
    int status;
    waitpid(-1, &status, 0);
    return status;
  }

mount("none", root_dir, NULL, MS_PRIVATE | MS_REC, NULL);

  if (chroot(root_dir))
    die(root_dir);

  if (proc && stat("/proc", &sb) == 0 &&
      mount("proc", "/proc", "proc", 0, NULL))
    die("/proc");

  if (dev && stat("/dev", &sb) == 0 &&
      mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=755"))
    die("/dev");

  if (devpts && stat("/dev/pts", &sb) == 0 &&
      mount("devpts", "/dev/pts", "devpts", MS_NOEXEC,
            "gid=5,mode=620,ptmxmode=666"))
    die("/dev/pts");

  if (sys && stat("/sys", &sb) == 0 && mount("none", "/sys", "sysfs", 0, NULL))
    die("/sys");

  if (tmp && stat("/tmp", &sb) == 0) {
    if (mount("tmpfs", "/tmp", "tmpfs", 0, NULL))
      die("/tmp");
    if (run && stat("/run", &sb) == 0 &&
        mkdir("/tmp/run", S_IRWXU | S_IRWXG | S_IRWXO) == 0) {
      if (mount("/tmp/run", "/run", "bind", MS_BIND | MS_PRIVATE, NULL))
        die("/run");
      mkdir("/run/user", S_IRWXU | S_IRWXG);
    }
  }

  char *user_runtime_dir = NULL;
  if (run && stat("/run/user", &sb) == 0) {
    asprintf(&user_runtime_dir, "/run/user/%d", uid);
    mkdir(user_runtime_dir, S_IRWXU | S_IRWXG);
    chown(user_runtime_dir, uid, gid);
  }

  if (setgid(gid) || setuid(uid))
    die("setuid");

  struct passwd *pwd;
  pwd = getpwuid(uid);

  const char *term = getenv("TERM");
  if (clear_env)
    clearenv();

  for (int index = 0; index < env_count; index++)
    putenv(env[index]);

  if (pwd) {
    setenv("USER", pwd->pw_name, 0);
    if (stat(pwd->pw_dir, &sb))
      mkdir(pwd->pw_dir, S_IRWXU);
    setenv("HOME", pwd->pw_dir, 0);
    setenv("SHELL", pwd->pw_shell, 0);
  }

  setenv("TERM", term, 0);
  setenv("PATH", uid ? "/bin:/usr/bin" : "/bin:/usr/bin:/sbin:/usr/sbin", 0);
  setenv("SHELL", "/bin/sh", 0);

  if (user_runtime_dir) {
    setenv("XDG_RUNTIME_DIR", user_runtime_dir, 0);
    if (stat("/var/chrome/wayland-0", &sb) == 0) {
      char *file;
      asprintf(&file, "%s/wayland-0", user_runtime_dir);
      symlink("/var/chrome/wayland-0", file);
    }
  }

  chdir(ch_dir);

  if (argc > optind) {
    execvp(argv[optind], &argv[optind]);
    die(argv[optind]);
  } else {
    execlp("sh", "sh", NULL);
    die("sh");
  }
}
