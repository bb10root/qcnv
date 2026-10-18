/*
 * Copyright (c) 2026 Oleksandr <oleksandr@e.email>
 *
 * This source code is licensed under the terms found in the LICENSE file
 * in the root directory of this source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "qc_nv_lib.h"

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_NV_ITEMS 65536 // resp.item_id is uint16, higher ids never match
#define NV_IMEI 550        // never written unless --allow-imei
#define NV_BUF_SIZE 4096 // Деякі нові елементи можуть бути більшими за 2КБ

// Допоміжна функція для створення папки
void make_dir(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0755) != 0)
        {
            perror("Error creating directory");
        }
    }
}

// --- BACKUP: Збереження всіх знайдених NV у файли ---
void backup_all_nv(const char *dir_path)
{
    uint8_t buffer[NV_BUF_SIZE];
    uint32_t r_len;
    int success_count = 0;

    make_dir(dir_path);
    printf("Starting NV backup (0-%d) to '%s'...\n", MAX_NV_ITEMS - 1, dir_path);

    for (uint32_t id = 0; id < MAX_NV_ITEMS; id++)
    {
        r_len = NV_BUF_SIZE;

        // Спроба прочитати ітем
        if (nv_read_item(id, buffer, &r_len) == 0 && r_len > 0)
        {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%u.nv", dir_path, id);

            FILE *fp = fopen(file_path, "wb");
            if (fp)
            {
                fwrite(buffer, 1, r_len, fp);
                fclose(fp);
                success_count++;
            }
        }

        // Прогрес-бар кожні 256 ітемів
        if (id % 256 == 0)
        {
            printf("Progress: [%u/%d] Found: %d items\r", id, MAX_NV_ITEMS, success_count);
            fflush(stdout);
        }
    }
    printf("\nBackup finished! Total items saved: %d\n", success_count);
}

// --- RESTORE: Завантаження NV з файлів у модем ---
void restore_all_nv(const char *dir_path, int allow_imei)
{
    struct dirent *de;
    DIR *dr = opendir(dir_path);

    if (dr == NULL)
    {
        printf("Error: Cannot open directory '%s' (errno: %d)\n", dir_path, errno);
        return;
    }

    int success_count = 0;
    int fail_count = 0;

    printf("Starting NV restore from '%s'...\n", dir_path);

    // Включаємо прапор перезапису перед початком
    nv_set_overwrite_flag(1);
    uint8_t *buf = malloc(8192);
    if (!buf)
    {
        closedir(dr);
        return;
    }

    while ((de = readdir(dr)) != NULL)
    {
        // Only "<digits>.nv": atoi() on any other name would give 0 and overwrite NV 0.
        char *end = NULL;
        unsigned long lid = strtoul(de->d_name, &end, 10);
        if (end != de->d_name && strcmp(end, ".nv") == 0 && lid < MAX_NV_ITEMS)
        {
            uint32_t id = (uint32_t)lid;
            if (id == NV_IMEI && !allow_imei)
            {
                printf("\nSkipping NV %u (IMEI); use --allow-imei to restore it\n", id);
                continue;
            }
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, de->d_name);

            FILE *fp = fopen(file_path, "rb");
            if (fp)
            {
                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                if (fsize <= 0 || fsize > NV_BUF_SIZE)
                {
                    printf("\nBlock %u skipped: size %ld\n", id, fsize);
                    fail_count++;
                }
                else
                {
                    if (fread(buf, 1, fsize, fp) == (size_t)fsize)
                    {
                        if (nv_write_item(id, buf, (uint32_t)fsize) == 1)
                        {
                            success_count++;
                        }
                        else
                        {
                            printf("\nBlock %u write failed\n", id);
                            fail_count++;
                        }
                    }
                    else
                    {
                        printf("\nBlock %u read from file failed\n", id);
                        fail_count++;
                    }
                }
                fclose(fp);
            }

            if ((success_count + fail_count) % 10 == 0)
            {
                printf("Restoring: %d items processed...\r", success_count + fail_count);
                fflush(stdout);
            }
        }
    }
    free(buf);

    closedir(dr);

    // Обов'язково скидаємо зміни на диск модема
    efs_force_sync();

    printf("\nRestore complete!\n- Success: %d\n- Failed: %d\n", success_count, fail_count);
}

void print_usage(const char *bin_name)
{
    printf(
        "Usage: %1$s <command> <id/path> [value] [-f <file>]\n\n"
        "  NV:    %1$s read/write   <id>   [value/file]\n"
        "  IMS:   %1$s iread/iwrite <id>   [value/file]\n"
        "  EFS:   %1$s fread/fwrite <path> [value/file]\n\n",
        bin_name);
    printf("  Backup All: %s backup <dir_name>\n", bin_name);
    printf("  Restore All:%s restore <dir_name> [--allow-imei]\n", bin_name);
    printf("  NV %d (IMEI) is never written without --allow-imei.\n", NV_IMEI);
}

void hex_dump(const uint8_t *data, uint32_t len)
{
    if (len == 0 || data == NULL)
        return;
    printf("--- DATA DUMP (%u bytes) ---\n", len);
    for (uint32_t i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");
    printf("---------------------------\n");
}

int hex_to_bytes(const char *hex, uint8_t *bytes, size_t *out_len)
{
    size_t len = strlen(hex);
    if (len % 2 != 0)
        return -1;

    for (size_t i = 0; i < len / 2; i++)
    {
        unsigned int byte;
        if (sscanf(hex + (i * 2), "%02x", &byte) != 1)
            return -1;
        bytes[i] = (uint8_t)byte;
    }
    *out_len = len / 2;
    return 0;
}

#define APP_NAME "BB10 Qualcomm NV tool"
#define APP_VERSION "1.4.0"

#include <banner.h>

int main(int argc, char *argv[])
{
    display_system_banner();
    if (argc < 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    const char *target = argv[2];
    const char *filename = NULL;
    int allow_imei = 0;
    int ret = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0 && (i + 1) < argc)
            filename = argv[++i];
        else if (strcmp(argv[i], "--allow-imei") == 0)
            allow_imei = 1;
    }
    if (strcmp(cmd, "backup") == 0)
    {
        backup_all_nv(target); // target тут — це назва папки
    }
    else if (strcmp(cmd, "restore") == 0)
    {
        restore_all_nv(target, allow_imei);
    }

    // --- ЧИТАННЯ (read / fread / iread) ---
    else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "fread") == 0 || strcmp(cmd, "iread") == 0)
    {
        uint8_t *buffer = NULL;
        size_t data_len = 0;

        if (strcmp(cmd, "read") == 0)
        {
            uint32_t id = (uint32_t)atoi(target);
            buffer = malloc(2048);
            uint32_t r_len = 2048;
            if (nv_read_item(id, buffer, &r_len) == 0)
                data_len = r_len;
        }
        else if (strcmp(cmd, "iread") == 0)
        {
            int id = atoi(target);
            // ims_get_parm() writes a NUL-terminated string of at most 127 chars.
            buffer = calloc(1, 128);
            if (buffer && ims_get_parm(id, (char *)buffer) == 0)
            {
                data_len = strlen((char *)buffer);
            }
        }
        else // fread
        {
            uint32_t f_size = efs_get_file_size(target);
            if (f_size > 0 && (buffer = malloc(f_size)) != NULL)
            {
                data_len = f_size;
                if (f_size < 0x800)
                {
                    if (efs_read_file(target, buffer, &data_len) != 0)
                        data_len = 0;
                }
                else if (efs_read_large_file(target, buffer, &data_len) != 0)
                    data_len = 0;
            }
        }

        if (data_len > 0)
        {
            if (filename)
            {
                FILE *fp = fopen(filename, "wb");
                if (fp)
                {
                    fwrite(buffer, 1, data_len, fp);
                    fclose(fp);
                    printf("Success: Saved %zu bytes to '%s'\n", data_len, filename);
                }
                else
                    perror("File open error");
            }
            else
            {
                if (strcmp(cmd, "iread") == 0)
                    printf("IMS Param %s Raw Data:\n", target);
                hex_dump(buffer, (uint32_t)data_len);
            }
        }
        else
        {
            printf("Error: Could not read '%s'\n", target);
            ret = 1;
        }
        free(buffer);
    }

    // --- ЗАПИС (write / fwrite / iwrite) ---
    else if (strcmp(cmd, "write") == 0 || strcmp(cmd, "fwrite") == 0 || strcmp(cmd, "iwrite") == 0)
    {
        uint8_t *payload = NULL;
        size_t p_len = 0;

        if (filename)
        {
            FILE *fp = fopen(filename, "rb");
            if (fp)
            {
                fseek(fp, 0, SEEK_END);
                p_len = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                payload = malloc(p_len + 1); // +1 для null-terminator якщо це текст
                fread(payload, 1, p_len, fp);
                payload[p_len] = '\0';
                fclose(fp);
            }
            else
                perror("Source file error");
        }
        else if (argc >= 4)
        {
            if (strcmp(cmd, "iwrite") == 0)
            {
                // Для IMS записуємо як звичайний рядок
                payload = (uint8_t *)strdup(argv[3]);
                p_len = strlen((char *)payload);
            }
            else
            {
                const char *hex_str = argv[3];
                payload = malloc(strlen(hex_str) / 2 + 1);
                if (hex_to_bytes(hex_str, payload, &p_len) != 0)
                {
                    printf("Error: Invalid HEX string format.\n");
                    free(payload);
                    payload = NULL;
                }
            }
        }

        if (payload && strcmp(cmd, "write") == 0 && (uint32_t)atoi(target) == NV_IMEI && !allow_imei)
        {
            printf("Error: NV %d is the IMEI; refusing to write without --allow-imei.\n", NV_IMEI);
            free(payload);
            payload = NULL;
            ret = 1;
        }
        else if (payload)
        {
            if (strcmp(cmd, "iwrite") == 0)
            {
                int id = atoi(target);
                nv_set_overwrite_flag(1);
                if (ims_set_parm(id, (const char *)payload) == 0)
                {
                    efs_force_sync();
                    printf("Success: IMS parameter %d updated to '%s'\n", id, payload);
                }
                else
                {
                    printf("Error: IMS set_parm failed\n");
                    ret = 1;
                }
            }
            else if (strcmp(cmd, "fwrite") == 0)
            {
                nv_set_overwrite_flag(1);
                int res = (p_len > 0x800) ? efs_write_large_file(target, payload, (uint32_t)p_len) : efs_write_file(target, payload, (uint32_t)p_len);

                if (res == 0)
                {
                    efs_force_sync();
                    printf("Success: Written %zu bytes to EFS '%s'\n", p_len, target);
                }
                else
                {
                    printf("Error: EFS write failed\n");
                    ret = 1;
                }
            }
            else // write (NV)
            {
                uint32_t id = (uint32_t)atoi(target);
                nv_set_overwrite_flag(1);
                if (nv_write_item(id, payload, p_len) == 1)
                {
                    efs_force_sync();
                    printf("Success: Written %zu bytes to NV %d\n", p_len, id);
                }
                else
                {
                    printf("Error: NV %u write failed (max 128 bytes)\n", id);
                    ret = 1;
                }
            }
            free(payload);
        }
        else if (!ret)
        {
            printf("Error: No data to write.\n");
            ret = 1;
        }
    }
    else
    {
        print_usage(argv[0]);
        ret = 1;
    }

    release_qmi_client();
    return ret;
}