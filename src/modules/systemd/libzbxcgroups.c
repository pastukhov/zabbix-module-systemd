#include "libzbxsystemd.h"
#include "libzbxcgroups.h"

// cgroup directories
char *cgroup_dir = NULL, *cpu_cgroup = NULL;

/******************************************************************************
 *                                                                            *
 * Function: cgroup_dir_detect                                                *
 *                                                                            *
 * Purpose: it should find cgroup metric directory                            *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - cgroup directory was not found            *
 *               SYSINFO_RET_OK - cgroup directory was found                  *
 *                                                                            *
 ******************************************************************************/
int     cgroup_dir_detect()
{
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "in cgroup_dir_detect()");

        char path[512];
        char *temp1, *temp2, *cgroup, *ddir;
        FILE *fp;
        DIR *dir;
        size_t ddir_size;

        if ((fp = fopen("/proc/mounts", "r")) == NULL)
        {
            zabbix_log(LOG_LEVEL_WARNING, LOG_PREFIX "cannot open /proc/mounts: %s", zbx_strerror(errno));
            return SYSINFO_RET_FAIL;
        }

        while (fgets(path, 512, fp) != NULL)
        {
            if ((strstr(path, "cpuset cgroup")) != NULL)
            {
                temp1 = string_replace(path, "cgroup ", "");
                temp2 = string_replace(temp1, strstr(temp1, " "), "");
                free(temp1);
                cgroup_dir = string_replace(temp2, "cpuset", "");
                free(temp2);
                zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "detected cgroup mount directory: %s", cgroup_dir);

                // detect cpu_cgroup - JoinController cpu,cpuacct
                cgroup = "cpu,cpuacct/";
                ddir_size = strlen(cgroup) + strlen(cgroup_dir) + 1;
                ddir = malloc(ddir_size);
                zbx_strlcpy(ddir, cgroup_dir, ddir_size);
                zbx_strlcat(ddir, cgroup, ddir_size);
                if (NULL != (dir = opendir(ddir)))
                {
                    closedir(dir);
                    cpu_cgroup = "cpu,cpuacct/";
                    zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "cpu_cgroup is JoinController cpu,cpuacct");
                } else {
                    cpu_cgroup = "cpuacct/";
                    zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "cpu_cgroup is cpuacct");
                }

                pclose(fp);
                return SYSINFO_RET_OK;
            }
        }
        pclose(fp);
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "cannot detect cgroup mount directory");
        return SYSINFO_RET_FAIL;
}

/******************************************************************************
 *                                                                            *
 * Function: SYSTEMD_CGROUP_MEM                                               *
 *                                                                            *
 * Purpose: cgroup memory metrics                                             *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                 as not supported by zabbix                 *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Notes: https://www.kernel.org/doc/Documentation/cgroups/memory.txt         *
 ******************************************************************************/
int     SYSTEMD_CGROUP_MEM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "in cgroup_mem(()");
        char    *unit, *metric;
        int     ret = SYSINFO_RET_FAIL;

        if (2 != request->nparam)
        {
                zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "invalid number of parameters: %d",  request->nparam);
                SET_MSG_RESULT(result, strdup("Invalid number of parameters"));
                return SYSINFO_RET_FAIL;
        }

        if (cgroup_dir == NULL)
        {
                zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "systemd.cgroup.mem metrics are not available at the moment - no cgroup directory");
                SET_MSG_RESULT(result, zbx_strdup(NULL, "systemd.cgroup.mem metrics are not available at the moment - no cgroup directory"));
                return SYSINFO_RET_FAIL;
        }

        unit = get_rparam(request, 0);
        metric = get_rparam(request, 1);
        char    *stat_file = "/memory.stat";
        char    *cgroup = "memory/system.slice/";
        size_t  filename_size = strlen(cgroup) + strlen(unit) + strlen(cgroup_dir) + strlen(stat_file) + 2;
        char    *filename = malloc(filename_size);
        zbx_strlcpy(filename, cgroup_dir, filename_size);  // /sys/fs/cgroup/
        zbx_strlcat(filename, cgroup, filename_size);      // /sys/fs/cgroup/memory/system.slice/
        zbx_strlcat(filename, unit, filename_size);        // /sys/fs/cgroup/memory/system.slice/dbus.service
        zbx_strlcat(filename, stat_file, filename_size);   // /sys/fs/cgroup/memory/system.slice/dbus.service/memory.stat
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "metric source file: %s", filename);
        FILE    *file;
        if (NULL == (file = fopen(filename, "r")))
        {
                zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "cannot open metric file: '%s'", filename);
                free(filename);
                SET_MSG_RESULT(result, strdup("Cannot open memory.stat file"));
                return SYSINFO_RET_FAIL;
        }

        char    line[MAX_STRING_LEN];
        char    *metric2 = malloc(strlen(metric)+3);
        memcpy(metric2, metric, strlen(metric));
        memcpy(metric2 + strlen(metric), " ", 2);
        zbx_uint64_t    value = 0;
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "looking metric %s in memory.stat file", metric);
        while (NULL != fgets(line, sizeof(line), file))
        {
                if (0 != strncmp(line, metric2, strlen(metric2)))
                        continue;
                if (1 != sscanf(line, "%*s " ZBX_FS_UI64, &value))
                {
                        zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "sscanf failed for matched metric line");
                        continue;
                }
                zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "unit: %s; metric: %s; value: %d", unit, metric, value);
                SET_UI64_RESULT(result, value);
                ret = SYSINFO_RET_OK;
                break;
        }
        zbx_fclose(file);
        free(filename);
        free(metric2);

        if (SYSINFO_RET_FAIL == ret)
                SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot find a line with requested metric in memory.stat file"));
        return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: cgroup_cpu                                                       *
 *                                                                            *
 * Purpose: cpu metrics                                                       *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                 as not supported by zabbix                 *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Notes: https://www.kernel.org/doc/Documentation/cgroups/cpuacct.txt        *
 ******************************************************************************/
