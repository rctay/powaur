#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <alpm_list.h>

#include "conf.h"
#include "environment.h"
#include "util.h"

struct config_t *config;

enum _pw_errno_t pwerrno = PW_ERR_OK;
char *powaur_dir;
char *powaur_editor;

/* Pacman configuration */
char *pacman_rootdir;
char *pacman_dbpath;
alpm_list_t *pacman_cachedirs;

struct commonstrings comstrs = {
	"powaur", "usage: ", "package(s)", "options", "    ",

	/* Pacman configuration */
	"/etc/pacman.conf", "RootDir", "DBPath", "CacheDir",

	/* -Si, -Qi */
	"Repository     :", "Name           :", "Version        :",
	"URL            :", "Licenses       :", "Groups         :",
	"Provides       :", "Depends On     :",	"Optional Deps  :",
	"Required By    :",
	"Conflicts With :", "Replaces       :", "Download Size  :",
	"Installed Size :", "Packager       :", "Architecture   :",
	"Build Date     :", "Install Date   :", "Install Reason :",
	"Install Script :", "MD5 Sum        :", "Description    :",

	/* -Si, -Qi for AUR */
	"AUR URL        :", "Votes          :", "Out of Date    :"
};

int setup_config(void)
{
	config = config_init();
	ASSERT(config != NULL, RET_ERR(PW_ERR_INIT_CONFIG, -1));

	return 0;
}

/* @param reload set to > 0 if reloading libalpm so that cachedirs can be
 * reinitialized.
 */
static int setup_pacman_environment(int reload)
{
	if (reload) {
		pw_printf(PW_LOG_DEBUG, "Reloading pacman configuration\n");
		pacman_cachedirs = NULL;
	}

	if (parse_pmconfig()) {
		/* Free cachedirs */
		FREELIST(pacman_cachedirs);
		RET_ERR(PW_ERR_PM_CONF_PARSE, -1);
	}

	if (!pacman_rootdir) {
		STRDUP(pacman_rootdir, PACMAN_DEF_ROOTDIR, RET_ERR(PW_ERR_MEMORY, -1));
	}

	if (!pacman_dbpath) {
		STRDUP(pacman_dbpath, PACMAN_DEF_DBPATH, RET_ERR(PW_ERR_MEMORY, -1));
	}

	if (!pacman_cachedirs) {
		pacman_cachedirs = alpm_list_add(pacman_cachedirs,
										 strdup(PACMAN_DEF_CACHEDIR));
	}

	alpm_option_set_root(pacman_rootdir);
	alpm_option_set_dbpath(pacman_dbpath);
	alpm_option_set_cachedirs(pacman_cachedirs);

	return 0;
}

static int setup_powaur_config(void)
{
	/* Check the following places for logfile:
	 * $XDG_CONFIG_HOME
	 * $HOME
	 *
	 * Then fallback to default settings
	 */

	int ret = -1;
	FILE *fp = NULL;
	char *dir;
	char buf[PATH_MAX];
	struct stat st;

	pw_printf(PW_LOG_DEBUG, "%s: Setting up powaur configuration\n", __func__);

	dir = getenv("XDG_CONFIG_HOME");
	if (dir) {
		snprintf(buf, PATH_MAX, "%s/%s", dir, PW_CONF);

		if (!stat(buf, &st)) {
			fp = fopen(buf, "r");
			if (!fp) {
				goto check_home;
			}

			pw_printf(PW_LOG_DEBUG, "%sParsing %s\n", comstrs.tab, buf);
			ret = parse_powaur_config(fp);
			fclose(fp);

			if (ret != -1) {
				goto cleanup;
			} else {
				goto check_home;
			}
		}
	}

check_home:
	/* Check $HOME */
	dir = getenv("HOME");
	if (dir) {
		snprintf(buf, PATH_MAX, "%s/.config/%s", dir, PW_CONF);

		if (!stat(buf, &st)) {
			fp = fopen(buf, "r");
			if (!fp) {
				goto cleanup;
			}

			pw_printf(PW_LOG_DEBUG, "%sParsing %s\n", comstrs.tab, buf);
			ret = parse_powaur_config(fp);
			fclose(fp);
			goto cleanup;
		}
	}

cleanup:

	/* Use default settings for unspecified options */
	if (!powaur_dir) {
		STRDUP(powaur_dir, PW_DEF_DIR, RET_ERR(PW_ERR_MEMORY, -1));
		pw_printf(PW_LOG_DEBUG, "%sFalling back to default directory %s\n",
				  comstrs.tab, powaur_dir);
	}

	if (!powaur_editor) {
		STRDUP(powaur_editor, PW_DEF_EDITOR, RET_ERR(PW_ERR_MEMORY, -1));
		pw_printf(PW_LOG_DEBUG, "%sFalling back to default editor %s\n",
				  comstrs.tab, powaur_editor);
	}

	return 0;
}

int setup_environment(void)
{
	if (setup_powaur_config()) {
		return -1;
	}

	return setup_pacman_environment(0);
}

void cleanup_environment(void)
{
	if (config) {
		config_free(config);
	}

	free(powaur_editor);
	free(powaur_dir);

	/* No need to free pacman_cachedirs */
	free(pacman_rootdir);
	free(pacman_dbpath);
}

int alpm_reload(void)
{
	pw_printf(PW_LOG_DEBUG, "Reloading libalpm\n");

	int ret = alpm_release();
	if (ret) {
		return ret;
	}

	ret = alpm_initialize();
	if (ret) {
		return ret;
	}

	return setup_pacman_environment(1);
}
