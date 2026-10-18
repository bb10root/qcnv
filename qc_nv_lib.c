/*
 * Copyright (c) 2026 Oleksandr <oleksandr@e.email>
 *
 * This source code is licensed under the terms found in the LICENSE file
 * in the root directory of this source tree.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>
#include "qc_nv_lib.h"

/* --- Типи функцій для dlsym (Прототипи QMI) --- */
typedef void *(*get_svc_obj_t)(int, int, int);
typedef int (*qmi_get_list_t)(void *, void *, uint32_t *, uint32_t *);
typedef int (*qmi_init_t)(void *, void *, void *, void *, void *, int *);
typedef int (*qmi_release_t)(int);
typedef int (*qmi_send_sync_t)(int, int, void *, int, void *, int, int);

static get_svc_obj_t p_get_service_object = NULL;
static qmi_get_list_t p_get_service_list = NULL;
static qmi_init_t p_qmi_init = NULL;
static qmi_release_t p_qmi_release = NULL;
static qmi_send_sync_t p_qmi_send_sync = NULL;

////////////////////////////////////////////////////

typedef int (*rimnv_init_qmi_client_shim_t)(void);
typedef int (*rimnv_read_nv_item_t)(int, void *, uint32_t *, uint32_t *);
typedef int (*rimnv_write_nv_item_t)(int, void *, uint32_t *, uint32_t *);
typedef int (*rimnv_set_nv_overwriteFlag_t)(int mode);

static rimnv_init_qmi_client_shim_t rimnv_init_qmi_client_shim = NULL;
static rimnv_read_nv_item_t rimnv_read_nv_item = NULL;
static rimnv_write_nv_item_t rimnv_write_nv_item = NULL;
static rimnv_set_nv_overwriteFlag_t rimnv_set_nv_overwriteFlag = NULL;

/* --- Константи та Msg ID --- */
#define RIM_SVC_ID 5
#define QMI_TIMEOUT_MS 5000
#define CHUNK_SIZE 0x800
#define EFS_MAX_PATH 128
#define MAX_NV_ITEM_SIZE 128
#define IMS_CONFIG_RESP_SIZE 136

#define MSG_ID_READ_NV_ITEM 1
#define MSG_ID_WRITE_NV_ITEM 2
#define MSG_ID_READ_EFS 3
#define MSG_ID_WRITE_EFS 4
#define MSG_ID_GET_IMS_PARM 5
#define MSG_ID_SET_IMS_PARM 6
#define MSG_ID_SID_HYSTERESIS_REG 7
// #define MSG_ID_   8  (27, 9)  [6, 2]
// #define MSG_ID_   9  (29, 30) [8, 4]
// #define MSG_ID_   10 (31, 22) [20, 1]
#define MSG_ID_RRC_STATE_IND_REG 11 // 0x0B
// #define MSG_ID_   12 (34, 18) [12, 1]
#define MSG_ID_CDMA_OTA_IND_REG 13 // 0x0D
// #define MSG_ID_   14 (37, 7) [4, 264]
#define MSG_ID_EFS_FORCE_SYNC 15 // 0x0F
// #define MSG_ID_   16 (5, 4) [4, 8]
#define MSG_ID_GET_PHY_LINK_DOWN_CAUSE 17    // 0x11
#define MSG_ID_SET_OVERWRITE 18              // 0x12
#define MSG_ID_SET_LTE_MODE 19               // 0x13
#define MSG_ID_GET_LTE_ATTACH_PARAMS 20      // 0x14
#define MSG_ID_UICC_SET_CDMA_SUBSCRIPTION 21 // 0x15
// #define MSG_ID_   22 (6, 7) [1, 264]
#define MSG_ID_GPS_E911_START_EMERG 24 // 0x18
#define MSG_ID_SMS_IMS_INCR_MSG_ID 25  // 0x19
#define MSG_ID_SMS_IMS_GET_MSG_ID 26   // 0x1A
// #define MSG_ID_   27 (54, 55) [140, 4]
#define MSG_ID_EPDG_SET_HANDOVER_FLAG 28 // 0x1C
#define MSG_ID_GET_MP_IPV6_ADDR 29       // 0x1D
#define MSG_ID_SET_LTE_PDN_PRIORITY 30   // 0x1E
// #define MSG_ID_   31 (20, 21) [1, 1]
#define MSG_ID_GET_FILE_SIZE 32      // 0x20
#define MSG_ID_NAS_SET_E911_STATE 33 // 0x21
// #define MSG_ID_   34 (66, 12) [8, 136]
#define MSG_ID_GET_LTE_SSAC_BARRING 35 // 0x23
// #define MSG_ID_   36 (22, 23) [1, 1]
#define MSG_ID_RM_EFS_FILE 37 // 0x25
// #define MSG_ID_   38 (28, 19) [16, 4]

#include <stdint.h>

// Структура опису сервісу QMI (Service Object)
typedef struct
{
    uint32_t service_id;         // 0x00: ID сервісу (у дампі 0x05 - ймовірно QMI_DMS або NV)
    uint32_t idl_version;        // 0x04: Версія IDL (0x01)
    uint32_t service_id_version; // 0x08: Комбінована версія (0xE3 = 227)
    uint32_t max_msg_len;        // 0x0C: Макс. довжина повідомлення (0x0896 = 2198 байт)

    uint16_t unknown_flags_1; // 0x10: 0x0020 (32)
    uint16_t unknown_flags_2; // 0x12: 0x0020 (32)

    uint32_t msg_table_size; // 0x14: Кількість повідомлень у таблиці (0x06)

    // Вказівники на таблиці опису повідомлень (IDL tables)
    // У дампі вони вказують на адреси в області 0x2A52xxxx
    const void *req_table_ptr;  // 0x18: Таблиця запитів (Request)
    const void *resp_table_ptr; // 0x1C: Таблиця відповідей (Response)
    const void *ind_table_ptr;  // 0x20: Таблиця індикацій (Indication)

    const void *idl_base_ptr; // 0x24: Вказівник на базові метадані IDL

    uint32_t type_table_size; // 0x28: Кількість типів (0x02)
    uint32_t reserved;        // 0x2C: Зарезервовано (у дампі 0x00)
} qmi_idl_service_object_t;

/* Глобальні змінні */
static uint32_t g_qmi_handle = 0;
static pthread_mutex_t g_nv_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Секція 1: Динамічне завантаження символів
 */
