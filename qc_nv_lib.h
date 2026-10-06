#ifndef QUIC_NV_MANAGER_H
#define QUIC_NV_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Ініціалізує символи та відкриває сесію з модемом.
     * @return Дескриптор клієнта (handle) або 0 у разі помилки.
     */
    int get_qmi_client_handle(void);

    /**
     * @brief Закриває сесію та звільняє ресурси.
     */
    void release_qmi_client(void);

    /**
     * @brief Читання класичного NV Item за його ID.
     * @param item_id ID елемента (наприклад, 550 для IMEI).
     * @param out_data Буфер для запису результату (мінімум 128 байт).
     * @param read_len Вказівник, куди буде записано фактичну довжину даних.
     * @return 0 при успіху, інакше код помилки.
     */
    int nv_read_item(uint16_t item_id, uint8_t *out_data, uint32_t *read_len);

    /**
     * Запис класичного NV-ітема (ID 2).
     * @param item_id ID ітема (наприклад, 550 для IMEI).
     * @param data Вказівник на буфер з даними.
     * @param data_len Довжина даних (макс. 128 байт).
     * @return 1 при успіху, 0 при помилці.
     */
    int nv_write_item(uint16_t item_id, const uint8_t *data, uint32_t data_len);

    /**
     * @brief Читання розміру файлу в EFS модему.
     * @param path Повний шлях до файлу (наприклад, "/nv/item_files/conf/data.xml").
     * @return Розмір файлу в байтах або 0, якщо файл відсутній.
     */
    uint32_t efs_get_file_size(const char *path);

    /**
     * @brief Читання файлу з EFS.
     * @param path Шлях до файлу.
     * @param out_buf Буфер для даних (має бути достатнього розміру).
     * @param size Вхід: бажаний розмір; Вихід: реально прочитано.
     * @return 0 при успіху.
     */
    int efs_read_file(const char *path, uint8_t *out_buf, size_t *size);

    /**
     * @brief Читання великого файлу з EFS частинами (chunks).
     * @param path Шлях до файлу.
     * @param out_buf Буфер для даних (має бути достатнього розміру).
     * @param size Вхід: бажаний розмір; Вихід: реально прочитано.
     * @return 0 при успіху.
     */
    int efs_read_large_file(const char *path, uint8_t *out_buf, size_t *size);

    /**
     * @brief Запис файлу в EFS.
     * @param path Шлях до файлу.
     * @param data Вказівник на дані для запису.
     * @param len Довжина даних.
     * @return 0 при успіху, інакше код помилки.
     */
    int efs_write_file(const char *path, const uint8_t *data, uint32_t len);

    /**
     * @brief Запис файлу в EFS (автоматично розбиває на частини).
     * @param path Шлях до файлу.
     * @param data Вказівник на дані для запису.
     * @param len Довжина даних.
     * @return 0 при успіху, інакше код помилки.
     */
    int efs_write_large_file(const char *path, const uint8_t *data, uint32_t len);

    /**
     * @brief Видалення файлу з файлової системи модему.
     * @param path Шлях до файлу.
     * @return 1 (true) при успіху, 0 при помилці.
     */
    int efs_rm_file(const char *path);

    /**
     * @brief Встановлення прапорця дозволу на перезапис NV.
     * @param enabled 1 — дозволити, 0 — заборонити.
     * @return 0 при успіху.
     */
    int nv_set_overwrite_flag(uint8_t enabled);

    /**
     * @brief Отримання параметрів конфігурації IMS.
     * @param parm_id ID параметра.
     * @param out_resp_struct Вказівник на структуру відповіді.
     * @return 0 при успіху.
     */
    int ims_get_parm(int parm_id, void *out_resp_struct);

    /**
     * @brief Встановлення текстового параметра IMS.
     * @param parm_id ID параметра.
     * @param val_str Рядок із значенням.
     * @return 0 при успіху.
     */
    int ims_set_parm(int parm_id, const char *val_str);

    /**
     * @brief Примусово зберігає всі зміни EFS/NV на флеш-пам'ять модему.
     * @return 0 при успіху, інакше код помилки.
     */
    int efs_force_sync(void);

    /**
     * Реєстрація гістерезису SID (System ID Hysteresis)
     * Допомагає стабілізувати з'єднання при слабкому сигналі.
     */
    int sid_hysteresis_register(void);

    /**
     * Отримання причини розриву фізичного каналу.
     * @return Код причини (cause code) або -1 при помилці QMI.
     */
    int get_phy_link_down_cause(void);

    /**
     * Встановлення режиму LTE (Enable/Disable).
     * @param mode 1 - увімкнути LTE, 0 - вимкнути.
     * @return 1 при успіху, 0 при помилці.
     */
    int set_lte_mode(uint8_t mode);

    /**
     * Отримання параметрів LTE Attach.
     * @param out_params Буфер розміром 136 байт для запису параметрів.
     * @return 1 при успіху, 0 при помилці.
     */
    int get_lte_attach_params(uint8_t *out_params);

    /**
     * Встановлення прапорця Handover для ePDG (VoWiFi).
     * @param in_params Вказівник на структуру параметрів (136 байт).
     * @return 1 при успіху, 0 при помилці.
     */
    int epdg_set_handover_flag(const uint8_t *in_params);

    /**
     * Отримання IPv6 адреси інтерфейсу.
     * @param interface_id ID інтерфейсу (param_1 у декомпіляті).
     * @param out_ipv6 Вказівник на буфер (мінімум 16 байт) для запису IPv6 адреси.
     * @return 1 при успіху, 0 при помилці.
     */
    int get_mp_ipv6_addr(uint32_t interface_id, uint8_t *out_ipv6);

    /**
     * Встановлення списку пріоритетів PDN для LTE Attach.
     * @param count Кількість профілів у списку.
     * @param profile_ids Масив ID профілів (по 2 байти на кожен).
     * @return 0 при успіху, інакше код помилки.
     */
    int set_lte_pdn_priority(uint8_t count, const uint16_t *profile_ids);

    /**
     * Запуск екстреної сесії GPS (E911).
     * Активує режим пріоритетного визначення координат.
     * @return 1 при успіху, 0 при помилці.
     */
    int gps_e911_start_session(void);

    /**
     * Реєстрація на отримання CDMA OTA індикацій (Lock orders тощо).
     * @return 1 при успіху, 0 при помилці.
     */
    int cdma_ota_ind_reg(void);

    /**
     * Встановлення стану NAS E911 (Emergency Mode).
     * @param state 1 - увімкнути режим екстреного виклику, 0 - вимкнути.
     * @return 1 при успіху, 0 при помилці.
     */
    int nas_set_e911_state(uint32_t state);

    /**
     * Отримання статусу обмеження доступу (SSAC Barring) для LTE.
     * @param service_type Тип сервісу для перевірки (наприклад, VoLTE).
     * @param out_barring_status Вказівник для запису статусу (16 біт).
     * @return 1 при успіху, 0 при помилці.
     */
    int get_lte_ssac_barring_info(uint32_t service_type, uint16_t *out_barring_status);

    /**
     * Інкрементація ID повідомлення для SMS over IMS.
     * @param format_3gpp 1 для 3GPP (LTE), 0 для 3GPP2 (CDMA).
     * @param out_msg_id Вказівник для запису нового ID (16 біт).
     * @return 1 при успіху, 0 при помилці.
     */
    int sms_ims_incr_msg_id(uint8_t format_3gpp, uint16_t *out_msg_id);

    /**
     * Отримання поточного ID повідомлення для SMS over IMS.
     * @param format_3gpp 1 для 3GPP, 0 для 3GPP2.
     * @param out_msg_id Вказівник для запису поточного ID (16 біт).
     * @return 1 при успіху, 0 при помилці.
     */
    int sms_ims_get_msg_id(uint8_t format_3gpp, uint16_t *out_msg_id);

    /**
     * Встановлення стану CDMA підписки на UICC.
     * @param state 0 - NV Mode (читати з пам'яті), 1 - RUIM Mode (читати з карти).
     * @return 1 при успіху, 0 при помилці.
     */
    int uicc_set_cdma_subscription(uint8_t state);

    /**
     * Реєстрація на отримання індикацій про зміну стану RRC (Idle/Connected).
     * @return 1 при успіху, 0 при помилці.
     */
    int rrc_state_ind_reg(void);

#ifdef __cplusplus
}
#endif

#endif // QUIC_NV_MANAGER_H