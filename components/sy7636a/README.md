# sy7636a

Trình điều khiển C cho PMIC màn hình mực điện tử **SY7636A** (Silergy) trên ESP-IDF. Hỗ trợ VCOM, nguồn, giải mã lỗi và đo nhiệt độ để chọn waveform.

ESP-IDF C driver for the **SY7636A** e-paper PMIC (Silergy): VCOM setting, rail control, fault decode, and the on-chip temperature sensor the waveform lookup needs.

Tên trong registry là `mindreset/sy7636a`.

## Tính năng

- API dùng handle và bus `i2c_master` có sẵn. EN, VCOM_EN và PGOOD có thể nối GPIO hoặc dùng callback khi nằm trên bộ mở rộng I/O.
- VCOM tính bằng mV. Hỗ trợ điều khiển thủ công hoặc tự động và đọc lại giá trị đã lưu.
- Bật tắt nguồn và đợi PGOOD, chọn VLDO, cấu hình thanh ghi xả và độ trễ cấp điện của từng đường nguồn.
- Giải mã lỗi UVP, SCP theo đường nguồn và OTP thành một enum.
- Đọc nhiệt độ theo °C để chọn dải waveform.

## Phần cứng

| Mục | Thông số |
|---|---|
| Chip | SY7636A (Silergy) |
| Bus | I²C, 400 kHz |
| Địa chỉ | `0x62` (`SY7636A_ADDR_DEFAULT`) |
| PGOOD | Cực máng hở. Read Pico đọc qua chân trên bộ mở rộng I/O. |

## Cách dùng

```c
sy7636a_config_t cfg = SY7636A_CONFIG_DEFAULT();
cfg.en_fn = board_sy_en;        // EN của bo nằm trên FCA9555.
cfg.pgood_fn = board_pgood;

sy7636a_handle_t pmic;
sy7636a_init(bus, &cfg, &pmic);
sy7636a_set_vcom(pmic, 1290);   // -1.29 V
sy7636a_power_on(pmic);

sy7636a_status_t st;
sy7636a_read(pmic, &st);        // vcom_mv / temperature_c / fault / vldo
```

Xem bản đọc thanh ghi đầy đủ tại `examples/read_status`.

See `examples/read_status` for a full dump.

## Giấy phép

Apache-2.0. Xem [LICENSE](LICENSE).

Apache-2.0, see [LICENSE](LICENSE).