int init_external_symbols(void)
{

    if (p_get_service_object && p_get_service_list && p_qmi_send_sync && p_qmi_init && p_qmi_release)
        return 0;

    // 1. Динамічно формуємо LD_LIBRARY_PATH на основі знайдених шляхів
    // Ми використовуємо стандартні для вашої системи шляхи з того скрипта
    const char *base = "/base";   // зазвичай BASEFS це корінь
    const char *radio = "/radio"; // зазвичай RADIOFS монтується сюди

    char new_ld_path[1024];
    snprintf(new_ld_path, sizeof(new_ld_path),
             "%s/usr/lib:%s/lib:%s/lib/dll:%s/lib:%s/lib/dll:%s/usr/lib:/proc/boot:.",
             base, base, base, radio, radio, radio);

    // Встановлюємо змінну оточення для поточного процесу
    // 1 - означає "перезаписати", якщо вона вже існує
    setenv("LD_LIBRARY_PATH", new_ld_path, 1);

#ifdef DEBUG
    // fprintf(stderr, "DEBUG: Setting LD_LIBRARY_PATH to: %s\n", new_ld_path);
#endif

    //     const char *core_libs[] = {
    //         "libencdec.so",
    //         "libqcnv.so",
    //         "libqmi_client_qmux.so",
    //         "libqmi_common.so",
    //     };

    //     int n_libs = sizeof(core_libs) / sizeof(core_libs[0]);

    //     for (int i = 0; i < n_libs; i++)
    //     {
    //         // RTLD_GLOBAL робить символи з цих ліб доступними для всіх наступних dlopen
    //         void *h = dlopen(core_libs[i], RTLD_GLOBAL | RTLD_LAZY);

    // #ifdef DEBUG
    //         if (!h)
    //         {
    //             fprintf(stderr, "DEBUG: Skipping/Failed %s: %s\n", core_libs[i], dlerror());
    //         }
    //         else
    //         {
    //             fprintf(stderr, "DEBUG: Successfully loaded %s\n", core_libs[i]);
    //         }
    // #endif
    //     }

    void *h_qct = dlopen("stp-handler-dc-qct.so", RTLD_GLOBAL | RTLD_NOW);

    // Завантажуємо основні інтерфейсні бібліотеки
    void *h_rim_vs = dlopen("libqmi_rim_vs.so", RTLD_GLOBAL | RTLD_NOW);
    void *h_qcci = dlopen("libqcci_ext.so", RTLD_GLOBAL | RTLD_NOW);

    if (!h_rim_vs || !h_qcci || !h_qct)
    {
        fprintf(stderr, "ERR: Could not load QMI libs: %s\n", dlerror());
        return -1;
    }

    rimnv_init_qmi_client_shim = (rimnv_init_qmi_client_shim_t)dlsym(h_qct, "rimnv_init_qmi_client_shim");
    rimnv_read_nv_item = (rimnv_read_nv_item_t)dlsym(h_qct, "rimnv_read_nv_item");
    rimnv_write_nv_item = (rimnv_write_nv_item_t)dlsym(h_qct, "rimnv_write_nv_item");
    rimnv_set_nv_overwriteFlag = (rimnv_set_nv_overwriteFlag_t)dlsym(h_qct, "rimnv_set_nv_overwriteFlag");

    p_get_service_object = (get_svc_obj_t)dlsym(h_rim_vs, "qmi_vs_rim_service_get_service_object_internal_v01");
    p_get_service_list = (qmi_get_list_t)dlsym(h_qcci, "qmi_client_get_service_list");
    p_qmi_init = (qmi_init_t)dlsym(h_qcci, "qmi_client_init");
    p_qmi_release = (qmi_release_t)dlsym(h_qcci, "qmi_client_release");
    p_qmi_send_sync = (qmi_send_sync_t)dlsym(h_qcci, "qmi_client_send_msg_sync");

    if (!p_get_service_object || !p_get_service_list || !p_qmi_send_sync || !p_qmi_init || !p_qmi_release)
    {
        fprintf(stderr, "ERR: Some QMI symbols missing\n");
        return -1;
    }

    return 0;
}

/*
// Оголошуємо функцію-ініціалізатор з атрибутом constructor
void __attribute__((constructor)) auto_init_nv_lib(void)
{
    // Викликаємо вашу функцію ініціалізації символів
    int res = init_external_symbols();

#ifdef DEBUG
    if (res == 0)
    {
        fprintf(stderr, "[DEBUG] External symbols initialized automatically.\n");
    }
    else
    {
        fprintf(stderr, "[DEBUG] Failed to initialize external symbols: %d\n", res);
    }
#endif
}
*/

/**
 * Спеціалізований вивід для об'єкта QMI сервісу.
 * Допомагає швидко перевірити вказівники та ідентифікатори сервісу.
 */
void debug_print_qmi_svc_obj(const qmi_idl_service_object_t *svc)
{
    if (!svc)
    {
        printf("[ERROR] SVC_OBJ pointer is NULL\n");
        return;
    }

    printf("\n==================================================\n");
    printf("        QMI SERVICE OBJECT INTERPRETATION         \n");
    printf("==================================================\n");

    // Базові ідентифікатори
    printf("Service ID:         0x%08X (%u)\n", svc->service_id, svc->service_id);
    printf("IDL Version:        0x%08X\n", svc->idl_version);
    printf("IDL Version (Maj):  0x%08X (%u)\n", svc->service_id_version, svc->service_id_version);
    printf("Max Message Len:    %u bytes\n", svc->max_msg_len);

    // Прапорці
    printf("Flags:              0x%04X, 0x%04X\n", svc->unknown_flags_1, svc->unknown_flags_2);

    // Таблиці повідомлень (Message Tables)
    printf("--------------------------------------------------\n");
    printf("Message Table Size: %u entries\n", svc->msg_table_size);
    printf("REQ Table Pointer:  %p\n", svc->req_table_ptr);
    printf("RESP Table Pointer: %p\n", svc->resp_table_ptr);
    printf("IND Table Pointer:  %p\n", svc->ind_table_ptr);

    // Метадані та типи
    printf("--------------------------------------------------\n");
    printf("IDL Base Metadata:  %p\n", svc->idl_base_ptr);
    printf("Type Table Size:    %u\n", svc->type_table_size);
    printf("Reserved/Padding:   0x%08X\n", svc->reserved);
    printf("==================================================\n\n");
}

