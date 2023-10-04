#define _GNU_SOURCE

#include <err.h>
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
#include <unistd.h>
#include <getopt.h>

#include <errno.h>
#define _COLOR_START_RED  "\x1B[31m"

#define _COLOR_STOP  "\x1B[0m"

#define DIE(expr) { \
	int error = errno; \
	printf(\
		"%s%s:%s:%i: error in %s: %s%s\n",  \
		_COLOR_START_RED, prog, __FILE__ ,__LINE__ , \
		#expr, strerror(error), _COLOR_STOP \
	); \
	exit(error); \
}

#define TRY_SYSCALL(expr) if (expr < 0) DIE(expr)

#define bind(src, dst) mount(src, dst, "bind", MS_BIND | MS_PRIVATE, NULL)

static char *prog;
const char *sep = ":";

void help() {
	printf("\nUsage:\n %s [options] -- [<program> [<argument>...]]\n\n", prog);	
	printf("Options:\n");
	printf(" -r <root-dir>    Change root directory.\n");
	printf(" -d <ch-dir>      Set working directory inside chroot (defaults to /).\n");
	printf(" -u <uid>         Change user inside chroot.\n");
	printf(" -u <uid>         Change group inside chroot.\n");
	printf(" -b <src<%s<dst>> Bind <src> to <root-dir>/<dst>.\n", sep);
	printf("                       Multiple instances allowed.\n");
	printf(" -E               Start with empty environment.\n");
	printf(" -e <name>=<val>  Set environment variable <name> to <val>.\n");
	printf("                       Multiple instances allowed.\n");	
	printf(" -h               Display this help.\n");
	printf(" --chroot         Shortcut for --proc --sys --dev --devpts.\n");
	printf(" --proc           Mount proc filesystem to /proc.\n");
	printf(" --sys            Mount sysfs filesystem to /sys.\n");
	printf(" --dev            Mount devtmpfs filesystem to /dev.\n");
	printf(" --devpts         Mount devpts filesystem to /dev/pts. Implies --dev.\n");
	printf(" --run            Mount tmpfs filesystem to /run.\n");
	printf(" --tmp            Mount tmpfs filesystem to /tmp.\n");
	printf("\nRootless mode requires file capabilities to be set:\n");
	printf(" sudo setcap =ep \"<prog_path>\"\n");
}

#define BIND_LIMIT 100
struct bind_mount {
	char src[PATH_MAX];
	char dst[PATH_MAX];
};

#define ENV_LIMIT 100
#define ENV_VAR_MAX 1000

int main(int argc, char* argv[]) {
	prog = argv[0];

	char *root_dir = "/";
	char *ch_dir = "/";
	int uid = -1;
	int gid = -1;
	int proc = 0;
	int sys = 0;
	int dev = 0;
	int devpts = 0;
	int run = 0;
	int tmp = 0;
	int clear_env = 0;

	struct bind_mount binds[BIND_LIMIT];
	int bind_count = 0;

	char env[ENV_LIMIT][ENV_VAR_MAX];
	int env_count = 0;

	if (argc == 1) {
		help();
		return 1;
	}

	opterr = 0;
	int arg;
	while ((arg = getopt(argc, argv, ":hr:d:u:g:-::b:e:E")) != -1) {
		switch(arg) {
			case 'h':
				help();

				return 0;
				
			// root dir
			case 'r':
				root_dir = optarg;
				
				break;
				
			// working dir inside chroot
			case 'd':
				ch_dir = optarg;
				
				break;

			// uid
			case 'u':
				uid = atoi(optarg);
				
				break;

			// gid
			case 'g':
				gid = atoi(optarg);
				
				break;

			// binds -b /usr/locall
			case 'b':
				char *src = strtok(optarg, sep);	
				char *dst = strtok(NULL, sep);

				if (dst == NULL)
					dst = src;

				strcpy(binds[bind_count].src, src);					 
				strcpy(binds[bind_count].dst, dst);
					
				bind_count++;
				
				break;

			case 'E':
				clear_env = 1;
				break;

			case 'e':
				if (strlen(optarg) < ENV_VAR_MAX) {
					strcpy(env[env_count], optarg);
					env_count++;
				}
				break;

			// flags
			case '-':	
				int val = 1;

				if (strcmp(optarg, "chroot") == 0) 
					proc = sys = dev = devpts = val;
				else if (strcmp(optarg, "proc") == 0) 
					proc = val;
				else if (strcmp(optarg, "sys") == 0) 
					sys = val;
				else if (strcmp(optarg, "dev") == 0) 
					dev = val;
				else if (strcmp(optarg, "devpts") == 0) 
					dev = devpts = val;
				else if (strcmp(optarg, "run") == 0) 
					run = val;
				else if (strcmp(optarg, "tmp") == 0) 
					tmp = val;
																				
				break;
			
			case '?':
				printf("%s%s: unrecognized option: %c%s\n", _COLOR_START_RED, prog, optopt, _COLOR_STOP);
				help();
				return 1;
							
			case ':':
				printf("%s%s: option requires an argument: %c%s\n", _COLOR_START_RED, prog, optopt, _COLOR_STOP);
				help();
				return 1;
		}
	}


	if (uid < 0) 
		uid = getuid();
		
	if (gid < 0) 
		gid = getgid();	

	TRY_SYSCALL(unshare(CLONE_NEWNS | CLONE_FS | CLONE_THREAD))
	TRY_SYSCALL(chdir("/"))

	for (int index = 0; index < bind_count; index++) {
		char *target;
		asprintf(&target, "%s/%s", root_dir, binds[index].dst);					
		TRY_SYSCALL(bind(binds[index].src, target))
	}


	if (strcmp(root_dir, "/") != 0) {
		TRY_SYSCALL(chroot(root_dir))
 		TRY_SYSCALL(bind("/", "/"))
 	}
 	
	if (proc)
		TRY_SYSCALL(mount("proc", "/proc", "proc", 0, NULL)) 	
		
 	if (dev) 
 		TRY_SYSCALL(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL))
 		
	if (devpts)
		TRY_SYSCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL))
		
	if (sys)
		TRY_SYSCALL(mount("none", "/sys", "sysfs", 0, NULL))
		
	if (tmp)
		TRY_SYSCALL(mount("none", "/tmp", "tmpfs", 0, NULL))
		
	if (run) 
		TRY_SYSCALL(mount("none", "/run", "tmpfs", 0, NULL))

  	TRY_SYSCALL(setgid(uid))
  	TRY_SYSCALL(setuid(gid))

	if (clear_env) 
		clearenv();

	for (int index = 0; index < env_count; index++)
		TRY_SYSCALL(putenv(env[index]))

  	TRY_SYSCALL(chdir(ch_dir))

  	if (argc > optind) 
  		TRY_SYSCALL(execvp(argv[optind], &argv[optind]))
  		
  	TRY_SYSCALL(execlp("sh", "sh", NULL))
}