int     cgroup_cpu(AGENT_REQUEST *request, AGENT_RESULT *result)
{
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "In cgroup_cpu()");

        char    *unit, *metric;
        int     ret = SYSINFO_RET_FAIL;

        if (2 != request->nparam)
        {
                zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "Invalid number of parameters: %d",  request->nparam);
                SET_MSG_RESULT(result, strdup("Invalid number of parameters"));
                return SYSINFO_RET_FAIL;
        }

        if (cgroup_dir == NULL)
        {
                zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "systemd.cgroup.cpu metrics are not available at the moment - no cgroup directory");
                SET_MSG_RESULT(result, zbx_strdup(NULL, "systemd.cgroup.cpu metrics are not available at the moment - no cgroup directory"));
                return SYSINFO_RET_FAIL;
        }

        unit = get_rparam(request, 0);
        metric = get_rparam(request, 1);
        char    *cgroup = NULL, *stat_file = NULL;
        if(strcmp(metric, "user") == 0 || strcmp(metric, "system") == 0 || strcmp(metric, "total") == 0) {
            stat_file = "/cpuacct.stat";
            cgroup = cpu_cgroup;
        } else {
            stat_file = "/cpu.stat";
            if (strchr(cpu_cgroup, ',') != NULL) {
                cgroup = cpu_cgroup;
            } else {
                cgroup = "cpu/system.slice/";
            }
        }
        size_t  filename_size = strlen(cgroup) + strlen(unit) + strlen(cgroup_dir) + strlen(stat_file) + 2;
        char    *filename = malloc(filename_size);
        zbx_strlcpy(filename, cgroup_dir, filename_size);
        zbx_strlcat(filename, cgroup, filename_size);
        zbx_strlcat(filename, unit, filename_size);
        zbx_strlcat(filename, stat_file, filename_size);
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "metric source file: %s", filename);
        FILE    *file;
        if (NULL == (file = fopen(filename, "r")))
        {
                zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "Cannot open metric file: '%s'", filename);
                free(filename);
                SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot open %s file", ++stat_file));
                return SYSINFO_RET_FAIL;
        }

        char    line[MAX_STRING_LEN];
        char    *metric2 = malloc(strlen(metric)+3);
        zbx_uint64_t cpu_num;
        memcpy(metric2, metric, strlen(metric));
        memcpy(metric2 + strlen(metric), " ", 2);
        zbx_uint64_t    value = 0;
        zbx_uint64_t    result_value = 0;
        zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "Looking metric %s in cpuacct.stat/cpu.stat file", metric);
        while (NULL != fgets(line, sizeof(line), file))
        {
                if (0 != strcmp("total", metric) && 0 != strncmp(line, metric2, strlen(metric2))) {
                        continue;
                }
                if (1 != sscanf(line, "%*s " ZBX_FS_UI64, &value))
                {
                        zabbix_log(LOG_LEVEL_ERR, LOG_PREFIX "sscanf failed for matched metric line");
                        continue;
                }
                result_value += value;
                ret = SYSINFO_RET_OK;
        }

        zbx_fclose(file);
        free(filename);
        free(metric2);

        if (SYSINFO_RET_FAIL == ret) {
                SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot find a line with requested metric in cpuacct.stat/cpu.stat file"));
        } else {
                // normalize CPU usage by using number of online CPUs - only tick metrics
                if ((strcmp(metric, "user") == 0 || strcmp(metric, "system") == 0 || strcmp(metric, "total") == 0) && (1 < (cpu_num = sysconf(_SC_NPROCESSORS_ONLN))))
                {
                        result_value /= cpu_num;
                }

                zabbix_log(LOG_LEVEL_DEBUG, LOG_PREFIX "unit: %s; metric: %s; value: %d", unit, metric, result_value);
                SET_UI64_RESULT(result, result_value);
        }

        return ret;
}