// Приблизна структура запису в таблиці повідомлень QMI IDL
typedef struct
{
    uint16_t msg_id;   // Ідентифікатор повідомлення (наприклад, 0x1770)
    uint16_t type_idx; // Індекс у таблиці типів
} qmi_idl_msg_table_entry_t;

void debug_print_qmi_tables(const qmi_idl_service_object_t *svc)
{
    if (!svc)
        return;

    printf("\n--- [ QMI MESSAGE TABLES DECODE ] ---\n");

    // Друкуємо таблицю запитів (REQUESTS)
    if (svc->req_table_ptr && svc->msg_table_size > 0)
    {
        printf("REQUEST TABLE (at %p):\n", svc->req_table_ptr);
        qmi_idl_msg_table_entry_t *entries = (qmi_idl_msg_table_entry_t *)svc->req_table_ptr;

        for (uint32_t i = 0; i < svc->msg_table_size; i++)
        {
            printf("  [%d] MsgID: 0x%04X | TypeIdx: %u\n",
                   i, entries[i].msg_id, entries[i].type_idx);
        }
    }
    else
    {
        printf("REQUEST TABLE: Empty or NULL\n");
    }

    printf("-------------------------------------\n");

    // Друкуємо таблицю відповідей (RESPONSES)
    if (svc->resp_table_ptr && svc->msg_table_size > 0)
    {
        printf("RESPONSE TABLE (at %p):\n", svc->resp_table_ptr);
        qmi_idl_msg_table_entry_t *entries = (qmi_idl_msg_table_entry_t *)svc->resp_table_ptr;

        for (uint32_t i = 0; i < svc->msg_table_size; i++)
        {
            printf("  [%d] MsgID: 0x%04X | TypeIdx: %u\n",
                   i, entries[i].msg_id, entries[i].type_idx);
        }
    }

    // Додатково можна глянути на Common Types, якщо розмір дозволяє
    if (svc->idl_base_ptr)
    {
        printf("-------------------------------------\n");
        printf("IDL BASE METADATA (at %p): First 16 bytes dump:\n", svc->idl_base_ptr);
        uint32_t *raw_meta = (uint32_t *)svc->idl_base_ptr;
        printf("  0x%08X 0x%08X 0x%08X 0x%08X\n",
               raw_meta[0], raw_meta[1], raw_meta[2], raw_meta[3]);
    }
    printf("=====================================\n");
}

/**
 * Універсальний Hex-дамп (якщо потрібно перевірити сирі дані поруч)
 */
void hex_dump_raw(const char *desc, const void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *)addr;

    if (desc != NULL)
        printf("%s:\n", desc);

    for (i = 0; i < len; i++)
    {
        if ((i % 16) == 0)
        {
            if (i != 0)
                printf("  %s\n", buff);
            printf("  %04x ", i);
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0)
    {
        printf("   ");
        i++;
    }
    printf("  %s\n", buff);
}

// Функція пошуку патерну в пам'яті
uint8_t *find_pattern(uint8_t *start, size_t length, uint8_t *pattern, size_t pat_len)
{
    for (size_t i = 0; i < length - pat_len; i++)
    {
        if (memcmp(start + i, pattern, pat_len) == 0)
        {
            return start + i;
        }
    }
    return NULL;
}

uint32_t extract_qmi_handle(uint8_t *match_addr)
{
    // Адреса інструкції "add r2, pc" (0x13d5a)
    uintptr_t add_pc_addr = (uintptr_t)match_addr + 16;

    // Розрахунок PC (адреса + 4 для Thumb-2)
    uintptr_t pc_val = add_pc_addr + 4;

    // Обчислення бази r2.
    // r2 = PC + офсет (0xD4B2).
    // Цей офсет веде в секцію .data або .got
    uintptr_t r2_base = pc_val + 0xD4B2;

    // ldr r0, [r2, #4]
    // Це адреса конкретної комірки пам'яті (0x21214)
    uintptr_t final_ptr_address = r2_base + 4;

#ifdef DEBUG
    printf("[DEBUG] Match at: %p\n", match_addr);
    printf("[DEBUG] Variable location in memory: %p\n", (void *)final_ptr_address);
#endif

    // Читаємо 4 байти як число (uint32_t)
    uint32_t qmi_handle = *(uint32_t *)final_ptr_address;

    return qmi_handle;
}

uint32_t scan_library_for_qmi()
{
    // Перевірка вже існуючого дескриптора
    if (g_qmi_handle || !rimnv_set_nv_overwriteFlag || !rimnv_init_qmi_client_shim)
    {
        return g_qmi_handle;
    }
    uint32_t res = 0;

    // Ініціалізуємо qmi_handle
    if (1 == rimnv_init_qmi_client_shim())
    {

        uint8_t *scan_start = (uint8_t *)((uintptr_t)rimnv_set_nv_overwriteFlag);
        size_t scan_range = 0x200; // 128KB діапазон

        uint8_t pattern[] = {0x41, 0xf2, 0x70, 0x72, 0x02, 0x92, 0x06, 0xab,
                             0x55, 0x4a, 0x00, 0x93, 0x01, 0x23, 0x01, 0x93,
                             0x7a, 0x44, 0x50, 0x68, 0x12, 0x21, 0x05, 0xaa,
                             0xfe, 0xf7, 0x7a, 0xe9};

        uint8_t *match = find_pattern(scan_start, scan_range, pattern, sizeof(pattern));

        if (match)
        {
#ifdef DEBUG
            printf("[+] Found ANCHOR at: %p\n", match);
#endif
            res = extract_qmi_handle(match);
        }
        else
        {
#ifdef DEBUG
            printf("[-] Pattern not found\n");
#endif
        }
    }
    return res;
}

static uint8_t svc_info[20];

uint32_t get_qmi_client_handle(void)
{
    pthread_mutex_lock(&g_nv_mutex);

    // Перевірка вже існуючого дескриптора
    if (g_qmi_handle != 0)
    {
        pthread_mutex_unlock(&g_nv_mutex);
        return g_qmi_handle;
    }

    // Завантаження символів (якщо ще не зроблено)
    if (!p_get_service_object && init_external_symbols() != 0)
    {
        pthread_mutex_unlock(&g_nv_mutex);
        return 0;
    }

    // Отримання сервісного об'єкта (SVC ID 5 для RIM)
    qmi_idl_service_object_t *svc_obj = p_get_service_object(1, 2, RIM_SVC_ID);
    if (!svc_obj)
    {
#ifdef DEBUG
        fprintf(stderr, "ERR: Could not get service object for ID %d\n", RIM_SVC_ID);
#endif
        goto err;
    }
#ifdef DEBUG
    // debug_print_qmi_svc_obj(svc_obj);
    // debug_print_qmi_tables(svc_obj);
#endif

    memset(svc_info, 0, sizeof(svc_info));
    uint32_t n_svc = 1, n_ent = 1;

    // Отримання інформації про сервіс
    int rc = p_get_service_list(svc_obj, svc_info, &n_ent, &n_svc);
    if (rc == 0)
    {
        // Ініціалізація клієнта
        // NULL, NULL, NULL означає синхронний режим (без callback-ів на індикації)
        rc = p_qmi_init(svc_info, svc_obj, NULL, NULL, NULL, &g_qmi_handle);
        if (rc == 0)
        {
            pthread_mutex_unlock(&g_nv_mutex);
            return g_qmi_handle;
        }
#ifdef DEBUG
        printf("n_ent=%d, n_svc=%d\n", n_ent, n_svc);
        hex_dump_raw("SVC_INFO dump: ", svc_info, sizeof(svc_info));
        fprintf(stderr, "ERR: qmi_client_init failed with rc=%d\n", rc);
#endif
        g_qmi_handle = scan_library_for_qmi();
        pthread_mutex_unlock(&g_nv_mutex);
        return g_qmi_handle;
    }
    else
    {
#ifdef DEBUG
        fprintf(stderr, "ERR: qmi_client_get_service_list failed with rc=%d\n", rc);
#endif
    }

err:
    pthread_mutex_unlock(&g_nv_mutex);
    return 0;
}

void release_qmi_client(void)
{
    pthread_mutex_lock(&g_nv_mutex);
    if (g_qmi_handle && p_qmi_release)
    {
        p_qmi_release(g_qmi_handle);
        g_qmi_handle = 0;
    }
    pthread_mutex_unlock(&g_nv_mutex);
}

/**
 * @brief Читання класичного NV Item за його ID.
 * @param item_id ID елемента (наприклад, 550 для IMEI).
 * @param out_data Буфер для запису результату (мінімум 128 байт).
 * @param read_len Вказівник, куди буде записано фактичну довжину даних.
 * @return 0 при успіху, інакше код помилки.
 */
int nv_read_item(uint32_t item_id, uint8_t *out_data, uint32_t *read_len)
{
    int h = get_qmi_client_handle();
    if (!h)
        return -1;

    // Підправлена структура відповіді:
    // В QMI статус (result/error) ЗАВЖДИ йде на початку (offset 0)
    struct __attribute__((packed))
    {
        uint16_t item_id;
        uint16_t result_code; // offset 0: 0 - успіх
        uint32_t data_len;    // offset 4: реальна довжина даних
        uint8_t payload[132]; // offset 8: самі дані
    } resp;

    // Підправлена структура запиту:
    // Асемблер показав, що модем чекає пакет розміром 140 байт (0x8C)
    struct __attribute__((packed))
    {
        uint32_t item_id;
    } req;

    // Спробуйте такий варіант заповнення:
    memset(&req, 0, sizeof(req));
    req.item_id = item_id;

    memset(&resp, 0, sizeof(resp));

    // Важливо: передаємо sizeof(req), тобто 140 байт
    int rc = p_qmi_send_sync(h, MSG_ID_READ_NV_ITEM, &req, sizeof(req), &resp, sizeof(resp), QMI_TIMEOUT_MS);
    // #ifdef DEBUG
    //     hex_dump_raw("MSG_ID_READ_NV_ITEM resp dump: ", &resp, sizeof(resp));
    // #endif
    if (rc == 0 && resp.result_code == 0 && resp.item_id == item_id)
    {
        uint32_t actual_len = (resp.data_len > 128) ? 128 : resp.data_len;
        if (out_data && read_len)
        {
            memcpy(out_data, resp.payload, actual_len);
            *read_len = actual_len;
        }
        return 0;
    }

    return (rc != 0) ? rc : (int)resp.result_code;
}

/**
 * Запис класичного NV-ітема (ID 2).
 * @param item_id ID ітема (наприклад, 550 для IMEI).
 * @param data Вказівник на буфер з даними.
 * @param data_len Довжина даних (макс. 128 байт).
 * @return 1 при успіху, 0 при помилці.
 */
int nv_write_item(uint32_t item_id, const uint8_t *data, uint32_t data_len)
{
    if (data_len > 128)
        return 0; // Обмеження заліза/драйвера

    int h = get_qmi_client_handle();
    if (!h)
        return -1;

    if (!h || !p_qmi_send_sync)
        return 0;

    struct __attribute__((packed))
    {
        uint32_t id;          // local_ac[0]
        uint32_t len;         // local_a8
        uint8_t payload[128]; // auStack_a4
    } req;

    memset(&req, 0, sizeof(req));
    req.id = item_id;
    req.len = data_len;
    memcpy(req.payload, data, data_len);

    uint32_t resp_code = 0; // local_b0

    // Надсилаємо MSG_ID 2, таймаут 5 секунд
    int rc = p_qmi_send_sync(h, MSG_ID_WRITE_NV_ITEM, &req, sizeof(req), &resp_code, 4, 5000);

    if (rc == 0)
    {
        // Витягуємо код помилки модему (верхні 16 біт)
        uint16_t nv_err = (uint16_t)(resp_code >> 16);
        if (nv_err == 0)
        {
            return 1; // Успіх
        }
    }

    return 0;
}

/**
 * Секція 3: EFS (Path-based)
 */
uint32_t efs_get_file_size(const char *path)
{
    int h = get_qmi_client_handle();
    if (!h || !path)
        return 0;

    struct __attribute__((packed))
    {
        uint32_t p_len;
        char path[128];
    } req;
    struct __attribute__((packed))
    {
        uint32_t res;
        uint32_t size;
    } resp;

    memset(&req, 0, sizeof(req));
    req.p_len = strlen(path);
    strncpy(req.path, path, 127);

    if (p_qmi_send_sync(h, MSG_ID_GET_FILE_SIZE, &req, sizeof(req), &resp, sizeof(resp), QMI_TIMEOUT_MS) == 0)
    {
#ifdef DEBUG
        hex_dump_raw("MSG_ID_GET_FILE_SIZE resp OK: ", &resp, sizeof(resp));
#endif
        return (resp.res == 0) ? resp.size : 0;
    }
    // #ifdef DEBUG
    //     hex_dump_raw("MSG_ID_GET_FILE_SIZE resp ERR: ", &resp, sizeof(resp));
    // #endif
    return 0;
}

int efs_read_file(const char *path, uint8_t *out_buf, size_t *size)
{
    int h = get_qmi_client_handle();
    if (!h)
        return -1;

    // Фіксований розмір як в асемблері
    struct __attribute__((packed))
    {
        uint32_t path_len;      // 0x00
        char path[128];         // 0x04
        uint16_t chunk_size;    // 0x84 (0x0800)
        uint8_t flag1;          // 0x86 (1)
        uint8_t flag2;          // 0x87 (0xf0)
        uint8_t is_active;      // 0x88 (resp[0x806])
        uint8_t chunk_index;    // 0x89 (counter)
        uint8_t flag3;          // 0x8a (1)
        uint8_t reserved;       // 0x8b
        uint16_t total_req_len; // 0x8c
        uint16_t padding;       // 0x8e
    } req;

    struct __attribute__((packed))
    {
        uint32_t chunk_len;  // Offset 0
        uint8_t data[0x800]; // Offset 4...2051
        uint8_t padding[2];  // До розміру 0x80c (якщо треба)
        int16_t status;      // Offset 2052 (твої 00 00)
        uint8_t has_more;    // Offset 2054 (твій 01 - продовжувати)
        uint8_t next_index;  // Offset 2055 (твій 01 - наступний чанк)
        uint16_t total_size; // Offset 2056
    } resp;

    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    size_t path_len = strlen(path);
    if (path_len > 128 || *size > 0x800)
        return -2;

    req.path_len = strlen(path);
    strncpy(req.path, path, 127);
    req.chunk_size = *size;

    int rc = p_qmi_send_sync(h, MSG_ID_READ_EFS, &req, sizeof(req), &resp, sizeof(resp), QMI_TIMEOUT_MS);
#ifdef DEBUG
    // printf("efs_read_file size=%d  rc=%d status=%d \n", resp.chunk_len, rc, resp.status);
    // hex_dump_raw("MSG_ID_READ_EFS resp : ", &resp, sizeof(resp));
#endif
    if (rc == 0 && resp.status == 0)
    {
        uint32_t actual_len = resp.chunk_len; // Offset 0 відповіді
        if (actual_len > *size)
            actual_len = *size;

        memcpy(out_buf, resp.data, actual_len); // Дані з Offset 4
        *size = actual_len;
        return 0;
    }

    return (rc != 0) ? rc : -1;
}

int efs_read_large_file(const char *path, uint8_t *out_buf, size_t *size)
{
    int h = get_qmi_client_handle();
    if (!h || !path || !out_buf || !size)
        return -1;

    // СТРУКТУРА ЗАПИТУ (0x90 / 144 байт)
    struct __attribute__((packed))
    {
        uint32_t path_len;      // 0x00
        char path[128];         // 0x04
        uint16_t chunk_size;    // 0x84 (0x0800)
        uint8_t flag1;          // 0x86 (1)
        uint8_t flag2;          // 0x87 (0xf0)
        uint8_t is_active;      // 0x88 (resp[0x806])
        uint8_t chunk_index;    // 0x89 (counter)
        uint8_t flag3;          // 0x8a (1)
        uint8_t reserved;       // 0x8b
        uint16_t total_req_len; // 0x8c
        uint16_t padding;       // 0x8e
    } req;

    // СТРУКТУРА ВІДПОВІДІ (0x80c / 2060 байт)
    struct __attribute__((packed))
    {
        uint32_t chunk_len;  // Offset 0
        uint8_t data[2048];  // Offset 4...2051
        int16_t status;      // Offset 2052 (твої 00 00)
        uint8_t has_more;    // Offset 2054 (твій 01 - продовжувати)
        uint8_t next_index;  // Offset 2055 (твій 01 - наступний чанк)
        uint8_t padding[2];  // До розміру 0x80c (якщо треба)
        uint16_t total_size; // Offset 2056
    } resp;

    memset(&req, 0, sizeof(req));
    req.path_len = strlen(path);
    strncpy(req.path, path, 127);
    req.chunk_size = CHUNK_SIZE;
    req.flag1 = 1;
    req.flag2 = 0xf0;
    req.flag3 = 1;
    req.is_active = 1;
    req.total_req_len = (uint16_t)*size;

    pthread_mutex_lock(&g_nv_mutex);

    uint8_t curr_idx = 0;
    int success = 0;

    // Початковий стан для входу в цикл
    resp.has_more = 1;

    while (resp.has_more == 1)
    {
        req.is_active = resp.has_more;
        req.chunk_index = curr_idx;

#ifdef DEBUG
        printf("chunk_size: 0x%x, flag1: %d, flag2: %d, flag3: %d, active: %d, total_len: 0x%x \n",
               req.chunk_size, req.flag1, req.flag2, req.flag3, req.is_active, req.total_req_len);
        // hex_dump_raw("MSG_ID_READ_EFS req : ", &req, sizeof(req));
#endif

        // Message ID 3 згідно з декомпілятом
        int rc = p_qmi_send_sync(h, MSG_ID_READ_EFS, &req, sizeof(req), &resp, sizeof(resp), QMI_TIMEOUT_MS);
#ifdef DEBUG
        printf("rc: %d\n", rc);
        printf("chunk_len: 0x%x more: %d  index: %d, total_size: 0x%x \n", resp.chunk_len, resp.has_more, resp.next_index, resp.total_size);
        // hex_dump_raw("MSG_ID_READ_EFS resp : ", &resp, sizeof(resp));
#endif
        if (rc != 0 || resp.status != 0)
        {
            success = -1;
            break;
        }

        // Перевірка на переповнення буфера
        if (resp.chunk_len > CHUNK_SIZE)
            break;

        memcpy(out_buf + (curr_idx * CHUNK_SIZE), resp.data, resp.chunk_len);

        *size = resp.total_size;

        curr_idx++;
        if (resp.has_more != 0 && resp.next_index != curr_idx)
        {
            success = -1;
            break;
        }

        if (resp.has_more == 0)
        {
            success = 0;
            break;
        }

        uint32_t remined = *size - (curr_idx * CHUNK_SIZE);
        req.chunk_size = (remined > CHUNK_SIZE) ? CHUNK_SIZE : remined;
    }

    pthread_mutex_unlock(&g_nv_mutex);
    return success;
}

int efs_write_file(const char *path, const uint8_t *data, uint32_t len)
{
    int h = get_qmi_client_handle();
    if (!h || !path)
        return -1;

    struct __attribute__((packed))
    {
        uint32_t p_len;
        char path[128];
        uint32_t c_len;
        uint8_t chunk[CHUNK_SIZE];
        uint32_t dummy;
        uint32_t total;
    } req;

    int16_t res_code = -1;

    memset(&req, 0, sizeof(req));
    req.p_len = strlen(path);
    strncpy(req.path, path, 127);
    req.total = (uint16_t)len;

    req.c_len = len;
    memcpy(req.chunk, data, len);
#ifdef DEBUG
    // printf("efs_write_file %d\n", sizeof(req));
#endif

    if (p_qmi_send_sync(h, MSG_ID_WRITE_EFS, &req, sizeof(req), &res_code, sizeof(res_code), QMI_TIMEOUT_MS) == 0)
        return (int)res_code;
    else
        return -1;
}

int efs_write_large_file(const char *path, const uint8_t *data, uint32_t len)
{
    int h = get_qmi_client_handle();
    if (!h || !path)
        return -1;

    struct __attribute__((packed))
    {
        uint32_t p_len;
        char path[128];
        uint32_t c_len;
        uint8_t chunk[CHUNK_SIZE];
        uint8_t is_large, seq, flag1, idx, flag2, dummy;
        uint16_t total;
    } req;

    int16_t res_code = -1;
    uint32_t num_chunks = (len + CHUNK_SIZE - 1) / CHUNK_SIZE;
    memset(&req, 0, sizeof(req));
    req.is_large = 1;
    req.seq = 0xF1;
    req.flag1 = 1;
    req.flag2 = 1;
    req.p_len = strlen(path);
    strncpy(req.path, path, 127);

    pthread_mutex_lock(&g_nv_mutex);
    for (uint8_t i = 0; i < num_chunks; i++)
    {
        req.total = (uint16_t)len;
        req.idx = i;

        uint32_t c_len = (i == num_chunks - 1) ? (len - (i * CHUNK_SIZE)) : CHUNK_SIZE;
        req.c_len = c_len;
        memcpy(req.chunk, data + (i * CHUNK_SIZE), c_len);

        if (p_qmi_send_sync(h, MSG_ID_WRITE_EFS, &req, sizeof(req), &res_code, 2, QMI_TIMEOUT_MS) != 0)
            break;
    }
    pthread_mutex_unlock(&g_nv_mutex);
    return (int)res_code;
}

/**
 * Секція 4: IMS Параметри
 */
int ims_get_parm(int id, char *val_str)
{
    int16_t parm_id = (int16_t)id; // 16-bit on the wire
    int h = get_qmi_client_handle();
    if (!h || !val_str)
        return -1;

    struct __attribute__((packed))
    {
        uint32_t len;
        char val[128];
        int8_t type;
        uint8_t dummy[3];
    } res;

    memset(&res, 0, sizeof(res));

    printf("ims id: %d\n", parm_id);
    int rc = p_qmi_send_sync(h, MSG_ID_GET_IMS_PARM, &parm_id, sizeof(parm_id), &res, sizeof(res), QMI_TIMEOUT_MS);
    printf("rc: %d\n", rc);
    hex_dump_raw("SG_ID_GET_IMS_PARM resp", &res, sizeof(res));
    strncpy(val_str, res.val, 127);
    return rc;
}

int ims_set_parm(int parm_id, const char *val_str)
{
    int h = get_qmi_client_handle();
    if (!h || !val_str)
        return -1;

    struct __attribute__((packed))
    {
        uint16_t id;
        uint16_t rsvd;
        uint32_t len;
        char val[128];
    } req;

    uint8_t res = 0;
    memset(&req, 0, sizeof(req));
    req.id = (uint16_t)parm_id;
    req.len = strlen(val_str);
    strncpy(req.val, val_str, 127);

    int rc = p_qmi_send_sync(h, MSG_ID_SET_IMS_PARM, &req, sizeof(req), &res, 1, QMI_TIMEOUT_MS);
    return !rc ? res : rc;
}

/**
 * Примусова синхронізація EFS (Flush cache to Flash)
 * Рекомендується викликати після efs_write_large_file або nv_write_item
 */
int efs_force_sync(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return -1;

    uint8_t sync_flag = 0; // "FORCE SYNC ALL"
    uint32_t sync_stat = 0;

    // Надсилаємо MSG_ID 15 (0x0F)
    int rc = p_qmi_send_sync(h, MSG_ID_EFS_FORCE_SYNC,
                             &sync_flag, 1,
                             &sync_stat, 4,
                             QMI_TIMEOUT_MS);

    if (rc == 0)
    {
        // Якщо QMI успішний, перевіряємо внутрішній статус EFS
        return (sync_stat == 0) ? 0 : (int)sync_stat;
    }

    return rc; // Помилка QMI
}
/**
 * Реєстрація гістерезису SID (System ID Hysteresis)
 * Допомагає стабілізувати з'єднання при слабкому сигналі.
 */
int sid_hysteresis_register(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return -1;

    struct __attribute__((packed))
    {
        uint32_t enable;
        uint32_t flag;
    } req;

    int32_t resp_status = 0;

    req.enable = 1;
    req.flag = 1;

    // Надсилаємо MSG_ID 7, таймаут великий — 20000 мс (як у декомпільованому коді)
    int rc = p_qmi_send_sync(h, MSG_ID_SID_HYSTERESIS_REG,
                             &req, sizeof(req),
                             &resp_status, 4,
                             20000);

    if (rc == 0)
    {
        return (resp_status == 0) ? 0 : (int)resp_status;
    }

    return rc;
}

/**
 * Отримання причини розриву фізичного каналу.
 * @return Код причини (cause code) або -1 при помилці QMI.
 */
int get_phy_link_down_cause(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return -1;

    uint8_t dummy_req = 0;
    uint32_t cause_code = 0;

    // Надсилаємо MSG_ID 17 (0x11)
    int rc = p_qmi_send_sync(h, MSG_ID_GET_PHY_LINK_DOWN_CAUSE,
                             &dummy_req, 1,
                             &cause_code, 4,
                             20000);

    if (rc == 0)
    {
        return (int)cause_code;
    }

    return -1; // Помилка зв'язку з модемом
}

/**
 * Встановлення режиму LTE (Enable/Disable).
 * @param mode 1 - увімкнути LTE, 0 - вимкнути.
 * @return 1 при успіху, 0 при помилці.
 */
int set_lte_mode(uint8_t mode)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint8_t req_data = mode;
    uint32_t resp_status = 0;

    // Надсилаємо MSG_ID 19 (0x13)
    int rc = p_qmi_send_sync(h, MSG_ID_SET_LTE_MODE,
                             &req_data, 1,
                             &resp_status, 4,
                             20000);

    if (rc == 0 && resp_status == 0)
    {
        return 1; // Успішно встановлено
    }

    return 0; // Помилка
}

/**
 * Отримання параметрів LTE Attach.
 * @param out_params Буфер розміром 136 байт для запису параметрів.
 * @return 1 при успіху, 0 при помилці.
 */
int get_lte_attach_params(uint8_t *out_params)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !out_params)
        return 0;

    uint8_t dummy_req = 0;
    uint8_t resp_buf[136];
    memset(resp_buf, 0, sizeof(resp_buf));

    // Надсилаємо MSG_ID 20 (0x14)
    int rc = p_qmi_send_sync(h, MSG_ID_GET_LTE_ATTACH_PARAMS,
                             &dummy_req, 1,
                             resp_buf, 136,
                             20000);

    if (rc == 0)
    {
        memcpy(out_params, resp_buf, 136);
        return 1;
    }

    return 0;
}

/**
 * Встановлення прапорця Handover для ePDG (VoWiFi).
 * @param in_params Вказівник на структуру параметрів (136 байт).
 * @return 1 при успіху, 0 при помилці.
 */
int epdg_set_handover_flag(const uint8_t *in_params)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !in_params)
        return 0;

    uint8_t req_buf[136];
    uint32_t resp_status = 0;

    // Копіюємо вхідні дані в локальний буфер
    memcpy(req_buf, in_params, 136);

    // Надсилаємо MSG_ID 28 (0x1C)
    int rc = p_qmi_send_sync(h, MSG_ID_EPDG_SET_HANDOVER_FLAG,
                             req_buf, 136,
                             &resp_status, 4,
                             20000);

    // Логіка успіху: rc == 0 (QMI OK) та resp_status == 0 (EFS/NV OK)
    if (rc == 0 && resp_status == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Отримання IPv6 адреси інтерфейсу.
 * @param interface_id ID інтерфейсу (param_1 у декомпіляті).
 * @param out_ipv6 Вказівник на буфер (мінімум 16 байт) для запису IPv6 адреси.
 * @return 1 при успіху, 0 при помилці.
 */
int get_mp_ipv6_addr(uint32_t interface_id, uint8_t *out_ipv6)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !out_ipv6)
        return 0;

    uint32_t req_id = interface_id;

    // Структура відповіді (0x18 = 24 байти)
    struct __attribute__((packed))
    {
        uint32_t resp_code;  // local_40[0]
        uint32_t reserved;   // local_40[1]
        uint32_t addr_part1; // local_38
        uint32_t addr_part2; // uStack_34
        uint32_t addr_part3; // local_30
        uint32_t addr_part4; // uStack_2c
    } resp;

    memset(&resp, 0, sizeof(resp));

    // Надсилаємо MSG_ID 29 (0x1D)
    int rc = p_qmi_send_sync(h, MSG_ID_GET_MP_IPV6_ADDR,
                             &req_id, 4,
                             &resp, sizeof(resp),
                             20000);

    // Логіка успіху з декомпіляту: 1 - resp_code
    if (rc == 0 && resp.resp_code == 0)
    {
        memcpy(out_ipv6, &resp.addr_part1, 16);
        return 1;
    }

    return 0;
}

/**
 * Встановлення списку пріоритетів PDN для LTE Attach.
 * @param count Кількість профілів у списку.
 * @param profile_ids Масив ID профілів (по 2 байти на кожен).
 * @return 0 при успіху, інакше код помилки.
 */
int set_lte_pdn_priority(uint8_t count, const uint16_t *profile_ids)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !profile_ids)
        return -1;

    struct __attribute__((packed))
    {
        uint8_t count;         // Кількість записів
        uint8_t reserved;      // Падінг/резерв
        uint16_t profiles[56]; // Список ID (всього до 0x72 байт)
    } req;

    memset(&req, 0, sizeof(req));
    req.count = count;

    // Обмежуємо копіювання, щоб не вийти за межі структури (0x72 байт разом)
    uint32_t to_copy = (count > 56) ? 56 : count;
    memcpy(req.profiles, profile_ids, to_copy * sizeof(uint16_t));

    int32_t resp_err = 0;

    // Надсилаємо MSG_ID 30 (0x1E)
    int rc = p_qmi_send_sync(h, MSG_ID_SET_LTE_PDN_PRIORITY,
                             &req, 0x72,
                             &resp_err, 4,
                             20000);

    if (rc == 0)
    {
        return (int)resp_err;
    }

    return rc;
}

/**
 * Запуск екстреної сесії GPS (E911).
 * Активує режим пріоритетного визначення координат.
 * @return 1 при успіху, 0 при помилці.
 */
int gps_e911_start_session(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint16_t req_data = 0;   // local_18[0]
    int32_t resp_status = 0; // local_1c

    // Надсилаємо MSG_ID 24 (0x18)
    int rc = p_qmi_send_sync(h, MSG_ID_GPS_E911_START_EMERG,
                             &req_data, 2,
                             &resp_status, 4,
                             5000);

    // Логіка з декомпіляту: успіх, якщо rc == 0 ТА resp_status == 0
    if (rc == 0 && resp_status == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Реєстрація на отримання CDMA OTA індикацій (Lock orders тощо).
 * @return 1 при успіху, 0 при помилці.
 */
int cdma_ota_ind_reg(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint32_t enable = 1; // local_18

    struct __attribute__((packed))
    {
        uint32_t qmi_err; // local_20
        uint32_t status;  // local_1c
    } resp;

    memset(&resp, 0, sizeof(resp));

    // Надсилаємо MSG_ID 13 (0x0D) з таймаутом 20 секунд
    int rc = p_qmi_send_sync(h, MSG_ID_CDMA_OTA_IND_REG,
                             &enable, 4,
                             &resp, sizeof(resp),
                             20000);

    // Успіх, якщо rc == 0 (QMI OK) та внутрішній статус успішний
    if (rc == 0 && resp.status == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Встановлення стану NAS E911 (Emergency Mode).
 * @param state 1 - увімкнути режим екстреного виклику, 0 - вимкнути.
 * @return 1 при успіху, 0 при помилці.
 */
int nas_set_e911_state(uint32_t state)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint32_t req_state = state; // param_1
    int32_t resp_status = 0;    // local_18

    // Надсилаємо MSG_ID 33 (0x21)
    int rc = p_qmi_send_sync(h, MSG_ID_NAS_SET_E911_STATE,
                             &req_state, 4,
                             &resp_status, 4,
                             2000);

    // Логіка успіху: rc == 0 ТА resp_status == 0
    if (rc == 0 && resp_status == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Отримання статусу обмеження доступу (SSAC Barring) для LTE.
 * @param service_type Тип сервісу для перевірки (наприклад, VoLTE).
 * @param out_barring_status Вказівник для запису статусу (16 біт).
 * @return 1 при успіху, 0 при помилці.
 */
int get_lte_ssac_barring_info(uint32_t service_type, uint16_t *out_barring_status)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !out_barring_status)
        return 0;

    uint32_t req_service = service_type;

    struct __attribute__((packed))
    {
        int32_t qmi_status;  // local_28 (результат операції QMI)
        uint32_t bar_status; // local_24 (фактичний статус баррингу)
    } resp;

    memset(&resp, 0, sizeof(resp));

    // Надсилаємо MSG_ID 35 (0x23), таймаут 2 секунди
    int rc = p_qmi_send_sync(h, MSG_ID_GET_LTE_SSAC_BARRING,
                             &req_service, 4,
                             &resp, sizeof(resp),
                             2000);

    // Успіх, якщо rc == 0 (зв'язок OK) та qmi_status == 0 (модем OK)
    if (rc == 0 && resp.qmi_status == 0)
    {
        *out_barring_status = (uint16_t)(resp.bar_status & 0xFFFF);
        return 1;
    }

    return 0;
}

/**
 * Інкрементація ID повідомлення для SMS over IMS.
 * @param format_3gpp 1 для 3GPP (LTE), 0 для 3GPP2 (CDMA).
 * @param out_msg_id Вказівник для запису нового ID (16 біт).
 * @return 1 при успіху, 0 при помилці.
 */
int sms_ims_incr_msg_id(uint8_t format_3gpp, uint16_t *out_msg_id)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !out_msg_id)
        return 0;

    uint8_t req_format = format_3gpp;
    uint16_t new_id = 0;

    // Надсилаємо MSG_ID 25 (0x19)
    int rc = p_qmi_send_sync(h, MSG_ID_SMS_IMS_INCR_MSG_ID,
                             &req_format, 1,
                             &new_id, 2,
                             5000);

    if (rc == 0)
    {
        *out_msg_id = new_id;
        return 1;
    }

    return 0;
}

/**
 * Отримання поточного ID повідомлення для SMS over IMS.
 * @param format_3gpp 1 для 3GPP, 0 для 3GPP2.
 * @param out_msg_id Вказівник для запису поточного ID (16 біт).
 * @return 1 при успіху, 0 при помилці.
 */
int sms_ims_get_msg_id(uint8_t format_3gpp, uint16_t *out_msg_id)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync || !out_msg_id)
        return 0;

    uint8_t req_format = format_3gpp;
    uint16_t current_id = 0;

    // Надсилаємо MSG_ID 26 (0x1A)
    int rc = p_qmi_send_sync(h, MSG_ID_SMS_IMS_GET_MSG_ID,
                             &req_format, 1,
                             &current_id, 2,
                             5000);

    if (rc == 0)
    {
        *out_msg_id = current_id;
        return 1;
    }

    return 0;
}

/**
 * Встановлення стану CDMA підписки на UICC.
 * @param state 0 - NV Mode (читати з пам'яті), 1 - RUIM Mode (читати з карти).
 * @return 1 при успіху, 0 при помилці.
 */
int uicc_set_cdma_subscription(uint8_t state)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint8_t req_state = state;
    int32_t resp_result = 0;

    // Надсилаємо MSG_ID 21 (0x15), таймаут 5 секунд
    int rc = p_qmi_send_sync(h, MSG_ID_UICC_SET_CDMA_SUBSCRIPTION,
                             &req_state, 1,
                             &resp_result, 4,
                             5000);

    // Успіх, якщо rc == 0 ТА модем повернув resp_result == 0
    if (rc == 0 && resp_result == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Реєстрація на отримання індикацій про зміну стану RRC (Idle/Connected).
 * @return 1 при успіху, 0 при помилці.
 */
int rrc_state_ind_reg(void)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint32_t enable = 1; // Активація сповіщень

    struct __attribute__((packed))
    {
        uint32_t qmi_err; // Результат виконання QMI (local_20)
        uint32_t status;  // Внутрішній статус реєстрації (local_1c)
    } resp;

    memset(&resp, 0, sizeof(resp));

    // Надсилаємо MSG_ID 11 (0x0B), таймаут 20 секунд
    int rc = p_qmi_send_sync(h, MSG_ID_RRC_STATE_IND_REG,
                             &enable, 4,
                             &resp, sizeof(resp),
                             20000);

    // Успіх, якщо rc == 0 та внутрішній статус успішний (0)
    if (rc == 0 && resp.status == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * Встановлення прапорця дозволу на перезапис NV/EFS.
 * @param enable 1 - дозволити перезапис, 0 - заборонити.
 * @return 1 при успіху, 0 при помилці.
 */
int nv_set_overwrite_flag(uint8_t enable)
{
    int h = get_qmi_client_handle();
    if (!h || !p_qmi_send_sync)
        return 0;

    uint8_t req_data = enable;
    uint8_t resp_data = 0;

    // Надсилаємо MSG_ID 18 (0x12), таймаут 5 секунд
    int rc = p_qmi_send_sync(h, MSG_ID_SET_OVERWRITE,
                             &req_data, 1,
                             &resp_data, 1,
                             5000);

    // Логіка з декомпіляту: rc == 0 (QMI OK) та resp_data != 0
    if (rc == 0 && resp_data != 0)
    {
        return 1;
    }

    return 0;
}
